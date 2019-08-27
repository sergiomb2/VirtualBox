/* $Id$ */
/** @file
 * Shared Clipboard Service - Mac OS X host.
 */

/*
 * Copyright (C) 2008-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/thread.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "darwin-pasteboard.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Global clipboard context information */
struct _VBOXCLIPBOARDCONTEXT
{
    /** We have a separate thread to poll for new clipboard content */
    RTTHREAD thread;
    bool volatile fTerminate;

    /** The reference to the current pasteboard */
    PasteboardRef pasteboard;

    PVBOXCLIPBOARDCLIENT pClient;
};


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Only one client is supported. There seems to be no need for more clients. */
static VBOXCLIPBOARDCONTEXT g_ctx;


/**
 * Checks if something is present on the clipboard and calls vboxSvcClipboardReportMsg.
 *
 * @returns IPRT status code (ignored).
 * @param   pCtx    The context.
 */
static int vboxClipboardChanged(VBOXCLIPBOARDCONTEXT *pCtx)
{
    if (pCtx->pClient == NULL)
        return VINF_SUCCESS;

    uint32_t fFormats = 0;
    bool fChanged = false;
    /* Retrieve the formats currently in the clipboard and supported by vbox */
    int rc = queryNewPasteboardFormats(pCtx->pasteboard, &fFormats, &fChanged);
    if (RT_SUCCESS(rc) && fChanged)
    {
        vboxSvcClipboardOldReportMsg(pCtx->pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS_WRITE, fFormats);
        Log(("vboxClipboardChanged fFormats %02X\n", fFormats));
    }

    return rc;
}


/**
 * The poller thread.
 *
 * This thread will check for the arrival of new data on the clipboard.
 *
 * @returns VINF_SUCCESS (not used).
 * @param   ThreadSelf  Our thread handle.
 * @param   pvUser      Pointer to the VBOXCLIPBOARDCONTEXT structure.
 *
 */
static int vboxClipboardThread(RTTHREAD ThreadSelf, void *pvUser)
{
    Log(("vboxClipboardThread: starting clipboard thread\n"));

    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    VBOXCLIPBOARDCONTEXT *pCtx = (VBOXCLIPBOARDCONTEXT *)pvUser;

    while (!pCtx->fTerminate)
    {
        /* call this behind the lock because we don't know if the api is
           thread safe and in any case we're calling several methods. */
        VBoxSvcClipboardLock();
        vboxClipboardChanged(pCtx);
        VBoxSvcClipboardUnlock();

        /* Sleep for 200 msecs before next poll */
        RTThreadUserWait(ThreadSelf, 200);
    }

    Log(("vboxClipboardThread: clipboard thread terminated successfully with return code %Rrc\n", VINF_SUCCESS));
    return VINF_SUCCESS;
}

/*
 * Public platform dependent functions.
 */

