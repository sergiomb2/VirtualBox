/* $Id$ */
/** @file
 * Guest / Host common code - Session type detection + handling.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/string.h>
#include <iprt/ldr.h>
#include <iprt/log.h>

#include <VBox/GuestHost/DisplayServerType.h>


/*********************************************************************************************************************************
*   Implementation                                                                                                               *
*********************************************************************************************************************************/

/**
 * Returns the VBGHDISPLAYSERVERTYPE as a string.
 *
 * @returns VBGHDISPLAYSERVERTYPE as a string.
 * @param   enmType             VBGHDISPLAYSERVERTYPE to return as a string.
 */
const char *VBGHDisplayServerTypeToStr(VBGHDISPLAYSERVERTYPE enmType)
{
    switch (enmType)
    {
        RT_CASE_RET_STR(VBGHDISPLAYSERVERTYPE_NONE);
        RT_CASE_RET_STR(VBGHDISPLAYSERVERTYPE_AUTO);
        RT_CASE_RET_STR(VBGHDISPLAYSERVERTYPE_X11);
        RT_CASE_RET_STR(VBGHDISPLAYSERVERTYPE_WAYLAND);
        RT_CASE_RET_STR(VBGHDISPLAYSERVERTYPE_XWAYLAND);
        default: break;
    }

    AssertFailedReturn("<invalid>");
}

/**
 * Tries to load a (system) library via a set of different names / versions.
 *
 * @returns VBox status code.
 * @returns VERR_NOT_FOUND if the library was not found.
 *
 * @param   apszLibs            Array of library (version) names to search for.
 *                              Descending order (e.g. "libfoo.so", "libfoo.so.2", "libfoo.so.2.6").
 * @param   cLibs               Number of library names in \a apszLibs.
 * @param   phLdrMod            Where to return the library handle on success.
 *
 * @note    Will print loading statuses to verbose release log.
 */
static int vbghDisplayServerTryLoadLib(const char **apszLibs, size_t cLibs, PRTLDRMOD phLdrMod)
{
    for (size_t i = 0; i < cLibs; i++)
    {
        const char *pszLib = apszLibs[i];
        int rc2 = RTLdrLoadSystem(pszLib, /* fNoUnload = */ true, phLdrMod);
        if (RT_SUCCESS(rc2))
        {
            LogRel2(("Loaded display server system library '%s'\n", pszLib));
            return VINF_SUCCESS;
        }
        else
            LogRel2(("Unable to load display server system library '%s': %Rrc\n", pszLib, rc2));
    }

    return VERR_NOT_FOUND;
}

/**
 * Tries to detect the desktop display server type the process is running in.
 *
 * @returns A value of VBGHDISPLAYSERVERTYPE, or VBGHDISPLAYSERVERTYPE_NONE if detection was not successful.
 *
 * @note    Precedence is:
 *            - Connecting to Wayland (via libwayland-client.so) and/or X11  (via libX11.so).
 *            - VBGH_ENV_WAYLAND_DISPLAY
 *            - VBGH_ENV_XDG_SESSION_TYPE
 *            - VBGH_ENV_XDG_CURRENT_DESKTOP.
 *
 *          Will print a warning to the release log if configuration mismatches.
 */
