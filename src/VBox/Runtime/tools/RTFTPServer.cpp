/* $Id$ */
/** @file
 * IPRT - Utility for running a (simple) FTP server.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <signal.h>

#include <iprt/ftp.h>

#include <iprt/net.h> /* To make use of IPv4Addr in RTGETOPTUNION. */

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/errcore.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/vfs.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Set by the signal handler when the FTP server shall be terminated. */
static volatile bool g_fCanceled = false;


#ifdef RT_OS_WINDOWS
static BOOL WINAPI signalHandler(DWORD dwCtrlType)
{
    bool fEventHandled = FALSE;
    switch (dwCtrlType)
    {
        /* User pressed CTRL+C or CTRL+BREAK or an external event was sent
         * via GenerateConsoleCtrlEvent(). */
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_C_EVENT:
            ASMAtomicWriteBool(&g_fCanceled, true);
            fEventHandled = TRUE;
            break;
        default:
            break;
        /** @todo Add other events here. */
    }

    return fEventHandled;
}
#else /* !RT_OS_WINDOWS */
/**
 * Signal handler that sets g_fCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Don't do anything
 * unnecessary here.
 */
static void signalHandler(int iSignal)
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fCanceled, true);
}
#endif

/**
 * Installs a custom signal handler to get notified
 * whenever the user wants to intercept the program.
 *
 * @todo Make this handler available for all VBoxManage modules?
 */
static int signalHandlerInstall(void)
{
    g_fCanceled = false;

    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)signalHandler, TRUE /* Add handler */))
    {
        rc = RTErrConvertFromWin32(GetLastError());
        RTMsgError("Unable to install console control handler, rc=%Rrc\n", rc);
    }
#else
    signal(SIGINT,   signalHandler);
    signal(SIGTERM,  signalHandler);
# ifdef SIGBREAK
    signal(SIGBREAK, signalHandler);
# endif
#endif
    return rc;
}

/**
 * Uninstalls a previously installed signal handler.
 */
static int signalHandlerUninstall(void)
{
    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)NULL, FALSE /* Remove handler */))
    {
        rc = RTErrConvertFromWin32(GetLastError());
        RTMsgError("Unable to uninstall console control handler, rc=%Rrc\n", rc);
    }
#else
    signal(SIGINT,   SIG_DFL);
    signal(SIGTERM,  SIG_DFL);
# ifdef SIGBREAK
    signal(SIGBREAK, SIG_DFL);
# endif
#endif
    return rc;
}

int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /* Use some sane defaults. */
    char     szAddress[64]         = "localhost";
    uint16_t uPort                 = 2121;

    /* By default use the current directory as serving root directory. */
    char     szRootDir[RTPATH_MAX];
    rc = RTPathGetCurrent(szRootDir, sizeof(szRootDir));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Retrieving current directory failed: %Rrc", rc);

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--address",      'a', RTGETOPT_REQ_IPV4ADDR }, /** @todo Use a string for DNS hostnames? */
        /** @todo Implement IPv6 support? */
        { "--port",         'p', RTGETOPT_REQ_UINT16 },
        { "--root-dir",     'r', RTGETOPT_REQ_STRING },
        { "--verbose",      'v', RTGETOPT_REQ_NOTHING }
    };

    RTEXITCODE      rcExit          = RTEXITCODE_SUCCESS;
    unsigned        uVerbosityLevel = 1;

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (rc)
        {
            case 'a':
                RTStrPrintf2(szAddress, sizeof(szAddress), "%RU8.%RU8.%RU8.%RU8", /** @todo Improve this. */
                             ValueUnion.IPv4Addr.au8[0], ValueUnion.IPv4Addr.au8[1], ValueUnion.IPv4Addr.au8[2], ValueUnion.IPv4Addr.au8[3]);
                break;

            case 'p':
                uPort = ValueUnion.u16;
                break;

            case 'v':
                uVerbosityLevel++;
                break;

            case 'h':
                RTPrintf("Usage: %s [options]\n"
                         "\n"
                         "Options:\n"
                         "  -a, --address (default: localhost)\n"
                         "      Specifies the address to use for listening.\n"
                         "  -p, --port (default: 2121)\n"
                         "      Specifies the port to use for listening.\n"
                         "  -r, --root-dir (default: current dir)\n"
                         "      Specifies the root directory being served.\n"
                         "  -v, --verbose\n"
                         "      Controls the verbosity level.\n"
                         "  -h, -?, --help\n"
                         "      Display this help text and exit successfully.\n"
                         "  -V, --version\n"
                         "      Display the revision and exit successfully.\n"
                         , RTPathFilename(argv[0]));
                return RTEXITCODE_SUCCESS;

            case 'V':
                RTPrintf("$Revision$\n");
                return RTEXITCODE_SUCCESS;

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /* Install signal handler. */
    rc = signalHandlerInstall();
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the FTP server instance.
         */
        RTFTPSERVER hFTPServer;
        rc = RTFTPServerCreate(&hFTPServer, szAddress, uPort, szRootDir);
        if (RT_SUCCESS(rc))
        {
            RTPrintf("Starting FTP server at %s:%RU16 ...\n", szAddress, uPort);
            RTPrintf("Root directory is '%s'\n", szRootDir);

            RTPrintf("Running FTP server ...\n");

            for (;;)
            {
                RTThreadSleep(200);

                if (g_fCanceled)
                    break;
            }

            RTPrintf("Stopping FTP server ...\n");

            int rc2 = RTFTPServerDestroy(hFTPServer);
            if (RT_SUCCESS(rc))
                rc = rc2;

            RTPrintf("Stopped FTP server\n");
        }
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTFTPServerCreate failed: %Rrc", rc);

        int rc2 = signalHandlerUninstall();
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    /* Set rcExit on failure in case we forgot to do so before. */
    if (RT_FAILURE(rc))
        rcExit = RTEXITCODE_FAILURE;

    return rcExit;
}

