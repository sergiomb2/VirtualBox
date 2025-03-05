/* $Id$ */
/** @file
 * Driver installation utility for Windows hosts and guests.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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
#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/cpp/ministring.h> /* For replacement fun. */
#include <iprt/env.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/types.h>
#include <iprt/process.h> /* For RTProcShortName(). */
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/test.h>
#include <iprt/utf16.h>
#include <iprt/win/windows.h>

#include <package-generated.h>
#include "product-generated.h"

#include <VBox/version.h>
#include <VBox/log.h>

#include <VBox/err.h>

#include <VBox/GuestHost/VBoxWinDrvInst.h>
#include <VBox/GuestHost/VBoxWinDrvStore.h>


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
static DECLCALLBACK(RTEXITCODE) vboxDrvInstCmdListMain(PRTGETOPTSTATE pGetState);
static DECLCALLBACK(RTEXITCODE) vboxDrvInstCmdInstallMain(PRTGETOPTSTATE pGetState);
static DECLCALLBACK(RTEXITCODE) vboxDrvInstCmdUninstallMain(PRTGETOPTSTATE pGetState);
static DECLCALLBACK(RTEXITCODE) vboxDrvInstCmdServiceMain(PRTGETOPTSTATE pGetState);

static DECLCALLBACK(const char *) vboxDrvInstCmdListHelp(PCRTGETOPTDEF pOpt);
static DECLCALLBACK(const char *) vboxDrvInstCmdInstallHelp(PCRTGETOPTDEF pOpt);
static DECLCALLBACK(const char *) vboxDrvInstCmdUninstallHelp(PCRTGETOPTDEF pOpt);
static DECLCALLBACK(const char *) vboxDrvInstCmdServiceHelp(PCRTGETOPTDEF pOpt);

struct VBOXDRVINSTCMD;
static RTEXITCODE vboxDrvInstShowUsage(PRTSTREAM pStrm, VBOXDRVINSTCMD const *pOnlyCmd);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Verbosity level. */
static unsigned  g_uVerbosity = 0;
static PRTLOGGER g_pLoggerRelease = NULL;
static char      g_szLogFile[RTPATH_MAX];
static uint32_t  g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t  g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t  g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */


/*********************************************************************************************************************************
*   Definitions                                                                                                                  *
*********************************************************************************************************************************/
typedef enum VBOXDRVINSTEXITCODE
{
    /** A reboot is needed in order to complete the (un)installation. */
    VBOXDRVINSTEXITCODE_REBOOT_NEEDED = RTEXITCODE_END
} VBOXDRVINSTEXITCODE;

/**
 * Driver installation command table entry.
 */
typedef struct VBOXDRVINSTCMD
{
    /** The command name. */
    const char     *pszCommand;
    /** The command handler.   */
    DECLCALLBACKMEMBER(RTEXITCODE, pfnHandler,(PRTGETOPTSTATE pGetState));

    /** Command description.   */
    const char     *pszDesc;
    /** Options array.  */
    PCRTGETOPTDEF   paOptions;
    /** Number of options in the option array. */
    size_t          cOptions;
    /** Gets help for an option. */
    DECLCALLBACKMEMBER(const char *, pfnOptionHelp,(PCRTGETOPTDEF pOpt));
} VBOXDRVINSTCMD;
/** Pointer to a const VBOXDRVINSTCMD entry. */
typedef VBOXDRVINSTCMD const *PCVBOXDRVINSTCMD;

/**
 * Command definition for the 'list' command.
 */
const VBOXDRVINSTCMD g_CmdList =
{
    "list",
    vboxDrvInstCmdListMain,
    "Lists installed drivers.",
    NULL, /* paOptions */
    0,    /* cOptions */
    vboxDrvInstCmdListHelp
};

/**
 * Long option values for the 'install' command.
 */
enum
{
    VBOXDRVINST_INSTALL_OPT_INF_FILE = 900,
    VBOXDRVINST_INSTALL_OPT_INF_SECTION,
    VBOXDRVINST_INSTALL_OPT_MODEL,
    VBOXDRVINST_INSTALL_OPT_PNPID,
    VBOXDRVINST_INSTALL_OPT_NOT_FORCE,
    VBOXDRVINST_INSTALL_OPT_NOT_SILENT,
    VBOXDRVINST_INSTALL_OPT_IGNORE_REBOOT,
    VBOXDRVINST_INSTALL_OPT_DEBUG_OS_VER
};

/**
 * Command line parameters for the 'install' command.
 */
static const RTGETOPTDEF g_aCmdInstallOptions[] =
{
    { "--inf-file",      VBOXDRVINST_INSTALL_OPT_INF_FILE,      RTGETOPT_REQ_STRING  },
    { "--inf-section",   VBOXDRVINST_INSTALL_OPT_INF_SECTION,   RTGETOPT_REQ_STRING  },
    { "--model",         VBOXDRVINST_INSTALL_OPT_MODEL,         RTGETOPT_REQ_STRING  },
    { "--pnp",           VBOXDRVINST_INSTALL_OPT_PNPID,         RTGETOPT_REQ_STRING  },
    { "--pnpid" ,        VBOXDRVINST_INSTALL_OPT_PNPID,         RTGETOPT_REQ_STRING  },
    { "--pnp-id",        VBOXDRVINST_INSTALL_OPT_PNPID,         RTGETOPT_REQ_STRING  },
    { "--not-force",     VBOXDRVINST_INSTALL_OPT_NOT_FORCE,     RTGETOPT_REQ_NOTHING },
    { "--not-silent",    VBOXDRVINST_INSTALL_OPT_NOT_SILENT,    RTGETOPT_REQ_NOTHING },
    { "--ignore-reboot", VBOXDRVINST_INSTALL_OPT_IGNORE_REBOOT, RTGETOPT_REQ_NOTHING },
    { "--debug-os-ver",  VBOXDRVINST_INSTALL_OPT_DEBUG_OS_VER,  RTGETOPT_REQ_UINT32_PAIR }
};