VBGHDISPLAYSERVERTYPE VBGHDisplayServerTypeDetect(void)
{
    LogRel2(("Detecting display server ...\n"));

    /* Try to connect to the wayland display, assuming it succeeds only when a wayland compositor is active: */
    bool     fHasWayland    = false;
    RTLDRMOD hWaylandClient = NIL_RTLDRMOD;

    /* Array of libwayland-client.so versions to search for.
     * Descending precedence. */
    const char* aLibsWayland[] =
    {
        "libwayland-client.so",
        "libwayland-client.so.0" /* Needed for Ubuntu */
    };

#define GET_SYMBOL(a_Mod, a_Name, a_Fn) \
    rc = RTLdrGetSymbol(a_Mod, a_Name, (void **)&a_Fn); \
    if (RT_FAILURE(rc)) \
        LogRel2(("Symbol '%s' unable to load, rc=%Rrc\n", a_Name, rc));

    int rc = vbghDisplayServerTryLoadLib(aLibsWayland, RT_ELEMENTS(aLibsWayland), &hWaylandClient);
    if (RT_SUCCESS(rc))
    {
        void * (*pWaylandDisplayConnect)(const char *) = NULL;
        GET_SYMBOL(hWaylandClient, "wl_display_connect", pWaylandDisplayConnect);
        void (*pWaylandDisplayDisconnect)(void *) = NULL;
        GET_SYMBOL(hWaylandClient, "wl_display_disconnect", pWaylandDisplayDisconnect);
        if (RT_SUCCESS(rc))
        {
            AssertPtrReturn(pWaylandDisplayConnect, VBGHDISPLAYSERVERTYPE_NONE);
            AssertPtrReturn(pWaylandDisplayDisconnect, VBGHDISPLAYSERVERTYPE_NONE);
            void *pDisplay = pWaylandDisplayConnect(NULL);
            if (pDisplay)
            {
                fHasWayland = true;
                pWaylandDisplayDisconnect(pDisplay);
            }
            else
                LogRel2(("Connecting to Wayland display failed\n"));
        }
        RTLdrClose(hWaylandClient);
    }

    /* Array of libX11.so versions to search for.
     * Descending precedence. */
    const char* aLibsX11[] =
    {
        "libX11.so"
    };

    /* Also try to connect to the default X11 display to determine if Xserver is running: */
    bool     fHasX = false;
    RTLDRMOD hX11  = NIL_RTLDRMOD;
    rc = vbghDisplayServerTryLoadLib(aLibsX11, RT_ELEMENTS(aLibsX11), &hX11);
    if (RT_SUCCESS(rc))
    {
        void * (*pfnOpenDisplay)(const char *) = NULL;
        GET_SYMBOL(hX11, "XOpenDisplay", pfnOpenDisplay);
        int (*pfnCloseDisplay)(void *) = NULL;
        GET_SYMBOL(hX11, "XCloseDisplay", pfnCloseDisplay);
        if (RT_SUCCESS(rc))
        {
            AssertPtrReturn(pfnOpenDisplay, VBGHDISPLAYSERVERTYPE_NONE);
            AssertPtrReturn(pfnCloseDisplay, VBGHDISPLAYSERVERTYPE_NONE);
            void *pDisplay = pfnOpenDisplay(NULL);
            if (pDisplay)
            {
                fHasX = true;
                pfnCloseDisplay(pDisplay);
            }
            else
                LogRel2(("Opening X display failed\n"));
        }

        RTLdrClose(hX11);
    }

#undef GET_SYMBOL

    /* If both wayland and X11 display can be connected then we should have XWayland: */
    VBGHDISPLAYSERVERTYPE retSessionType = VBGHDISPLAYSERVERTYPE_NONE;
    if (fHasWayland && fHasX)
        retSessionType = VBGHDISPLAYSERVERTYPE_XWAYLAND;
    else if (fHasWayland)
        retSessionType = VBGHDISPLAYSERVERTYPE_WAYLAND;
    else if (fHasX)
        retSessionType = VBGHDISPLAYSERVERTYPE_X11;

    LogRel2(("Detected via connection: %s\n", VBGHDisplayServerTypeToStr(retSessionType)));

    /* If retSessionType is set, we assume we're done here;
     * otherwise try the environment variables as a fallback. */
    if (retSessionType != VBGHDISPLAYSERVERTYPE_NONE)
        return retSessionType;

    /*
     * XDG_SESSION_TYPE is a systemd(1) environment variable and is unlikely
     * set in non-systemd environments or remote logins.
     * Therefore we check the Wayland specific display environment variable first.
     */
    VBGHDISPLAYSERVERTYPE waylandDisplayType = VBGHDISPLAYSERVERTYPE_NONE;
    const char *const pWaylandDisplayType = RTEnvGet(VBGH_ENV_WAYLAND_DISPLAY);
    if (pWaylandDisplayType != NULL)
        waylandDisplayType = VBGHDISPLAYSERVERTYPE_WAYLAND;

    LogRel2(("Wayland display type is: %s\n", VBGHDisplayServerTypeToStr(waylandDisplayType)));

    VBGHDISPLAYSERVERTYPE xdgSessionType = VBGHDISPLAYSERVERTYPE_NONE;
    const char *pSessionType = RTEnvGet(VBGH_ENV_XDG_SESSION_TYPE);
    if (pSessionType)
    {
        if (RTStrIStartsWith(pSessionType, "wayland"))
            xdgSessionType = VBGHDISPLAYSERVERTYPE_WAYLAND;
        else if (RTStrIStartsWith(pSessionType, "x11"))
            xdgSessionType = VBGHDISPLAYSERVERTYPE_X11;
    }

    LogRel2(("XDG session type is: %s\n", VBGHDisplayServerTypeToStr(xdgSessionType)));

    VBGHDISPLAYSERVERTYPE xdgCurrentDesktopType = VBGHDISPLAYSERVERTYPE_NONE;

    const char *pszCurDesktop = RTEnvGet(VBGH_ENV_XDG_CURRENT_DESKTOP);
    if (pszCurDesktop)
    {
        if (RTStrIStr(pszCurDesktop, "wayland"))
            xdgCurrentDesktopType = VBGHDISPLAYSERVERTYPE_WAYLAND;
        else if (RTStrIStr(pszCurDesktop, "x11"))
            xdgCurrentDesktopType = VBGHDISPLAYSERVERTYPE_X11;
    }

    LogRel2(("XDG current desktop type is: %s\n", VBGHDisplayServerTypeToStr(xdgCurrentDesktopType)));

    /* Set the returning type according to the precedence. */
    if      (waylandDisplayType    != VBGHDISPLAYSERVERTYPE_NONE) retSessionType = waylandDisplayType;
    else if (xdgSessionType        != VBGHDISPLAYSERVERTYPE_NONE) retSessionType = xdgSessionType;
    else if (xdgCurrentDesktopType != VBGHDISPLAYSERVERTYPE_NONE) retSessionType = xdgCurrentDesktopType;
    else                                                    retSessionType = VBGHDISPLAYSERVERTYPE_NONE;

    /* Try to detect any mismatches between the variables above.
     * This might indicate a misconfigred / broken system, and we can warn the user then. */
#define COMPARE_SESSION_TYPES(a_Type1, a_Type2) \
    if (   (a_Type1 != VBGHDISPLAYSERVERTYPE_NONE) \
        && (a_Type2 != VBGHDISPLAYSERVERTYPE_NONE) \
        && (a_Type1 != a_Type2)) \
        { \
            LogRel(("Unable to reliably detect desktop environment:\n")); \
            LogRel(("Mismatch between %s (%s) and %s (%s) detected! This might indicate a misconfigured and/or broken system!\n", \
                    #a_Type1, VBGHDisplayServerTypeToStr(a_Type1), #a_Type2, VBGHDisplayServerTypeToStr(a_Type2))); \
            LogRel(("Use --session-type to override this detection.\n")); \
            retSessionType = VBGHDISPLAYSERVERTYPE_NONE; \
        }

    COMPARE_SESSION_TYPES(waylandDisplayType, xdgSessionType);
    COMPARE_SESSION_TYPES(waylandDisplayType, xdgCurrentDesktopType);
    COMPARE_SESSION_TYPES(xdgSessionType, xdgCurrentDesktopType);

#undef COMPARE_SESSION_TYPES

    return retSessionType;
}

/**
 * Returns true if @a enmType is indicating running X.
 *
 * @returns \c true if @a enmType is running X, \c false if not.
 * @param   enmType             Type to check.
 */
bool VBGHDisplayServerTypeIsXAvailable(VBGHDISPLAYSERVERTYPE enmType)
{
    return    enmType == VBGHDISPLAYSERVERTYPE_XWAYLAND
           || enmType == VBGHDISPLAYSERVERTYPE_X11;
}

/**
 * Returns true if @a enmType is indicating running Wayland.
 *
 * @returns \c true if @a enmType is running Wayland, \c false if not.
 * @param   enmType             Type to check.
 */
bool VBGHDisplayServerTypeIsWaylandAvailable(VBGHDISPLAYSERVERTYPE enmType)
{
    return    enmType == VBGHDISPLAYSERVERTYPE_XWAYLAND
           || enmType == VBGHDISPLAYSERVERTYPE_WAYLAND;
}