/** Initialise the host side of the shared clipboard - called by the hgcm layer. */
int VBoxClipboardSvcImplInit(void)
{
    Log(("vboxClipboardInit\n"));

    g_ctx.fTerminate = false;

    int rc = initPasteboard(&g_ctx.pasteboard);
    AssertRCReturn(rc, rc);

    rc = RTThreadCreate(&g_ctx.thread, vboxClipboardThread, &g_ctx, 0,
                        RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
    if (RT_FAILURE(rc))
    {
        g_ctx.thread = NIL_RTTHREAD;
        destroyPasteboard(&g_ctx.pasteboard);
    }

    return rc;
}

/** Terminate the host side of the shared clipboard - called by the hgcm layer. */
void VBoxClipboardSvcImplDestroy(void)
{
    Log(("vboxClipboardDestroy\n"));

    /*
     * Signal the termination of the polling thread and wait for it to respond.
     */
    ASMAtomicWriteBool(&g_ctx.fTerminate, true);
    int rc = RTThreadUserSignal(g_ctx.thread);
    AssertRC(rc);
    rc = RTThreadWait(g_ctx.thread, RT_INDEFINITE_WAIT, NULL);
    AssertRC(rc);

    /*
     * Destroy the pasteboard and uninitialize the global context record.
     */
    destroyPasteboard(&g_ctx.pasteboard);
    g_ctx.thread = NIL_RTTHREAD;
    g_ctx.pClient = NULL;
}

int VBoxClipboardSvcImplConnect(PVBOXCLIPBOARDCLIENT pClient, bool fHeadless)
{
    RT_NOREF(fHeadless);

    if (g_ctx.pClient != NULL)
    {
        /* One client only. */
        return VERR_NOT_SUPPORTED;
    }

    VBoxSvcClipboardLock();

    pClient->State.pCtx = &g_ctx;
    pClient->State.pCtx->pClient = pClient;

    /* Initially sync the host clipboard content with the client. */
    int rc = VBoxClipboardSvcImplSync(pClient);

    VBoxSvcClipboardUnlock();
    return rc;
}

int VBoxClipboardSvcImplSync(PVBOXCLIPBOARDCLIENT pClient)
{
    /* Sync the host clipboard content with the client. */
    VBoxSvcClipboardLock();
    int rc = vboxClipboardChanged(pClient->State.pCtx);
    VBoxSvcClipboardUnlock();

    return rc;
}

int VBoxClipboardSvcImplDisconnect(PVBOXCLIPBOARDCLIENT pClient)
{
    VBoxSvcClipboardLock();
    pClient->State.pCtx->pClient = NULL;
    VBoxSvcClipboardUnlock();

    return VINF_SUCCESS;
}

int VBoxClipboardSvcImplFormatAnnounce(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx,
                                       PSHAREDCLIPBOARDFORMATDATA pFormats)
{
    RT_NOREF(pCmdCtx);

    LogFlowFunc(("uFormats=%02X\n", pFormats->uFormats));

    if (pFormats->uFormats == 0)
    {
        /* This is just an automatism, not a genuine announcement */
        return VINF_SUCCESS;
    }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    if (pFormats->uFormats & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST) /* No URI support yet. */
        return VINF_SUCCESS;
#endif

    return vboxSvcClipboardOldReportMsg(pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA, pFormats->uFormats);
}

/**
 * Called by the HGCM clipboard subsystem when the guest wants to read the host clipboard.
 *
 * @param pClient               Context information about the guest VM.
 * @param pCmdCtx               Command context to use for reading the data. Currently unused.
 * @param pData                 Data block to put read data into.
 * @param pcbActual             Where to write the actual size of the written data.
 */
int VBoxClipboardSvcImplReadData(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx,
                                 PSHAREDCLIPBOARDDATABLOCK pData, uint32_t *pcbActual)
{
    RT_NOREF(pCmdCtx);

    VBoxSvcClipboardLock();

    /* Default to no data available. */
    *pcbActual = 0;

    int rc = readFromPasteboard(pClient->State.pCtx->pasteboard,
                                pData->uFormat, pData->pvData, pData->cbData, pcbActual);

    VBoxSvcClipboardUnlock();

    return rc;
}

/**
 * Called by the HGCM clipboard subsystem when we have requested data and that data arrives.
 *
 *
 * @param pClient               Context information about the guest VM.
 * @param pCmdCtx               Command context to use for writing the data. Currently unused.
 * @param pData                 Data block to write to clipboard.
 */
int VBoxClipboardSvcImplWriteData(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx, PSHAREDCLIPBOARDDATABLOCK pData)
{
    RT_NOREF(pCmdCtx);

    VBoxSvcClipboardLock();

    writeToPasteboard(pClient->State.pCtx->pasteboard, pData->pvData, pData->cbData, pData->uFormat);

    VBoxSvcClipboardUnlock();

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int VBoxClipboardSvcImplURIReadDir(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pClient, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteDir(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pClient, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIReadFileHdr(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pClient, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteFileHdr(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pClient, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIReadFileData(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDFILEDATA pFileData)
{
    RT_NOREF(pClient, pFileData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteFileData(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDFILEDATA pFileData)
{
    RT_NOREF(pClient, pFileData);
    return VERR_NOT_IMPLEMENTED;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