/**
 * Command definition for the 'install' command.
 */
const VBOXDRVINSTCMD g_CmdInstall =
{
    "install",
    vboxDrvInstCmdInstallMain,
    "Installs a driver.",
    g_aCmdInstallOptions,
    RT_ELEMENTS(g_aCmdInstallOptions),
    vboxDrvInstCmdInstallHelp
};

/**
 * Long option values for the 'uninstall' command.
 */
enum
{
    VBOXDRVINST_UNINSTALL_OPT_HOST = 900,
    VBOXDRVINST_UNINSTALL_OPT_INF_FILE,
    VBOXDRVINST_UNINSTALL_OPT_INF_SECTION,
    VBOXDRVINST_UNINSTALL_OPT_MODEL,
    VBOXDRVINST_UNINSTALL_OPT_PNPID,
    VBOXDRVINST_UNINSTALL_OPT_FORCE,
    VBOXDRVINST_UNINSTALL_OPT_NOT_SILENT,
    VBOXDRVINST_UNINSTALL_OPT_IGNORE_REBOOT
};

/**
 * Command line parameters for the 'uninstall' command.
 */
static const RTGETOPTDEF g_aCmdUninstallOptions[] =
{
    /* Sub commands. */
    { "host",            VBOXDRVINST_UNINSTALL_OPT_HOST,          RTGETOPT_REQ_NOTHING  },
    /* Parameters. */
    { "--inf-file",      VBOXDRVINST_UNINSTALL_OPT_INF_FILE,      RTGETOPT_REQ_STRING  },
    { "--inf-section",   VBOXDRVINST_UNINSTALL_OPT_INF_SECTION,   RTGETOPT_REQ_STRING  },
    { "--model",         VBOXDRVINST_UNINSTALL_OPT_MODEL,         RTGETOPT_REQ_STRING  },
    { "--pnp",           VBOXDRVINST_UNINSTALL_OPT_PNPID,         RTGETOPT_REQ_STRING  },
    { "--pnpid" ,        VBOXDRVINST_UNINSTALL_OPT_PNPID,         RTGETOPT_REQ_STRING  },
    { "--pnp-id",        VBOXDRVINST_UNINSTALL_OPT_PNPID,         RTGETOPT_REQ_STRING  },
    { "--force",         VBOXDRVINST_UNINSTALL_OPT_FORCE,         RTGETOPT_REQ_NOTHING },
    { "--not-silent",    VBOXDRVINST_UNINSTALL_OPT_NOT_SILENT,    RTGETOPT_REQ_NOTHING },
    { "--ignore-reboot", VBOXDRVINST_UNINSTALL_OPT_IGNORE_REBOOT, RTGETOPT_REQ_NOTHING }
};

/**
 * Command definition for the 'uninstall' command.
 */
const VBOXDRVINSTCMD g_CmdUninstall =
{
    "uninstall",
    vboxDrvInstCmdUninstallMain,
    "Uninstalls drivers.",
    g_aCmdUninstallOptions,
    RT_ELEMENTS(g_aCmdUninstallOptions),
    vboxDrvInstCmdUninstallHelp
};

/**
 * Long option values for the 'service' command.
 */
enum
{
    VBOXDRVINST_SERVICE_OPT_START = 900,
    VBOXDRVINST_SERVICE_OPT_STOP,
    VBOXDRVINST_SERVICE_OPT_RESTART,
    VBOXDRVINST_SERVICE_OPT_WAIT,
    VBOXDRVINST_SERVICE_OPT_NO_WAIT
};

/**
 * Command line parameters for the 'service' command.
 */
static const RTGETOPTDEF g_aCmdServiceOptions[] =
{
    { "start",     VBOXDRVINST_SERVICE_OPT_START,   RTGETOPT_REQ_NOTHING },
    { "stop",      VBOXDRVINST_SERVICE_OPT_STOP,    RTGETOPT_REQ_NOTHING },
    { "restart",   VBOXDRVINST_SERVICE_OPT_RESTART, RTGETOPT_REQ_NOTHING },
    { "--wait",    VBOXDRVINST_SERVICE_OPT_WAIT,    RTGETOPT_REQ_INT32 },
    { "--no-wait", VBOXDRVINST_SERVICE_OPT_NO_WAIT, RTGETOPT_REQ_NOTHING }
};

/**
 * Command definition for the 'service' command.
 */
const VBOXDRVINSTCMD g_CmdService =
{
    "service",
    vboxDrvInstCmdServiceMain,
    "Controls services.",
    g_aCmdServiceOptions,
    RT_ELEMENTS(g_aCmdServiceOptions),
    vboxDrvInstCmdServiceHelp
};

/**
 * Commands.
 */
static const VBOXDRVINSTCMD * const g_apCommands[] =
{
    &g_CmdList,
    &g_CmdInstall,
    &g_CmdUninstall,
    &g_CmdService
};

/**
 * Common option definitions for all commands.
 */
static const RTGETOPTDEF g_aCmdCommonOptions[] =
{
    { "--logfile",     'l', RTGETOPT_REQ_STRING },
    { "--help",        'h', RTGETOPT_REQ_NOTHING },
    { "--verbose",     'v', RTGETOPT_REQ_NOTHING },
    { "--version",     'V', RTGETOPT_REQ_NOTHING }
};


/*********************************************************************************************************************************
*   Implementation                                                                                                               *
*********************************************************************************************************************************/

/**
 * Logs message, va_list version.
 *
 * @returns VBox status code.
 * @param   pszPrefix           Logging prefix to use. Can be NULL.
 * @param   pszFormat           Format string to log.
 * @param   args                va_list to use.
 */
