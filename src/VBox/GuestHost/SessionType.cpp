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

#include <VBox/GuestHost/Log.h>
#include <VBox/GuestHost/SessionType.h>


/*********************************************************************************************************************************
*   Implementation                                                                                                               *
*********************************************************************************************************************************/

/**
 * Returns the VBGHSESSIONTYPE as a string.
 *
 * @returns VBGHSESSIONTYPE as a string.
 * @param   enmType             VBGHSESSIONTYPE to return as a string.
 */
const char *VBGHSessionTypeToStr(VBGHSESSIONTYPE enmType)
{
    switch (enmType)
    {
        RT_CASE_RET_STR(VBGHSESSIONTYPE_NONE);
        RT_CASE_RET_STR(VBGHSESSIONTYPE_AUTO);
        RT_CASE_RET_STR(VBGHSESSIONTYPE_WAYLAND);
        RT_CASE_RET_STR(VBGHSESSIONTYPE_X11);
        default: break;
    }

    AssertFailedReturn("<invalid>");
}

/**
 * Tries to detect the desktop session type the process is running in.
 *
 * @returns A value of VBGHSESSIONTYPE, or VBGHSESSIONTYPE_NONE if detection was not successful.
 *
 * @note    Precedence is: VBGH_ENV_WAYLAND_DISPLAY, VBGH_ENV_XDG_SESSION_TYPE, VBGH_ENV_XDG_CURRENT_DESKTOP.
 */
VBGHSESSIONTYPE VBGHSessionTypeDetect(void)
{
    VBGHLogVerbose(1, "Detecting session type ...\n");

    /*
     * XDG_SESSION_TYPE is a systemd(1) environment variable and is unlikely
     * set in non-systemd environments or remote logins.
     * Therefore we check the Wayland specific display environment variable first.
     */
    VBGHSESSIONTYPE waylandDisplayType = VBGHSESSIONTYPE_NONE;
    const char *const pWaylandDisplayType = RTEnvGet(VBGH_ENV_WAYLAND_DISPLAY);
    if (pWaylandDisplayType != NULL)
        waylandDisplayType = VBGHSESSIONTYPE_WAYLAND;

    VBGHLogVerbose(1, "Wayland display type is: %s\n", VBGHSessionTypeToStr(waylandDisplayType));

    VBGHSESSIONTYPE xdgSessionType = VBGHSESSIONTYPE_NONE;
    const char *pSessionType = RTEnvGet(VBGH_ENV_XDG_SESSION_TYPE);
    if (pSessionType)
    {
        if (RTStrIStartsWith(pSessionType, "wayland"))
            xdgSessionType = VBGHSESSIONTYPE_WAYLAND;
        else if (RTStrIStartsWith(pSessionType, "x11"))
            xdgSessionType = VBGHSESSIONTYPE_X11;
    }

    VBGHLogVerbose(1, "XDG session type is: %s\n", VBGHSessionTypeToStr(xdgSessionType));

    VBGHSESSIONTYPE xdgCurrentDesktopType = VBGHSESSIONTYPE_NONE;

    const char *pszCurDesktop = RTEnvGet(VBGH_ENV_XDG_CURRENT_DESKTOP);
    if (pszCurDesktop)
    {
        if (RTStrIStr(pszCurDesktop, "wayland"))
            xdgCurrentDesktopType = VBGHSESSIONTYPE_WAYLAND;
        else if (RTStrIStr(pszCurDesktop, "x11"))
            xdgCurrentDesktopType = VBGHSESSIONTYPE_X11;
    }

    VBGHLogVerbose(1, "XDG current desktop type is: %s\n", VBGHSessionTypeToStr(xdgCurrentDesktopType));

    /* Set the returning type according to the precedence. */
    VBGHSESSIONTYPE retSessionType;
    if      (waylandDisplayType    != VBGHSESSIONTYPE_NONE) retSessionType = waylandDisplayType;
    else if (xdgSessionType        != VBGHSESSIONTYPE_NONE) retSessionType = xdgSessionType;
    else if (xdgCurrentDesktopType != VBGHSESSIONTYPE_NONE) retSessionType = xdgCurrentDesktopType;
    else                                                    retSessionType = VBGHSESSIONTYPE_NONE;

    /* Try to detect any mismatches between the variables above.
     * This might indicate a misconfigred / broken system, and we can warn the user then. */
#define COMPARE_SESSION_TYPES(a_Type1, a_Type2) \
    if (   (a_Type1 != VBGHSESSIONTYPE_NONE) \
        && (a_Type2 != VBGHSESSIONTYPE_NONE) \
        && (a_Type1 != a_Type2)) \
        { \
            VBGHLogError("Unable to reliably detect desktop environment:\n"); \
            VBGHLogError("Mismatch between %s (%s) and %s (%s) detected! This might indicate a misconfigured and/or broken system!\n", \
                         #a_Type1, VBGHSessionTypeToStr(a_Type1), #a_Type2, VBGHSessionTypeToStr(a_Type2)); \
            VBGHLogError("Use --session-type to override this detection.\n"); \
            retSessionType = VBGHSESSIONTYPE_NONE; \
        }

    COMPARE_SESSION_TYPES(waylandDisplayType, xdgSessionType);
    COMPARE_SESSION_TYPES(waylandDisplayType, xdgCurrentDesktopType);
    COMPARE_SESSION_TYPES(xdgSessionType, xdgCurrentDesktopType);

#undef COMPARE_SESSION_TYPES

    return retSessionType;
}