DECLINLINE(void) vboxDrvInstLogExV(const char *pszPrefix, const char *pszFormat, va_list args)
{
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    AssertPtrReturnVoid(psz);

    if (pszPrefix)
        LogRel(("%s: %s", pszPrefix, psz));
    else
        LogRel(("%s", psz));

    RTStrFree(psz);
}

/**
 * Logs a message.
 *
 * @returns VBox status code.
 * @param   pszFormat           Format string to log.
 */
DECLINLINE(void) vboxDrvInstLogError(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    vboxDrvInstLogExV("*** Error", pszFormat, args);
    va_end(args);
}

/**
 * Logs an error message.
 *
 * @returns VBox status code.
 * @param   pszFormat           Format string to log.
 */
DECLINLINE(void) vboxDrvInstLog(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    vboxDrvInstLogExV(NULL, pszFormat, args);
    va_end(args);
}

/**
 * Logging callback for the Windows driver (un)installation code.
 */
static DECLCALLBACK(void) vboxDrvInstLogCallback(VBOXWINDRIVERLOGTYPE enmType, const char *pszMsg, void *pvUser)
{
    RT_NOREF(pvUser);

    /*
     * Log to standard output:
     */
    switch (enmType)
    {
        case VBOXWINDRIVERLOGTYPE_ERROR:
            vboxDrvInstLogError("%s\n", pszMsg);
            break;

        case VBOXWINDRIVERLOGTYPE_REBOOT_NEEDED:
            vboxDrvInstLog("A reboot is needed in order to complete the (un)installation!\n");
            break;

        default:
            vboxDrvInstLog("%s\n", pszMsg);
            break;
    }
}

/** Option help for the 'list' command. */
static DECLCALLBACK(const char *) vboxDrvInstCmdListHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        default:
            break;
    }
    return NULL;
}

/**
 * Main (entry) function for the 'list' command.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) vboxDrvInstCmdListMain(PRTGETOPTSTATE pGetState)
{
    const char *pszPattern = NULL;

    int           ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(pGetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'h':
                return vboxDrvInstShowUsage(g_pStdOut, &g_CmdList);

            case VINF_GETOPT_NOT_OPTION:
            {
                /** @todo Use pattern to filter entries, e.g. "pnp:<PNP-ID>" or "model:VBoxSup*". */
                pszPattern = ValueUnion.psz;
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    PVBOXWINDRVSTORE pStore;
    int rc = VBoxWinDrvStoreCreate(&pStore);

    PVBOXWINDRVSTORELIST pList = NULL;
    if (pszPattern)
        rc = VBoxWinDrvStoreQueryAny(pStore, pszPattern, &pList);
    else
        rc = VBoxWinDrvStoreQueryAll(pStore, &pList);
    if (RT_SUCCESS(rc))
    {
        vboxDrvInstLog("Location: %s\n\n", VBoxWinDrvStoreBackendGetLocation(pStore));

        vboxDrvInstLog("%-40s | %-40s\n", "OEM INF File", "Version");
        vboxDrvInstLog("%-40s | %-40s\n", "    Model (First)", "PnP ID (First)");
        vboxDrvInstLog("--------------------------------------------------------------------------------\n");

        size_t cEntries = 0;
        PVBOXWINDRVSTOREENTRY pCur;
        RTListForEach(&pList->List, pCur, VBOXWINDRVSTOREENTRY, Node)
        {
            vboxDrvInstLog("%-40ls | %-40ls\n",
                           pCur->wszInfFile, pCur->Ver.wszDriverVer);
            vboxDrvInstLog("    %-36ls | %-40ls\n",
                           pCur->wszModel, pCur->wszPnpId);
            cEntries++;
        }

        if (pszPattern)
            vboxDrvInstLog("\nFound %zu entries (filtered).\n", cEntries);
        else
            vboxDrvInstLog("\nFound %zu entries.\n", cEntries);
    }

    VBoxWinDrvStoreListFree(pList);

    VBoxWinDrvStoreDestroy(pStore);
    pStore = NULL;

    vboxDrvInstLog("\nUse DOS-style wildcards to adjust results.\n");
    vboxDrvInstLog("Use \"--help\" to print syntax help.\n");

    return RTEXITCODE_SUCCESS;
}

/** Option help for the 'install' command. */
static DECLCALLBACK(const char *) vboxDrvInstCmdInstallHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case VBOXDRVINST_INSTALL_OPT_INF_FILE:      return "Specifies the INF file to install";
        case VBOXDRVINST_INSTALL_OPT_INF_SECTION:   return "Specifies the INF section to install";
        case VBOXDRVINST_INSTALL_OPT_MODEL:         return "Specifies the driver model";
        case VBOXDRVINST_INSTALL_OPT_PNPID:         return "Specifies the PnP (device) ID";
        case VBOXDRVINST_INSTALL_OPT_NOT_FORCE:     return "Installation will not be forced";
        case VBOXDRVINST_INSTALL_OPT_NOT_SILENT:    return "Installation will not run in silent mode";
        case VBOXDRVINST_INSTALL_OPT_IGNORE_REBOOT: return "Ignores reboot requirements";
        case VBOXDRVINST_INSTALL_OPT_DEBUG_OS_VER:  return "Overwrites the detected OS version";
        default:
            break;
    }
    return NULL;
}

/**
 * Main (entry) function for the 'install' command.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) vboxDrvInstCmdInstallMain(PRTGETOPTSTATE pGetState)
{
    char *pszInfFile = NULL;
    char *pszModel = NULL;
    char *pszPnpId = NULL;
    char *pszInfSection = NULL;
    uint64_t uOsVer = 0;

    /* By default we want to force an installation.
     *
     * However, we do *not* want the installation to be silent by default,
     * as this this will result in an ERROR_AUTHENTICODE_TRUST_NOT_ESTABLISHED error
     * if drivers get installed with our mixed SHA1 / SH256 certificates on older
     * Windows guest (7, Vista, ++).
     *
     * So if the VBOX_WIN_DRIVERINSTALL_F_SILENT is missing, this will result in a
     * (desired) Windows driver installation dialog to confirm (or reject) the installation
     * by the user.
     *
     * On the other hand, for unattended installs we need VBOX_WIN_DRIVERINSTALL_F_SILENT
     * being set, as our certificates will get installed into the Windows certificate
     * store *before* we perform any driver installation.
     */
    uint32_t fInstall = VBOX_WIN_DRIVERINSTALL_F_FORCE;

    /* Whether to ignore reboot messages or not. This will also affect the returned exit code. */
    bool fIgnoreReboot = false;

    int rc = VINF_SUCCESS;

#define DUP_ARG_TO_STR(a_Str) \
    a_Str = RTStrDup(ValueUnion.psz); \
    if (!a_Str) \
    { \
        RTMsgError("Can't handle argument '%s': Out of memory\n", ValueUnion.psz); \
        rc = VERR_NO_MEMORY; \
        break; \
    }

    int           ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(pGetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'h':
                return vboxDrvInstShowUsage(g_pStdOut, &g_CmdInstall);

            case VBOXDRVINST_INSTALL_OPT_INF_FILE:
                DUP_ARG_TO_STR(pszInfFile);
                break;

            case VBOXDRVINST_INSTALL_OPT_INF_SECTION:
                DUP_ARG_TO_STR(pszInfSection);
                break;

            case VBOXDRVINST_INSTALL_OPT_MODEL:
                DUP_ARG_TO_STR(pszModel);
                break;

            case VBOXDRVINST_INSTALL_OPT_PNPID:
                DUP_ARG_TO_STR(pszPnpId);
                break;

            case VBOXDRVINST_INSTALL_OPT_NOT_FORCE:
                fInstall &= ~VBOX_WIN_DRIVERINSTALL_F_FORCE;
                break;

            case VBOXDRVINST_INSTALL_OPT_NOT_SILENT:
                fInstall &= ~VBOX_WIN_DRIVERINSTALL_F_SILENT;
                break;

            case VBOXDRVINST_INSTALL_OPT_IGNORE_REBOOT:
                fIgnoreReboot = true;
                break;

            case VBOXDRVINST_INSTALL_OPT_DEBUG_OS_VER:
                uOsVer = RTSYSTEM_MAKE_NT_VERSION(ValueUnion.PairU32.uFirst, ValueUnion.PairU32.uSecond,
                                                  0 /* Build Version */);
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

#undef DUP_ARG_TO_STR

    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;

    VBOXWINDRVINST hWinDrvInst;
    rc = VBoxWinDrvInstCreateEx(&hWinDrvInst, g_uVerbosity, &vboxDrvInstLogCallback, NULL /* pvUser */);
    if (RT_SUCCESS(rc))
    {
        if (uOsVer)
            VBoxWinDrvInstSetOsVersion(hWinDrvInst, uOsVer);

        rc = VBoxWinDrvInstInstallEx(hWinDrvInst, pszInfFile, pszModel, pszPnpId, fInstall);
        if (RT_SUCCESS(rc))
        {
            if (   rc == VINF_REBOOT_NEEDED
                && !fIgnoreReboot)
                rcExit = (RTEXITCODE)VBOXDRVINSTEXITCODE_REBOOT_NEEDED;
        }
        else
            rcExit = RTEXITCODE_FAILURE;

        VBoxWinDrvInstDestroy(hWinDrvInst);
    }

    RTStrFree(pszInfFile);
    RTStrFree(pszInfSection);
    RTStrFree(pszModel);
    RTStrFree(pszPnpId);

    return rcExit;
}

/** Option help for the 'uninstall' command. */
static DECLCALLBACK(const char *) vboxDrvInstCmdUninstallHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case VBOXDRVINST_UNINSTALL_OPT_HOST:          return "Uninstalls all VirtualBox host drivers";
        case VBOXDRVINST_UNINSTALL_OPT_INF_FILE:      return "Specifies the INF File to uninstall";
        case VBOXDRVINST_UNINSTALL_OPT_INF_SECTION:   return "Specifies the INF section to uninstall";
        case VBOXDRVINST_UNINSTALL_OPT_MODEL:         return "Specifies the driver model to uninstall";
        case VBOXDRVINST_UNINSTALL_OPT_PNPID:         return "Specifies the PnP (device) ID to uninstall";
        case VBOXDRVINST_UNINSTALL_OPT_FORCE:         return "Forces uninstallation";
        case VBOXDRVINST_UNINSTALL_OPT_NOT_SILENT:    return "Runs uninstallation in non-silent mode";
        case VBOXDRVINST_UNINSTALL_OPT_IGNORE_REBOOT: return "Ignores reboot requirements";
        default:
            break;
    }
    return NULL;
}

/**
 * Uninstalls all (see notes below) VirtualBox host-related drivers.
 *
 * @returns VBox status code.
 * @param   hDrvInst            Windows driver installer handle to use.
 * @param   fInstallFlags       [Un]Installation flags to use (of type VBOX_WIN_DRIVERINSTALL_F_XXX).
 */
static int vboxDrvInstCmdUninstallVBoxHost(VBOXWINDRVINST hDrvInst, uint32_t fInstallFlags)
{
    /** @todo Check for running VirtualBox processes first? */

    int rc;

#define UNINSTALL_DRIVER(a_Driver) \
    rc = VBoxWinDrvInstUninstall(hDrvInst, NULL /* pszInfFile */, a_Driver, NULL /* pszPnPId */, fInstallFlags); \
    if (   RT_FAILURE(rc) \
        && !(fInstallFlags & VBOX_WIN_DRIVERINSTALL_F_FORCE)) \
        return rc;

#define CONTROL_SERVICE(a_Svc, a_Fn) \
    rc = VBoxWinDrvInstControlServiceEx(hDrvInst, a_Svc, a_Fn, VBOXWINDRVSVCFN_F_WAIT, RT_MS_30SEC); \
    if (RT_FAILURE(rc)) \
    { \
        if (   rc != VERR_NOT_FOUND /* Service is optional, thus not fatal if not found. */ \
            && !(fInstallFlags & VBOX_WIN_DRIVERINSTALL_F_FORCE)) \
            return rc; \
    }

#define STOP_SERVICE(a_Svc) CONTROL_SERVICE(a_Svc, VBOXWINDRVSVCFN_STOP)
#define DELETE_SERVICE(a_Svc) CONTROL_SERVICE(a_Svc, VBOXWINDRVSVCFN_DELETE)

    /* Stop VBoxSDS first. */
    STOP_SERVICE("VBoxSDS");

    /*
     * Note! The order how to uninstall all drivers is important here,
     *       as drivers can (and will!) hold references to the VBoxSUP (VirtualBox support) driver.
     *       So do not change the order here unless you exactly know what you are doing.
     */
    static const char *s_aszDriverUninstallOrdered[] =
    {
        "VBoxNetAdp*", /* To catch also deprecated VBoxNetAdp5 drivers. */
        "VBoxNetLwf*",
        "VBoxUSB*"
    };

    for (size_t i = 0; i < RT_ELEMENTS(s_aszDriverUninstallOrdered); i++)
        UNINSTALL_DRIVER(s_aszDriverUninstallOrdered[i]);

    static const char *s_aszServicesToStopOrdered[] =
    {
        "VBoxNetAdp",
        "VBoxNetLwf",
        "VBoxUSBMon"
    };

    for (size_t i = 0; i < RT_ELEMENTS(s_aszServicesToStopOrdered); i++)
        STOP_SERVICE(s_aszServicesToStopOrdered[i]);

    /* Must come last. */
    UNINSTALL_DRIVER("VBoxSup*");

    /* Delete all services (if not already done via driver uninstallation). */
    for (size_t i = 0; i < RT_ELEMENTS(s_aszServicesToStopOrdered); i++)
        DELETE_SERVICE(s_aszServicesToStopOrdered[i]);

    /* Ditto. */
    DELETE_SERVICE("VBoxSup");

#undef STOP_SERVICE
#undef UNINSTALL_DRIVER

    return VINF_SUCCESS;
}

/**
 * Main (entry) function for the 'uninstall' command.
 *
 * @returns Program exit code.
 * @param   pGetState   RTGetOpt state.
 */
static DECLCALLBACK(RTEXITCODE) vboxDrvInstCmdUninstallMain(PRTGETOPTSTATE pGetState)
{
    char *pszInfFile = NULL;
    char *pszModel = NULL;
    char *pszPnpId = NULL;
    char *pszInfSection = NULL;

    /* By default we want a silent uninstallation (but not forcing it). */
    uint32_t fInstall = VBOX_WIN_DRIVERINSTALL_F_SILENT;

    /* Whether to ignore reboot messages or not. This will also affect the returned exit code. */
    bool fIgnoreReboot = false;
    /* Whether to (automatically) uninstall all related VBox host drivers or not. */
    bool fVBoxHost = false;

    int rc = VINF_SUCCESS;

#define DUP_ARG_TO_STR(a_Str) \
    a_Str = RTStrDup(ValueUnion.psz); \
    if (!a_Str) \
    { \
        RTMsgError("Can't handle argument '%s': Out of memory\n", ValueUnion.psz); \
        rc = VERR_NO_MEMORY; \
        break; \
    }

    int           ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(pGetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'h':
                return vboxDrvInstShowUsage(g_pStdOut, &g_CmdUninstall);

            case VBOXDRVINST_UNINSTALL_OPT_HOST:
                fVBoxHost = true;
                break;

            case VBOXDRVINST_UNINSTALL_OPT_INF_FILE:
                DUP_ARG_TO_STR(pszInfFile);
                break;

            case VBOXDRVINST_UNINSTALL_OPT_INF_SECTION:
                DUP_ARG_TO_STR(pszInfSection);
                break;

            case VBOXDRVINST_UNINSTALL_OPT_MODEL:
                DUP_ARG_TO_STR(pszModel);
                break;

            case VBOXDRVINST_UNINSTALL_OPT_PNPID:
                DUP_ARG_TO_STR(pszPnpId);
                break;

            case VBOXDRVINST_UNINSTALL_OPT_FORCE:
                fInstall |= VBOX_WIN_DRIVERINSTALL_F_FORCE;
                break;

            case VBOXDRVINST_UNINSTALL_OPT_NOT_SILENT:
                fInstall &= ~VBOX_WIN_DRIVERINSTALL_F_SILENT;
                break;

            case VBOXDRVINST_UNINSTALL_OPT_IGNORE_REBOOT:
                fIgnoreReboot = true;
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

#undef DUP_ARG_TO_STR

    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;

    VBOXWINDRVINST hWinDrvInst;
    rc = VBoxWinDrvInstCreateEx(&hWinDrvInst, g_uVerbosity, &vboxDrvInstLogCallback, NULL /* pvUser */);
    if (RT_SUCCESS(rc))
    {
        if (fVBoxHost)
            rc = vboxDrvInstCmdUninstallVBoxHost(hWinDrvInst, fInstall);
        else
            rc = VBoxWinDrvInstUninstall(hWinDrvInst, pszInfFile, pszModel, pszPnpId, fInstall);
        if (RT_SUCCESS(rc))
        {
            if (   rc == VINF_REBOOT_NEEDED
                && !fIgnoreReboot)
                rcExit = (RTEXITCODE)VBOXDRVINSTEXITCODE_REBOOT_NEEDED;
        }
        else
            rcExit = RTEXITCODE_FAILURE;

        VBoxWinDrvInstDestroy(hWinDrvInst);
    }

    RTStrFree(pszInfFile);
    RTStrFree(pszInfSection);
    RTStrFree(pszModel);
    RTStrFree(pszPnpId);

    return rcExit;
}

/** Option help for the 'service' command. */
static DECLCALLBACK(const char *) vboxDrvInstCmdServiceHelp(PCRTGETOPTDEF pOpt)
{
    switch (pOpt->iShort)
    {
        case VBOXDRVINST_SERVICE_OPT_START:   return "Starts the service";
        case VBOXDRVINST_SERVICE_OPT_STOP:    return "Stops the service";
        case VBOXDRVINST_SERVICE_OPT_RESTART: return "Restarts the service";
        case VBOXDRVINST_SERVICE_OPT_WAIT:    return "Waits for the service to reach the desired state";
        case VBOXDRVINST_SERVICE_OPT_NO_WAIT: return "Skips waiting for the service to reach the desired state";

        default:
            break;
    }
    return NULL;
}

static DECLCALLBACK(RTEXITCODE) vboxDrvInstCmdServiceMain(PRTGETOPTSTATE pGetState)
{
    const char     *pszService = NULL;
    VBOXWINDRVSVCFN enmFn      = VBOXWINDRVSVCFN_INVALID;
    /* We wait 30s by default, unless specified otherwise below. */
    uint32_t        fFlags     = VBOXWINDRVSVCFN_F_WAIT;
    RTMSINTERVAL    msTimeout  = RT_MS_30SEC;

    int           ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(pGetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'h':
                return vboxDrvInstShowUsage(g_pStdOut, &g_CmdService);

            case VBOXDRVINST_SERVICE_OPT_START:
            {
                if (enmFn != VBOXWINDRVSVCFN_INVALID)
                    return RTMsgErrorExitFailure("Service control function already specified\n");
                enmFn = VBOXWINDRVSVCFN_START;
                break;
            }

            case VBOXDRVINST_SERVICE_OPT_STOP:
            {
                if (enmFn != VBOXWINDRVSVCFN_INVALID)
                    return RTMsgErrorExitFailure("Service control function already specified\n");
                enmFn = VBOXWINDRVSVCFN_STOP;
                break;
            }

            case VBOXDRVINST_SERVICE_OPT_RESTART:
            {
                if (enmFn != VBOXWINDRVSVCFN_INVALID)
                    return RTMsgErrorExitFailure("Service control function already specified\n");
                enmFn = VBOXWINDRVSVCFN_RESTART;
                break;
            }

            case VBOXDRVINST_SERVICE_OPT_WAIT:
                /* Note: fFlags already set above. */
                msTimeout = ValueUnion.u32 * RT_MS_1SEC; /* Seconds -> Milliseconds. */
                if (!msTimeout)
                    return RTMsgErrorExitFailure("Timeout value is invalid\n");
                break;

            case VBOXDRVINST_SERVICE_OPT_NO_WAIT:
                fFlags &= ~VBOXWINDRVSVCFN_F_WAIT;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                if (pszService)
                    return RTMsgErrorExitFailure("Service name already specified\n");

                pszService = ValueUnion.psz;
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (!pszService)
        return RTMsgErrorExitFailure("No service to control specified\n");
    if (enmFn == VBOXWINDRVSVCFN_INVALID)
        return RTMsgErrorExitFailure("No or invalid service control function specified\n");

    VBOXWINDRVINST hWinDrvInst;
    int rc = VBoxWinDrvInstCreateEx(&hWinDrvInst, g_uVerbosity, &vboxDrvInstLogCallback, NULL /* pvUser */);
    if (RT_SUCCESS(rc))
    {
        rc = VBoxWinDrvInstControlServiceEx(hWinDrvInst, pszService, enmFn, fFlags, msTimeout);
        VBoxWinDrvInstDestroy(hWinDrvInst);
    }

    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

/**
 * Shows the commands and their descriptions.
 *
 * @returns RTEXITCODE
 * @param   pStrm               Stream to use.
 */
static RTEXITCODE vboxDrvInstShowCommands(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm, "Commands:\n");
    for (uintptr_t iCmd = 0; iCmd < RT_ELEMENTS(g_apCommands); iCmd++)
        RTStrmPrintf(pStrm, "%12s - %s\n", g_apCommands[iCmd]->pszCommand, g_apCommands[iCmd]->pszDesc);
    return RTEXITCODE_SUCCESS;
}

/**
 * Shows the general usage.
 *
 * @returns RTEXITCODE
 * @param   pStrm               Stream to use.
 */
static RTEXITCODE vboxDrvInstShowUsage(PRTSTREAM pStrm, PCVBOXDRVINSTCMD pOnlyCmd)
{
    const char *pszProcName = RTProcShortName();

    RTStrmPrintf(pStrm, "usage: %s [global options] <command> [command-options]\n", pszProcName);
    RTStrmPrintf(pStrm,
                 "\n"
                 "Global Options:\n"
                 "  -h, -?, --help\n"
                 "    Displays help\n"
                 "  -l | --logfile <file>\n"
                 "    Enables logging to a file\n"
                 "  -v, --verbose\n"
                 "    Increase verbosity\n"
                 "  -V, --version\n"
                 "    Displays version\n"
                 );

    for (uintptr_t iCmd = 0; iCmd < RT_ELEMENTS(g_apCommands); iCmd++)
    {
        PCVBOXDRVINSTCMD const pCmd = g_apCommands[iCmd];
        if (!pOnlyCmd || pCmd == pOnlyCmd)
        {
            RTStrmPrintf(pStrm,
                         "\n"
                         "Command '%s':\n"
                         "    %s\n",
                         pCmd->pszCommand, pCmd->pszDesc);

            if (!pCmd->paOptions)
                continue;

            RTStrmPrintf(pStrm, "Options for '%s':\n", pCmd->pszCommand);
            PCRTGETOPTDEF const paOptions = pCmd->paOptions;
            for (unsigned i = 0; i < pCmd->cOptions; i++)
            {
                if (RT_C_IS_PRINT(paOptions[i].iShort))
                    RTStrmPrintf(pStrm, "  -%c, %s\n", paOptions[i].iShort, paOptions[i].pszLong);
                else
                    RTStrmPrintf(pStrm, "  %s\n", paOptions[i].pszLong);

                const char *pszHelp = NULL;
                if (pCmd->pfnOptionHelp)
                    pszHelp = pCmd->pfnOptionHelp(&paOptions[i]);
                if (pszHelp)
                    RTStrmPrintf(pStrm, "    %s\n", pszHelp);
            }
        }
    }

    RTStrmPrintf(pStrm, "\nExamples:\n");
    RTStrmPrintf(pStrm, "\t%s install   --inf-file C:\\Path\\To\\VBoxUSB.inf\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s install   --debug-os-ver 6:0 --inf-file C:\\Path\\To\\VBoxGuest.inf\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s uninstall host\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s uninstall --inf -file C:\\Path\\To\\VBoxUSB.inf --pnp-id \"USB\\VID_80EE&PID_CAFE\"\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s uninstall --model \"VBoxUSB.AMD64\"\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s uninstall --model \"VBoxUSB*\"\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s service   VBoxSDS stop\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s service   VBoxSDS start --no-wait\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s service   VBoxSDS restart --wait 180\n", pszProcName);
    RTStrmPrintf(pStrm, "\t%s list      \"VBox*\"\n\n", pszProcName);
    RTStrmPrintf(pStrm, "Exit codes:\n");
    RTStrmPrintf(pStrm, "\t1 - The requested command failed.\n");
    RTStrmPrintf(pStrm, "\t2 - Syntax error.\n");
    RTStrmPrintf(pStrm, "\t5 - A reboot is needed in order to complete the (un)installation.\n\n");

    return RTEXITCODE_SUCCESS;
}

/**
 * Shows tool version.
 *
 * @returns RTEXITCODE
 * @param   pStrm               Stream to use.
 */
static RTEXITCODE vboxDrvInstShowVersion(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm, "%s\n", RTBldCfgRevisionStr());
    return RTEXITCODE_SUCCESS;
}

/**
 * Shows the logo.
 *
 * @param   pStream             Output stream to show logo on.
 */
static void vboxDrvInstShowLogo(PRTSTREAM pStream)
{
    RTStrmPrintf(pStream, VBOX_PRODUCT " VBoxDrvInst (Driver Installation Utility) Version " VBOX_VERSION_STRING " - r%s (%s)\n"
                 "Copyright (C) " VBOX_C_YEAR " " VBOX_VENDOR "\n\n", RTBldCfgRevisionStr(), RTBldCfgTargetArch());
}

/**
 * @callback_method_impl{FNRTLOGPHASE, Release logger callback}
 */
static DECLCALLBACK(void) vboxDrvInstLogHeaderFooter(PRTLOGGER pLoggerRelease, RTLOGPHASE enmPhase, PFNRTLOGPHASEMSG pfnLog)
{
    /* Some introductory information. */
    static RTTIMESPEC s_TimeSpec;
    char szTmp[256];
    if (enmPhase == RTLOGPHASE_BEGIN)
        RTTimeNow(&s_TimeSpec);
    RTTimeSpecToString(&s_TimeSpec, szTmp, sizeof(szTmp));

    switch (enmPhase)
    {
        case RTLOGPHASE_BEGIN:
        {
            pfnLog(pLoggerRelease,
                   "VBoxDrvInst %s r%s (verbosity: %u) (%s %s) release log (%s)\n"
                   "Log opened %s\n",
                   RTBldCfgVersion(), RTBldCfgRevisionStr(), g_uVerbosity,
                   __DATE__, __TIME__, RTBldCfgTargetArch(), szTmp);

            int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Product: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Release: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Version: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Service Pack: %s\n", szTmp);

            /* the package type is interesting for Linux distributions */
            char szExecName[RTPATH_MAX];
            char *pszExecName = RTProcGetExecutablePath(szExecName, sizeof(szExecName));
            pfnLog(pLoggerRelease,
                   "Executable: %s\n"
                   "Process ID: %u\n"
                   "Package type: %s"
#ifdef VBOX_OSE
                   " (OSE)"
#endif
                   "\n",
                   pszExecName ? pszExecName : "unknown",
                   RTProcSelf(),
                   VBOX_PACKAGE_STRING);
            break;
        }

        case RTLOGPHASE_PREROTATE:
            pfnLog(pLoggerRelease, "Log rotated - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_POSTROTATE:
            pfnLog(pLoggerRelease, "Log continuation - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_END:
            pfnLog(pLoggerRelease, "End of log file - Log started %s\n", szTmp);
            break;

        default:
            /* nothing */
            break;
    }
}


/**
 * Creates the default release logger outputting to the specified file.
 *
 * @return  IPRT status code.
 * @param   pszLogFile      Filename for log output.
 */
static int vboxDrvInstLogCreate(const char *pszLogFile)
{
    /* Create release logger (stdout + file). */
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    RTUINT fFlags = RTLOGFLAGS_USECRLF | RTLOGFLAGS_APPEND;
    int rc = RTLogCreateEx(&g_pLoggerRelease, "VBOXDRVINST_RELEASE_LOG", fFlags, "all",
                           RT_ELEMENTS(s_apszGroups), s_apszGroups, UINT32_MAX /*cMaxEntriesPerGroup*/,
                           0 /*cBufDescs*/, NULL /*paBufDescs*/, RTLOGDEST_STDOUT | RTLOGDEST_USER,
                           vboxDrvInstLogHeaderFooter, g_cHistory, g_uHistoryFileSize, g_uHistoryFileTime,
                           NULL /*pOutputIf*/, NULL /*pvOutputIfUser*/,
                           NULL /*pErrInfo*/, "%s", pszLogFile ? pszLogFile : "");
    if (RT_SUCCESS(rc))
    {
        /* register this logger as the release logger */
        RTLogRelSetDefaultInstance(g_pLoggerRelease);

        /* Explicitly flush the log in case of VBOXDRVINST_RELEASE_LOG=buffered. */
        RTLogFlush(g_pLoggerRelease);
    }

    return rc;
}

/**
 * Destroys the currently active logging instance.
 */
static void vboxDrvInstLogDestroy(void)
{
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
}

/**
 * Performs initialization tasks before a specific command is being run.
 *
 * @returns VBox status code.
 */
static int vboxDrvInstInit(void)
{
    int rc = vboxDrvInstLogCreate(g_szLogFile[0] ? g_szLogFile : NULL);
    if (RT_FAILURE(rc))
    {
        RTMsgError("Failed to create release log '%s', rc=%Rrc\n", g_szLogFile[0] ? g_szLogFile : "<None>", rc);
        return rc;
    }

    /* Refuse to run on too old Windows versions (<= NT4). */
    uint64_t const uNtVer = RTSystemGetNtVersion();
    if (RTSYSTEM_NT_VERSION_GET_MAJOR(uNtVer) <= 4)
    {
        vboxDrvInstLogError("Windows version (%d.%d.%d) too old and not supported\n", RTSYSTEM_NT_VERSION_GET_MAJOR(uNtVer),
                                                                                      RTSYSTEM_NT_VERSION_GET_MINOR(uNtVer),
                                                                                      RTSYSTEM_NT_VERSION_GET_BUILD(uNtVer));
        return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

/**
 * Performs destruction tasks after a specific command has been run.
 */
static void vboxDrvInstDestroy(void)
{
    vboxDrvInstLogDestroy();
}

int main(int argc, char **argv)
{
    /*
     * Init IPRT.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    vboxDrvInstShowLogo(g_pStdOut);

    /*
     * Process common options.
     */
    RTGETOPTSTATE GetState;
    RT_ZERO(GetState);
    rc = RTGetOptInit(&GetState, argc, argv, g_aCmdCommonOptions, RT_ELEMENTS(g_aCmdCommonOptions),
                      1 /*idxFirst*/, 0 /*fFlags - must not sort! */);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    int           ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (ch)
        {
            case 'h':
                return vboxDrvInstShowUsage(g_pStdOut, NULL);

            case 'l':
                rc = RTStrCopy(g_szLogFile, sizeof(g_szLogFile), ValueUnion.psz);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExitFailure("Error setting logfile, rc=%Rrc\n", rc);
                break;

            case 'v':
                g_uVerbosity++;
                break;

            case 'V':
                return vboxDrvInstShowVersion(g_pStdOut);

            case VERR_GETOPT_UNKNOWN_OPTION:
                return vboxDrvInstShowUsage(g_pStdOut, NULL);

            case VINF_GETOPT_NOT_OPTION:
            {
                for (uintptr_t iCmd = 0; iCmd < RT_ELEMENTS(g_apCommands); iCmd++)
                {
                    PCVBOXDRVINSTCMD const pCmd = g_apCommands[iCmd];
                    if (strcmp(ValueUnion.psz, pCmd->pszCommand) == 0)
                    {
                        rc = vboxDrvInstInit();
                        if (RT_FAILURE(rc))
                            return RTEXITCODE_FAILURE;

                        /* Count the combined option definitions:  */
                        size_t cCombinedOptions  = pCmd->cOptions + RT_ELEMENTS(g_aCmdCommonOptions);

                        RTEXITCODE rcExit;

                        /* Combine the option definitions: */
                        PRTGETOPTDEF paCombinedOptions = (PRTGETOPTDEF)RTMemAlloc(cCombinedOptions * sizeof(RTGETOPTDEF));
                        if (paCombinedOptions)
                        {
                            uint32_t idxOpts = 0;
                            memcpy(paCombinedOptions, g_aCmdCommonOptions, sizeof(g_aCmdCommonOptions));
                            idxOpts += RT_ELEMENTS(g_aCmdCommonOptions);

                            memcpy(&paCombinedOptions[idxOpts], pCmd->paOptions, pCmd->cOptions * sizeof(RTGETOPTDEF));
                            idxOpts += (uint32_t)pCmd->cOptions;

                            /* Re-initialize the option getter state and pass it to the command handler. */
                            rc = RTGetOptInit(&GetState, argc, argv, paCombinedOptions, cCombinedOptions,
                                              GetState.iNext /*idxFirst*/, RTGETOPTINIT_FLAGS_OPTS_FIRST);

                            if (RT_SUCCESS(rc))
                                rcExit = pCmd->pfnHandler(&GetState);
                             else
                                rcExit = RTMsgErrorExitFailure("RTGetOptInit failed for '%s': %Rrc", ValueUnion.psz, rc);
                            RTMemFree(paCombinedOptions);
                        }
                        else
                            rcExit = RTMsgErrorExitFailure("Out of memory!");

                        vboxDrvInstDestroy();
                        return rcExit;
                    }
                }
                RTMsgError("Unknown command '%s'!\n", ValueUnion.psz);
                vboxDrvInstShowCommands(g_pStdErr);
                return RTEXITCODE_SYNTAX;
            }

            default:
                break;
        }
    }

    /* List all Windows driver store entries if no command is given. */
    rc = vboxDrvInstInit();
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;
    RTEXITCODE rcExit = vboxDrvInstCmdListMain(&GetState);
    vboxDrvInstDestroy();
    return rcExit;
}

