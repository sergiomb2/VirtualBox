/* $Id$ */
/** @file
 * Shared Clipboard Service - Internal code for transfer (list) handling.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#include <VBox/log.h>

#include <VBox/err.h>

#include <VBox/GuestHost/clipboard-helper.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/HostServices/VBoxClipboardExt.h>

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "VBoxSharedClipboardSvc-transfers.h"


/*********************************************************************************************************************************
*   Externals                                                                                                                    *
*********************************************************************************************************************************/
extern uint32_t             g_fTransferMode;
extern SHCLEXTSTATE         g_ExtState;
extern PVBOXHGCMSVCHELPERS  g_pHelpers;
extern ClipboardClientMap   g_mapClients;
extern ClipboardClientQueue g_listClientsDeferred;


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
static int shClSvcTransferSetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                      PSHCLMSGCTX pMsgCtx, PSHCLLISTOPENPARMS pOpenParms);
static int shClSvcTransferSetListClose(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                       PSHCLMSGCTX pMsgCtx, SHCLLISTHANDLE hList);


/*********************************************************************************************************************************
*   Provider implementation                                                                                                      *
*********************************************************************************************************************************/

/**
 * Resets all transfers of a Shared Clipboard client.
 *
 * @param   pClient             Client to reset transfers for.
 */
void shClSvcClientTransfersReset(PSHCLCLIENT pClient)
{
    if (!pClient)
        return;

    LogFlowFuncEnter();

    const uint32_t cTransfers = ShClTransferCtxGetTotalTransfers(&pClient->TransferCtx);
    for (uint32_t i = 0; i < cTransfers; i++)
    {
        PSHCLTRANSFER pTransfer = ShClTransferCtxGetTransfer(&pClient->TransferCtx, i);
        if (pTransfer)
            shClSvcTransferAreaDetach(&pClient->State, pTransfer);
    }

    ShClTransferCtxDestroy(&pClient->TransferCtx);
}


/*********************************************************************************************************************************
*   Provider implementation                                                                                                      *
*********************************************************************************************************************************/

DECLCALLBACK(int) shClSvcTransferIfaceOpen(PSHCLPROVIDERCTX pCtx)
{
    LogFlowFuncEnter();

    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

DECLCALLBACK(int) shClSvcTransferIfaceClose(PSHCLPROVIDERCTX pCtx)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc = shClSvcTransferStop(pClient, pCtx->pTransfer);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) shClSvcTransferIfaceGetRoots(PSHCLPROVIDERCTX pCtx, PSHCLROOTLIST *ppRootList)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsgHdr = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_HDR_READ,
                                             VBOX_SHCL_CPARMS_ROOT_LIST_HDR_READ_REQ);
    if (pMsgHdr)
    {
        SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU64(&pMsgHdr->paParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                     pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU32(&pMsgHdr->paParms[1], 0 /* fRoots */);

        rc = shClSvcMsgAdd(pClient, pMsgHdr, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = shClSvcClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayloadHdr;
                rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent,
                                              pCtx->pTransfer->uTimeoutMs, &pPayloadHdr);
                if (RT_SUCCESS(rc))
                {
                    PSHCLROOTLISTHDR pSrcRootListHdr = (PSHCLROOTLISTHDR)pPayloadHdr->pvData;
                    Assert(pPayloadHdr->cbData == sizeof(SHCLROOTLISTHDR));

                    LogFlowFunc(("cRoots=%RU32, fRoots=0x%x\n", pSrcRootListHdr->cRoots, pSrcRootListHdr->fRoots));

                    PSHCLROOTLIST pRootList = ShClTransferRootListAlloc();
                    if (pRootList)
                    {
                        if (pSrcRootListHdr->cRoots)
                        {
                            pRootList->paEntries =
                                (PSHCLROOTLISTENTRY)RTMemAllocZ(pSrcRootListHdr->cRoots * sizeof(SHCLROOTLISTENTRY));

                            if (pRootList->paEntries)
                            {
                                for (uint32_t i = 0; i < pSrcRootListHdr->cRoots; i++)
                                {
                                    PSHCLCLIENTMSG pMsgEntry = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_ENTRY_READ,
                                                                               VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_READ_REQ);

                                    uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

                                    HGCMSvcSetU64(&pMsgEntry->paParms[0],
                                                  VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uClientID,
                                                                                       pCtx->pTransfer->State.uID, uEvent));
                                    HGCMSvcSetU32(&pMsgEntry->paParms[1], 0 /* fRoots */);
                                    HGCMSvcSetU32(&pMsgEntry->paParms[2], i /* uIndex */);

                                    rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
                                    AssertRC(rc2);

                                    rc = shClSvcMsgAdd(pClient, pMsgEntry, true /* fAppend */);
                                    if (RT_FAILURE(rc))
                                        break;

                                    PSHCLEVENTPAYLOAD pPayloadEntry;
                                    rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent,
                                                       pCtx->pTransfer->uTimeoutMs, &pPayloadEntry);
                                    if (RT_FAILURE(rc))
                                        break;

                                    PSHCLROOTLISTENTRY pSrcRootListEntry = (PSHCLROOTLISTENTRY)pPayloadEntry->pvData;
                                    Assert(pPayloadEntry->cbData == sizeof(SHCLROOTLISTENTRY));

                                    rc = ShClTransferListEntryCopy(&pRootList->paEntries[i], pSrcRootListEntry);

                                    ShClPayloadFree(pPayloadEntry);

                                    ShClEventUnregister(&pCtx->pTransfer->Events, uEvent);

                                    if (RT_FAILURE(rc))
                                        break;
                                }
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }

                        if (RT_SUCCESS(rc))
                        {
                            pRootList->Hdr.cRoots = pSrcRootListHdr->cRoots;
                            pRootList->Hdr.fRoots = 0; /** @todo Implement this. */

                            *ppRootList = pRootList;
                        }
                        else
                            ShClTransferRootListFree(pRootList);

                        ShClPayloadFree(pPayloadHdr);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
            }

            ShClEventUnregister(&pCtx->pTransfer->Events, uEvent);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeave();
    return rc;
}

DECLCALLBACK(int) shClSvcTransferIfaceListOpen(PSHCLPROVIDERCTX pCtx,
                                               PSHCLLISTOPENPARMS pOpenParms, PSHCLLISTHANDLE phList)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_OPEN,
                                          VBOX_SHCL_CPARMS_LIST_OPEN);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        pMsg->Ctx.uContextID = VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID, pCtx->pTransfer->State.uID,
                                                        uEvent);

        rc = shClSvcTransferSetListOpen(pMsg->cParms, pMsg->paParms, &pMsg->Ctx, pOpenParms);
        if (RT_SUCCESS(rc))
        {
            rc = shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
            if (RT_SUCCESS(rc))
            {
                int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
                AssertRC(rc2);

                rc = shClSvcClientWakeup(pClient);
                if (RT_SUCCESS(rc))
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        Assert(pPayload->cbData == sizeof(SHCLREPLY));

                        PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                        AssertPtr(pReply);

                        Assert(pReply->uType == VBOX_SHCL_REPLYMSGTYPE_LIST_OPEN);

                        LogFlowFunc(("hList=%RU64\n", pReply->u.ListOpen.uHandle));

                        *phList = pReply->u.ListOpen.uHandle;

                        ShClPayloadFree(pPayload);
                    }
                }

                ShClEventUnregister(&pCtx->pTransfer->Events, uEvent);
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) shClSvcTransferIfaceListClose(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_CLOSE,
                                          VBOX_SHCL_CPARMS_LIST_CLOSE);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        pMsg->Ctx.uContextID = VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID, pCtx->pTransfer->State.uID,
                                                        uEvent);

        rc = shClSvcTransferSetListClose(pMsg->cParms, pMsg->paParms, &pMsg->Ctx, hList);
        if (RT_SUCCESS(rc))
        {
            rc = shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
            if (RT_SUCCESS(rc))
            {
                int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
                AssertRC(rc2);

                rc = shClSvcClientWakeup(pClient);
                if (RT_SUCCESS(rc))
                {
                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                    if (RT_SUCCESS(rc))
                        ShClPayloadFree(pPayload);
                }

                ShClEventUnregister(&pCtx->pTransfer->Events, uEvent);
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) shClSvcTransferIfaceListHdrRead(PSHCLPROVIDERCTX pCtx,
                                                  SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_HDR_READ,
                                          VBOX_SHCL_CPARMS_LIST_HDR_READ_REQ);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU64(&pMsg->paParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                  pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->paParms[1], hList);
        HGCMSvcSetU32(&pMsg->paParms[2], 0 /* fFlags */);

        rc = shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = shClSvcClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent,
                                   pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLLISTHDR));

                    *pListHdr = *(PSHCLLISTHDR)pPayload->pvData;

                    ShClPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) shClSvcTransferIfaceListHdrWrite(PSHCLPROVIDERCTX pCtx,
                                                   SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr)
{
    RT_NOREF(pCtx, hList, pListHdr);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

DECLCALLBACK(int) shClSvcTransferIfaceListEntryRead(PSHCLPROVIDERCTX pCtx,
                                                    SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_ENTRY_READ,
                                          VBOX_SHCL_CPARMS_LIST_ENTRY_READ);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU64(&pMsg->paParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                  pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->paParms[1], hList);
        HGCMSvcSetU32(&pMsg->paParms[2], 0 /* fInfo */);

        rc = shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = shClSvcClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLLISTENTRY));

                    rc = ShClTransferListEntryCopy(pListEntry, (PSHCLLISTENTRY)pPayload->pvData);

                    ShClPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) shClSvcTransferIfaceListEntryWrite(PSHCLPROVIDERCTX pCtx,
                                                     SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry)
{
    RT_NOREF(pCtx, hList, pListEntry);

    LogFlowFuncEnter();

    return VERR_NOT_IMPLEMENTED;
}

int shClSvcTransferIfaceObjOpen(PSHCLPROVIDERCTX pCtx, PSHCLOBJOPENCREATEPARMS pCreateParms,
                                PSHCLOBJHANDLE phObj)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_OPEN,
                                          VBOX_SHCL_CPARMS_OBJ_OPEN);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        LogFlowFunc(("pszPath=%s, fCreate=0x%x\n", pCreateParms->pszPath, pCreateParms->fCreate));

        const uint32_t cbPath = (uint32_t)strlen(pCreateParms->pszPath) + 1; /* Include terminating zero */

        HGCMSvcSetU64(&pMsg->paParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                  pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->paParms[1], 0); /* uHandle */
        HGCMSvcSetU32(&pMsg->paParms[2], cbPath);
        HGCMSvcSetPv (&pMsg->paParms[3], pCreateParms->pszPath, cbPath);
        HGCMSvcSetU32(&pMsg->paParms[4], pCreateParms->fCreate);

        rc = shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = shClSvcClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLREPLY));

                    PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                    AssertPtr(pReply);

                    Assert(pReply->uType == VBOX_SHCL_REPLYMSGTYPE_OBJ_OPEN);

                    LogFlowFunc(("hObj=%RU64\n", pReply->u.ObjOpen.uHandle));

                    *phObj = pReply->u.ObjOpen.uHandle;

                    ShClPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int shClSvcTransferIfaceObjClose(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_CLOSE,
                                          VBOX_SHCL_CPARMS_OBJ_CLOSE);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU64(&pMsg->paParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                  pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->paParms[1], hObj);

        rc = shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = shClSvcClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLREPLY));

#ifdef VBOX_STRICT
                    PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                    AssertPtr(pReply);

                    Assert(pReply->uType == VBOX_SHCL_REPLYMSGTYPE_OBJ_CLOSE);

                    LogFlowFunc(("hObj=%RU64\n", pReply->u.ObjClose.uHandle));
#endif

                    ShClPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int shClSvcTransferIfaceObjRead(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_READ,
                                          VBOX_SHCL_CPARMS_OBJ_READ_REQ);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU64(&pMsg->paParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                  pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->paParms[1], hObj);
        HGCMSvcSetU32(&pMsg->paParms[2], cbData);
        HGCMSvcSetU32(&pMsg->paParms[3], fFlags);

        rc = shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = shClSvcClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    Assert(pPayload->cbData == sizeof(SHCLOBJDATACHUNK));

                    PSHCLOBJDATACHUNK pDataChunk = (PSHCLOBJDATACHUNK)pPayload->pvData;
                    AssertPtr(pDataChunk);

                    const uint32_t cbRead = RT_MIN(cbData, pDataChunk->cbData);

                    memcpy(pvData, pDataChunk->pvData, cbRead);

                    if (pcbRead)
                        *pcbRead = cbRead;

                    ShClPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int shClSvcTransferIfaceObjWrite(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                 void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten)
{
    LogFlowFuncEnter();

    PSHCLCLIENT pClient = (PSHCLCLIENT)pCtx->pvUser;
    AssertPtr(pClient);

    int rc;

    PSHCLCLIENTMSG pMsg = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_WRITE,
                                          VBOX_SHCL_CPARMS_OBJ_WRITE);
    if (pMsg)
    {
        const SHCLEVENTID uEvent = ShClEventIDGenerate(&pCtx->pTransfer->Events);

        HGCMSvcSetU64(&pMsg->paParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                  pCtx->pTransfer->State.uID, uEvent));
        HGCMSvcSetU64(&pMsg->paParms[1], hObj);
        HGCMSvcSetU64(&pMsg->paParms[2], cbData);
        HGCMSvcSetU64(&pMsg->paParms[3], fFlags);

        rc = shClSvcMsgAdd(pClient, pMsg, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = ShClEventRegister(&pCtx->pTransfer->Events, uEvent);
            AssertRC(rc2);

            rc = shClSvcClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pCtx->pTransfer->Events, uEvent, pCtx->pTransfer->uTimeoutMs, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    const uint32_t cbRead = RT_MIN(cbData, pPayload->cbData);

                    memcpy(pvData, pPayload->pvData, cbRead);

                    if (pcbWritten)
                        *pcbWritten = cbRead;

                    ShClPayloadFree(pPayload);
                }
            }
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/*********************************************************************************************************************************
*   HGCM getters / setters                                                                                                       *
*********************************************************************************************************************************/

/**
 * Returns whether a HGCM message is allowed in a certain service mode or not.
 *
 * @returns \c true if message is allowed, \c false if not.
 * @param   uMode               Service mode to check allowance for.
 * @param   uMsg                HGCM message to check allowance for.
 */
bool shClSvcTransferMsgIsAllowed(uint32_t uMode, uint32_t uMsg)
{
    const bool fHostToGuest =    uMode == VBOX_SHCL_MODE_HOST_TO_GUEST
                              || uMode == VBOX_SHCL_MODE_BIDIRECTIONAL;

    const bool fGuestToHost =    uMode == VBOX_SHCL_MODE_GUEST_TO_HOST
                              || uMode == VBOX_SHCL_MODE_BIDIRECTIONAL;

    bool fAllowed = false; /* If in doubt, don't allow. */

    switch (uMsg)
    {
        case VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_WRITE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_WRITE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_HDR_WRITE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_WRITE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_WRITE:
            fAllowed = fGuestToHost;
            break;

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_HDR_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_READ:
            fAllowed = fHostToGuest;
            break;

        case VBOX_SHCL_GUEST_FN_CONNECT:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_PEEK_WAIT:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_PEEK_NOWAIT:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_REPORT_FEATURES:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_QUERY_FEATURES:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_GET:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_REPLY:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_CANCEL:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_ERROR:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_OPEN:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_CLOSE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_OPEN:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_CLOSE:
            fAllowed = fHostToGuest || fGuestToHost;
            break;

        default:
            break;
    }

    LogFlowFunc(("uMsg=%RU32 (%s), uMode=%RU32 -> fAllowed=%RTbool\n", uMsg, ShClGuestMsgToStr(uMsg), uMode, fAllowed));
    return fAllowed;
}

/**
 * Gets a transfer message reply from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pReply              Where to store the reply.
 */
static int shClSvcTransferGetReply(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                   PSHCLREPLY pReply)
{
    int rc;

    if (cParms >= VBOX_SHCL_CPARMS_REPLY_MIN)
    {
        uint32_t cbPayload = 0;

        /* paParms[0] has the context ID. */
        rc = HGCMSvcGetU32(&paParms[1], &pReply->uType);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &pReply->rc);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[3], &cbPayload);
        if (RT_SUCCESS(rc))
        {
            rc = HGCMSvcGetPv(&paParms[4], &pReply->pvPayload, &pReply->cbPayload);
            AssertReturn(cbPayload == pReply->cbPayload, VERR_INVALID_PARAMETER);
        }

        if (RT_SUCCESS(rc))
        {
            rc = VERR_INVALID_PARAMETER; /* Play safe. */

            switch (pReply->uType)
            {
                case VBOX_SHCL_REPLYMSGTYPE_TRANSFER_STATUS:
                {
                    if (cParms >= 6)
                        rc = HGCMSvcGetU32(&paParms[5], &pReply->u.TransferStatus.uStatus);

                    LogFlowFunc(("uTransferStatus=%RU32\n", pReply->u.TransferStatus.uStatus));
                    break;
                }

                case VBOX_SHCL_REPLYMSGTYPE_LIST_OPEN:
                {
                    if (cParms >= 6)
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ListOpen.uHandle);

                    LogFlowFunc(("hListOpen=%RU64\n", pReply->u.ListOpen.uHandle));
                    break;
                }

                case VBOX_SHCL_REPLYMSGTYPE_LIST_CLOSE:
                {
                    if (cParms >= 6)
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ListClose.uHandle);

                    LogFlowFunc(("hListClose=%RU64\n", pReply->u.ListClose.uHandle));
                    break;
                }

                case VBOX_SHCL_REPLYMSGTYPE_OBJ_OPEN:
                {
                    if (cParms >= 6)
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ObjOpen.uHandle);

                    LogFlowFunc(("hObjOpen=%RU64\n", pReply->u.ObjOpen.uHandle));
                    break;
                }

                case VBOX_SHCL_REPLYMSGTYPE_OBJ_CLOSE:
                {
                    if (cParms >= 6)
                        rc = HGCMSvcGetU64(&paParms[5], &pReply->u.ObjClose.uHandle);

                    LogFlowFunc(("hObjClose=%RU64\n", pReply->u.ObjClose.uHandle));
                    break;
                }

                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer root list header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pRootLstHdr         Where to store the transfer root list header on success.
 */
static int shClSvcTransferGetRootListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                         PSHCLROOTLISTHDR pRootLstHdr)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_ROOT_LIST_HDR_WRITE)
    {
        rc = HGCMSvcGetU32(&paParms[1], &pRootLstHdr->fRoots);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &pRootLstHdr->cRoots);
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer root list entry from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pListEntry          Where to store the root list entry.
 */
static int shClSvcTransferGetRootListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                           PSHCLROOTLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_WRITE)
    {
        rc = HGCMSvcGetU32(&paParms[1], &pListEntry->fInfo);
        /* Note: paParms[2] contains the entry index, currently being ignored. */
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetPv(&paParms[3], (void **)&pListEntry->pszName, &pListEntry->cbName);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbInfo;
            rc = HGCMSvcGetU32(&paParms[4], &cbInfo);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&paParms[5], &pListEntry->pvInfo, &pListEntry->cbInfo);
                AssertReturn(cbInfo == pListEntry->cbInfo, VERR_INVALID_PARAMETER);
            }
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer list open request from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pOpenParms          Where to store the open parameters of the request.
 */
static int shClSvcTransferGetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                      PSHCLLISTOPENPARMS pOpenParms)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_OPEN)
    {
        uint32_t cbPath   = 0;
        uint32_t cbFilter = 0;

        rc = HGCMSvcGetU32(&paParms[1], &pOpenParms->fList);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &cbFilter);
        if (RT_SUCCESS(rc))
        {
            rc = HGCMSvcGetStr(&paParms[3], &pOpenParms->pszFilter, &pOpenParms->cbFilter);
            AssertReturn(cbFilter == pOpenParms->cbFilter, VERR_INVALID_PARAMETER);
        }
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[4], &cbPath);
        if (RT_SUCCESS(rc))
        {
            rc = HGCMSvcGetStr(&paParms[5], &pOpenParms->pszPath, &pOpenParms->cbPath);
            AssertReturn(cbPath == pOpenParms->cbPath, VERR_INVALID_PARAMETER);
        }

        /** @todo Some more validation. */
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets a transfer list open request to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pMsgCtx             Message context to use.
 * @param   pOpenParms          List open parameters to set.
 */
static int shClSvcTransferSetListOpen(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                      PSHCLMSGCTX pMsgCtx, PSHCLLISTOPENPARMS pOpenParms)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_OPEN)
    {
        HGCMSvcSetU64(&paParms[0], pMsgCtx->uContextID);
        HGCMSvcSetU32(&paParms[1], pOpenParms->fList);
        HGCMSvcSetU32(&paParms[2], pOpenParms->cbFilter);
        HGCMSvcSetPv (&paParms[3], pOpenParms->pszFilter, pOpenParms->cbFilter);
        HGCMSvcSetU32(&paParms[4], pOpenParms->cbPath);
        HGCMSvcSetPv (&paParms[5], pOpenParms->pszPath, pOpenParms->cbPath);
        HGCMSvcSetU64(&paParms[6], 0); /* OUT: uHandle */

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets a transfer list close request to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pMsgCtx             Message context to use.
 * @param   hList               Handle of list to close.
 */
static int shClSvcTransferSetListClose(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                       PSHCLMSGCTX pMsgCtx, SHCLLISTHANDLE hList)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_CLOSE)
    {
        HGCMSvcSetU64(&paParms[0], pMsgCtx->uContextID);
        HGCMSvcSetU64(&paParms[1], hList);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer list header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   phList              Where to store the list handle.
 * @param   pListHdr            Where to store the list header.
 */
static int shClSvcTransferGetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                     PSHCLLISTHANDLE phList, PSHCLLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_HDR)
    {
        rc = HGCMSvcGetU64(&paParms[1], phList);
        /* Note: Flags (paParms[2]) not used here. */
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[3], &pListHdr->fFeatures);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&paParms[4], &pListHdr->cTotalObjects);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU64(&paParms[5], &pListHdr->cbTotalSize);

        if (RT_SUCCESS(rc))
        {
            /** @todo Validate pvMetaFmt + cbMetaFmt. */
            /** @todo Validate header checksum. */
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets a transfer list header to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pListHdr            Pointer to list header to set.
 */
static int shClSvcTransferSetListHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PSHCLLISTHDR pListHdr)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_HDR)
    {
        /** @todo Set pvMetaFmt + cbMetaFmt. */
        /** @todo Calculate header checksum. */

        HGCMSvcSetU32(&paParms[3], pListHdr->fFeatures);
        HGCMSvcSetU64(&paParms[4], pListHdr->cTotalObjects);
        HGCMSvcSetU64(&paParms[5], pListHdr->cbTotalSize);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer list entry from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   phList              Where to store the list handle.
 * @param   pListEntry          Where to store the list entry.
 */
static int shClSvcTransferGetListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                       PSHCLLISTHANDLE phList, PSHCLLISTENTRY pListEntry)
{
    int rc;

    if (cParms == VBOX_SHCL_CPARMS_LIST_ENTRY)
    {
        rc = HGCMSvcGetU64(&paParms[1], phList);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetU32(&paParms[2], &pListEntry->fInfo);
        if (RT_SUCCESS(rc))
            rc = HGCMSvcGetPv(&paParms[3], (void **)&pListEntry->pszName, &pListEntry->cbName);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbInfo;
            rc = HGCMSvcGetU32(&paParms[4], &cbInfo);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&paParms[5], &pListEntry->pvInfo, &pListEntry->cbInfo);
                AssertReturn(cbInfo == pListEntry->cbInfo, VERR_INVALID_PARAMETER);
            }
        }

        if (RT_SUCCESS(rc))
        {
            if (!ShClTransferListEntryIsValid(pListEntry))
                rc = VERR_INVALID_PARAMETER;
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets a Shared Clipboard list entry to HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pListEntry          Pointer list entry to set.
 */
static int shClSvcTransferSetListEntry(uint32_t cParms, VBOXHGCMSVCPARM paParms[],
                                       PSHCLLISTENTRY pListEntry)
{
    int rc;

    /* Sanity. */
    AssertReturn(ShClTransferListEntryIsValid(pListEntry), VERR_INVALID_PARAMETER);

    if (cParms == VBOX_SHCL_CPARMS_LIST_ENTRY)
    {
        HGCMSvcSetPv (&paParms[3], pListEntry->pszName, pListEntry->cbName);
        HGCMSvcSetU32(&paParms[4], pListEntry->cbInfo);
        HGCMSvcSetPv (&paParms[5], pListEntry->pvInfo, pListEntry->cbInfo);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Gets a transfer object data chunk from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pDataChunk          Where to store the object data chunk data.
 */
static int shClSvcTransferGetObjDataChunk(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PSHCLOBJDATACHUNK pDataChunk)
{
    AssertPtrReturn(paParms,    VERR_INVALID_PARAMETER);
    AssertPtrReturn(pDataChunk, VERR_INVALID_PARAMETER);

    int rc;

    if (cParms == VBOX_SHCL_CPARMS_OBJ_WRITE)
    {
        rc = HGCMSvcGetU64(&paParms[1], &pDataChunk->uHandle);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbData;
            rc = HGCMSvcGetU32(&paParms[2], &cbData);
            if (RT_SUCCESS(rc))
            {
                rc = HGCMSvcGetPv(&paParms[3], &pDataChunk->pvData, &pDataChunk->cbData);
                AssertReturn(cbData == pDataChunk->cbData, VERR_INVALID_PARAMETER);

                /** @todo Implement checksum handling. */
            }
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Handles a guest reply (VBOX_SHCL_GUEST_FN_REPLY) message.
 *
 * @returns VBox status code.
 * @param   pClient             Pointer to associated client.
 * @param   pTransfer           Pointer to transfer to handle guest reply for.
 * @param   cParms              Number of function parameters supplied.
 * @param   paParms             Array function parameters supplied.
 */
static int shClSvcTransferHandleReply(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer,
                                      uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(pClient);

    int rc;

    uint32_t   cbReply = sizeof(SHCLREPLY);
    PSHCLREPLY pReply  = (PSHCLREPLY)RTMemAlloc(cbReply);
    if (pReply)
    {
        rc = shClSvcTransferGetReply(cParms, paParms, pReply);
        if (RT_SUCCESS(rc))
        {
            PSHCLEVENTPAYLOAD pPayload
                = (PSHCLEVENTPAYLOAD)RTMemAlloc(sizeof(SHCLEVENTPAYLOAD));
            if (pPayload)
            {
                pPayload->pvData = pReply;
                pPayload->cbData = cbReply;

                switch (pReply->uType)
                {
                    case VBOX_SHCL_REPLYMSGTYPE_TRANSFER_STATUS:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_REPLYMSGTYPE_LIST_OPEN:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_REPLYMSGTYPE_LIST_CLOSE:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_REPLYMSGTYPE_OBJ_OPEN:
                        RT_FALL_THROUGH();
                    case VBOX_SHCL_REPLYMSGTYPE_OBJ_CLOSE:
                    {
                        uint64_t uCID;
                        rc = HGCMSvcGetU64(&paParms[0], &uCID);
                        if (RT_SUCCESS(rc))
                        {
                            const SHCLEVENTID uEvent = VBOX_SHCL_CONTEXTID_GET_EVENT(uCID);

                            LogFlowFunc(("uCID=%RU64 -> uEvent=%RU32\n", uCID, uEvent));

                            rc = ShClEventSignal(&pTransfer->Events, uEvent, pPayload);
                        }
                        break;
                    }

                    default:
                        rc = VERR_NOT_FOUND;
                        break;
                }

                if (RT_FAILURE(rc))
                {
                    if (pPayload)
                        RTMemFree(pPayload);
                }
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_FAILURE(rc))
    {
        if (pReply)
            RTMemFree(pReply);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Transfer client (guest) handler for the Shared Clipboard host service.
 *
 * @returns VBox status code, or VINF_HGCM_ASYNC_EXECUTE if returning to the client will be deferred.
 * @param   pClient             Pointer to associated client.
 * @param   callHandle          The client's call handle of this call.
 * @param   u32Function         Function number being called.
 * @param   cParms              Number of function parameters supplied.
 * @param   paParms             Array function parameters  supplied.
 * @param   tsArrival           Timestamp of arrival.
 */
int shClSvcTransferHandler(PSHCLCLIENT pClient,
                           VBOXHGCMCALLHANDLE callHandle,
                           uint32_t u32Function,
                           uint32_t cParms,
                           VBOXHGCMSVCPARM paParms[],
                           uint64_t tsArrival)
{
    RT_NOREF(callHandle, paParms, tsArrival);

    LogFlowFunc(("uClient=%RU32, u32Function=%RU32 (%s), cParms=%RU32, g_ExtState.pfnExtension=%p\n",
                 pClient->State.uClientID, u32Function, ShClGuestMsgToStr(u32Function), cParms, g_ExtState.pfnExtension));

    /* Check if we've the right mode set. */
    if (!shClSvcTransferMsgIsAllowed(ShClSvcGetMode(), u32Function))
    {
        LogFunc(("Wrong clipboard mode, denying access\n"));
        return VERR_ACCESS_DENIED;
    }

    /* A (valid) service extension is needed because VBoxSVC needs to keep track of the
     * clipboard areas cached on the host. */
    if (!g_ExtState.pfnExtension)
    {
#ifdef DEBUG_andy
        AssertPtr(g_ExtState.pfnExtension);
#endif
        LogFunc(("Invalid / no service extension set, skipping transfer handling\n"));
        return VERR_NOT_SUPPORTED;
    }

    int rc = VERR_INVALID_PARAMETER; /* Play safe by default. */

    /*
     * Pre-check: For certain messages we need to make sure that a (right) transfer is present.
     */
    uint64_t      uCID      = 0; /* Context ID */
    PSHCLTRANSFER pTransfer = NULL;

    switch (u32Function)
    {
        default:
        {
            if (!ShClTransferCtxGetTotalTransfers(&pClient->TransferCtx))
            {
                LogFunc(("No transfers found\n"));
                rc = VERR_SHCLPB_TRANSFER_ID_NOT_FOUND;
                break;
            }

            if (cParms < 1)
                break;

            rc = HGCMSvcGetU64(&paParms[0], &uCID);
            if (RT_FAILURE(rc))
                break;

            const SHCLTRANSFERID uTransferID = VBOX_SHCL_CONTEXTID_GET_TRANSFER(uCID);

            pTransfer = ShClTransferCtxGetTransfer(&pClient->TransferCtx, uTransferID);
            if (!pTransfer)
            {
                LogFunc(("Transfer with ID %RU16 not found\n", uTransferID));
                rc = VERR_SHCLPB_TRANSFER_ID_NOT_FOUND;
            }
            break;
        }
    }

    if (RT_FAILURE(rc))
        return rc;

    rc = VERR_INVALID_PARAMETER; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHCL_GUEST_FN_REPLY:
        {
            rc = shClSvcTransferHandleReply(pClient, pTransfer, cParms, paParms);
            break;
        }

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_ROOT_LIST_HDR_READ)
                break;

            if (   ShClTransferGetSource(pTransfer) == SHCLSOURCE_LOCAL
                && ShClTransferGetDir(pTransfer)    == SHCLTRANSFERDIR_TO_REMOTE)
            {
                /* Get roots if this is a local write transfer (host -> guest). */
                rc = ShClSvcImplTransferGetRoots(pClient, pTransfer);
            }
            else
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            SHCLROOTLISTHDR rootListHdr;
            RT_ZERO(rootListHdr);

            rootListHdr.cRoots = ShClTransferRootsCount(pTransfer);

            HGCMSvcSetU64(&paParms[0], 0 /* Context ID */);
            HGCMSvcSetU32(&paParms[1], rootListHdr.fRoots);
            HGCMSvcSetU32(&paParms[2], rootListHdr.cRoots);

            rc = VINF_SUCCESS;
            break;
        }

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_WRITE:
        {
            SHCLROOTLISTHDR lstHdr;
            rc = shClSvcTransferGetRootListHdr(cParms, paParms, &lstHdr);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = ShClTransferRootListHdrDup(&lstHdr);
                uint32_t cbData = sizeof(SHCLROOTLISTHDR);

                const SHCLEVENTID uEvent = VBOX_SHCL_CONTEXTID_GET_EVENT(uCID);

                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    rc = ShClEventSignal(&pTransfer->Events, uEvent, pPayload);
                    if (RT_FAILURE(rc))
                        ShClPayloadFree(pPayload);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_READ)
                break;

            /* paParms[1] contains fInfo flags, currently unused. */
            uint32_t uIndex;
            rc = HGCMSvcGetU32(&paParms[2], &uIndex);
            if (RT_SUCCESS(rc))
            {
                SHCLROOTLISTENTRY rootListEntry;
                rc = ShClTransferRootsEntry(pTransfer, uIndex, &rootListEntry);
                if (RT_SUCCESS(rc))
                {
                    HGCMSvcSetPv (&paParms[3], rootListEntry.pszName, rootListEntry.cbName);
                    HGCMSvcSetU32(&paParms[4], rootListEntry.cbInfo);
                    HGCMSvcSetPv (&paParms[5], rootListEntry.pvInfo, rootListEntry.cbInfo);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_WRITE:
        {
            SHCLROOTLISTENTRY lstEntry;
            rc = shClSvcTransferGetRootListEntry(cParms, paParms, &lstEntry);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = ShClTransferRootListEntryDup(&lstEntry);
                uint32_t cbData = sizeof(SHCLROOTLISTENTRY);

                const SHCLEVENTID uEvent = VBOX_SHCL_CONTEXTID_GET_EVENT(uCID);

                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    rc = ShClEventSignal(&pTransfer->Events, uEvent, pPayload);
                    if (RT_FAILURE(rc))
                        ShClPayloadFree(pPayload);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_OPEN:
        {
            SHCLLISTOPENPARMS listOpenParms;
            rc = shClSvcTransferGetListOpen(cParms, paParms, &listOpenParms);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = ShClTransferListOpen(pTransfer, &listOpenParms, &hList);
                if (RT_SUCCESS(rc))
                {
                    /* Return list handle. */
                    HGCMSvcSetU64(&paParms[6], hList);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_CLOSE:
        {
            if (cParms != VBOX_SHCL_CPARMS_LIST_CLOSE)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&paParms[1], &hList);
            if (RT_SUCCESS(rc))
            {
                rc = ShClTransferListClose(pTransfer, hList);
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_HDR_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_LIST_HDR)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&paParms[1], &hList); /* Get list handle. */
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHDR hdrList;
                rc = ShClTransferListGetHeader(pTransfer, hList, &hdrList);
                if (RT_SUCCESS(rc))
                    rc = shClSvcTransferSetListHdr(cParms, paParms, &hdrList);
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_HDR_WRITE:
        {
            SHCLLISTHDR hdrList;
            rc = ShClTransferListHdrInit(&hdrList);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = shClSvcTransferGetListHdr(cParms, paParms, &hList, &hdrList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = ShClTransferListHdrDup(&hdrList);
                    uint32_t cbData = sizeof(SHCLLISTHDR);

                    const SHCLEVENTID uEvent = VBOX_SHCL_CONTEXTID_GET_EVENT(uCID);

                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = ShClEventSignal(&pTransfer->Events, uEvent, pPayload);
                        if (RT_FAILURE(rc))
                            ShClPayloadFree(pPayload);
                    }
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_LIST_ENTRY)
                break;

            SHCLLISTHANDLE hList;
            rc = HGCMSvcGetU64(&paParms[1], &hList); /* Get list handle. */
            if (RT_SUCCESS(rc))
            {
                SHCLLISTENTRY entryList;
                rc = ShClTransferListEntryInit(&entryList);
                if (RT_SUCCESS(rc))
                {
                    rc = ShClTransferListRead(pTransfer, hList, &entryList);
                    if (RT_SUCCESS(rc))
                        rc = shClSvcTransferSetListEntry(cParms, paParms, &entryList);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_WRITE:
        {
            SHCLLISTENTRY entryList;
            rc = ShClTransferListEntryInit(&entryList);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                rc = shClSvcTransferGetListEntry(cParms, paParms, &hList, &entryList);
                if (RT_SUCCESS(rc))
                {
                    void    *pvData = ShClTransferListEntryDup(&entryList);
                    uint32_t cbData = sizeof(SHCLLISTENTRY);

                    const SHCLEVENTID uEvent = VBOX_SHCL_CONTEXTID_GET_EVENT(uCID);

                    PSHCLEVENTPAYLOAD pPayload;
                    rc = ShClPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                    if (RT_SUCCESS(rc))
                    {
                        rc = ShClEventSignal(&pTransfer->Events, uEvent, pPayload);
                        if (RT_FAILURE(rc))
                            ShClPayloadFree(pPayload);
                    }
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_OBJ_OPEN:
        {
            ASSERT_GUEST_STMT_BREAK(cParms == VBOX_SHCL_CPARMS_OBJ_OPEN, VERR_WRONG_PARAMETER_COUNT);

            SHCLOBJOPENCREATEPARMS openCreateParms;
            RT_ZERO(openCreateParms);

            uint32_t cbPath;
            rc = HGCMSvcGetU32(&paParms[2], &cbPath); /** @todo r=bird: This is an pointless parameter. */
            if (RT_SUCCESS(rc))
            {
                /** @todo r=bird: This is the wrong way of getting a string!   */
                rc = HGCMSvcGetPv(&paParms[3], (void **)&openCreateParms.pszPath, &openCreateParms.cbPath);
                if (cbPath != openCreateParms.cbPath)
                    rc = VERR_INVALID_PARAMETER;
            }
            if (RT_SUCCESS(rc))
                rc = HGCMSvcGetU32(&paParms[4], &openCreateParms.fCreate);

            if (RT_SUCCESS(rc))
            {
                SHCLOBJHANDLE hObj;
                rc = ShClTransferObjOpen(pTransfer, &openCreateParms, &hObj);
                if (RT_SUCCESS(rc))
                {
                    LogFlowFunc(("hObj=%RU64\n", hObj));

                    HGCMSvcSetU64(&paParms[1], hObj);
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_OBJ_CLOSE:
        {
            if (cParms != VBOX_SHCL_CPARMS_OBJ_CLOSE)
                break;

            SHCLOBJHANDLE hObj;
            rc = HGCMSvcGetU64(&paParms[1], &hObj); /* Get object handle. */
            if (RT_SUCCESS(rc))
                rc = ShClTransferObjClose(pTransfer, hObj);
            break;
        }

        case VBOX_SHCL_GUEST_FN_OBJ_READ:
        {
            if (cParms != VBOX_SHCL_CPARMS_OBJ_READ)
                break;

            SHCLOBJHANDLE hObj;
            rc = HGCMSvcGetU64(&paParms[1], &hObj); /* Get object handle. */

            uint32_t cbToRead = 0;
            if (RT_SUCCESS(rc))
                rc = HGCMSvcGetU32(&paParms[2], &cbToRead);

            void    *pvBuf = NULL;
            uint32_t cbBuf = 0;
            if (RT_SUCCESS(rc))
                rc = HGCMSvcGetPv(&paParms[3], &pvBuf, &cbBuf);

            LogFlowFunc(("hObj=%RU64, cbBuf=%RU32, cbToRead=%RU32, rc=%Rrc\n", hObj, cbBuf, cbToRead, rc));

            if (   RT_SUCCESS(rc)
                && (   !cbBuf
                    || !cbToRead
                    ||  cbBuf < cbToRead
                   )
               )
            {
                rc = VERR_INVALID_PARAMETER;
            }

            if (RT_SUCCESS(rc))
            {
                uint32_t cbRead;
                rc = ShClTransferObjRead(pTransfer, hObj, pvBuf, cbToRead, &cbRead, 0 /* fFlags */);
                if (RT_SUCCESS(rc))
                {
                    HGCMSvcSetU32(&paParms[3], cbRead);

                    /** @todo Implement checksum support. */
                }
            }
            break;
        }

        case VBOX_SHCL_GUEST_FN_OBJ_WRITE:
        {
            SHCLOBJDATACHUNK dataChunk;
            rc = shClSvcTransferGetObjDataChunk(cParms, paParms, &dataChunk);
            if (RT_SUCCESS(rc))
            {
                void    *pvData = ShClTransferObjDataChunkDup(&dataChunk);
                uint32_t cbData = sizeof(SHCLOBJDATACHUNK);

                const SHCLEVENTID uEvent = VBOX_SHCL_CONTEXTID_GET_EVENT(uCID);

                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClPayloadAlloc(uEvent, pvData, cbData, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    rc = ShClEventSignal(&pTransfer->Events, uEvent, pPayload);
                    if (RT_FAILURE(rc))
                        ShClPayloadFree(pPayload);
                }
            }

            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    LogFlowFunc(("[Client %RU32] Returning rc=%Rrc\n", pClient->State.uClientID, rc));
    return rc;
}

/**
 * Transfer host handler for the Shared Clipboard host service.
 *
 * @returns VBox status code.
 * @param   u32Function         Function number being called.
 * @param   cParms              Number of function parameters supplied.
 * @param   paParms             Array function parameters  supplied.
 */
int shClSvcTransferHostHandler(uint32_t u32Function,
                               uint32_t cParms,
                               VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(cParms, paParms);

    int rc = VERR_NOT_IMPLEMENTED; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHCL_HOST_FN_CANCEL: /** @todo Implement this. */
            break;

        case VBOX_SHCL_HOST_FN_ERROR: /** @todo Implement this. */
            break;

        default:
            break;

    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int shClSvcTransferHostMsgHandler(PSHCLCLIENT pClient, PSHCLCLIENTMSG pMsg)
{
    RT_NOREF(pClient);

    int rc;

    switch (pMsg->uMsg)
    {
        default:
            rc = VINF_SUCCESS;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Registers an clipboard transfer area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           Shared Clipboard transfer to register a clipboard area for.
 */
int shClSvcTransferAreaRegister(PSHCLCLIENTSTATE pClientState, PSHCLTRANSFER pTransfer)
{
    RT_NOREF(pClientState);

    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->pArea == NULL, ("An area already is registered for this transfer\n"),
                    VERR_WRONG_ORDER);

    pTransfer->pArea = new SharedClipboardArea();
    if (!pTransfer->pArea)
        return VERR_NO_MEMORY;

    int rc;

    if (g_ExtState.pfnExtension)
    {
        SHCLEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = NIL_SHCLAREAID;

        /* As the meta data is now complete, register a new clipboard on the host side. */
        rc = g_ExtState.pfnExtension(g_ExtState.pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_REGISTER, &parms, sizeof(parms));
        if (RT_SUCCESS(rc))
        {
            /* Note: Do *not* specify SHCLAREA_OPEN_FLAGS_MUST_NOT_EXIST as flags here, as VBoxSVC took care of the
             *       clipboard area creation already. */
            rc = pTransfer->pArea->OpenTemp(parms.uID /* Area ID */,
                                            SHCLAREA_OPEN_FLAGS_NONE);
        }

        LogFlowFunc(("Registered new clipboard area (%RU32) by client %RU32 with rc=%Rrc\n",
                     parms.uID, pClientState->uClientID, rc));
    }
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Unregisters an clipboard transfer area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           Shared Clipboard transfer to unregister a clipboard area from.
 */
int shClSvcTransferAreaUnregister(PSHCLCLIENTSTATE pClientState, PSHCLTRANSFER pTransfer)
{
    RT_NOREF(pClientState);

    LogFlowFuncEnter();

    if (!pTransfer->pArea)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    if (g_ExtState.pfnExtension)
    {
        SHCLEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = pTransfer->pArea->GetID();

        rc = g_ExtState.pfnExtension(g_ExtState.pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_UNREGISTER, &parms, sizeof(parms));
        if (RT_SUCCESS(rc))
        {
            rc = pTransfer->pArea->Close();
            if (RT_SUCCESS(rc))
            {
                delete pTransfer->pArea;
                pTransfer->pArea = NULL;
            }
        }

        LogFlowFunc(("Unregistered clipboard area (%RU32) by client %RU32 with rc=%Rrc\n",
                     parms.uID, pClientState->uClientID, rc));
    }

    delete pTransfer->pArea;
    pTransfer->pArea = NULL;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Attaches to an existing (registered) clipboard transfer area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           Shared Clipboard transfer to attach a clipboard area to.
 * @param   uID                 ID of clipboard area to to attach to. Specify 0 to attach to the most recent one.
 */
int shClSvcTransferAreaAttach(PSHCLCLIENTSTATE pClientState, PSHCLTRANSFER pTransfer,
                              SHCLAREAID uID)
{
    RT_NOREF(pClientState);

    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->pArea == NULL, ("An area already is attached to this transfer\n"),
                    VERR_WRONG_ORDER);

    pTransfer->pArea = new SharedClipboardArea();
    if (!pTransfer->pArea)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    if (g_ExtState.pfnExtension)
    {
        SHCLEXTAREAPARMS parms;
        RT_ZERO(parms);

        parms.uID = uID; /* 0 means most recent clipboard area. */

        /* The client now needs to attach to the most recent clipboard area
         * to keep a reference to it. The host does the actual book keeping / cleanup then.
         *
         * This might fail if the host does not have a most recent clipboard area (yet). */
        rc = g_ExtState.pfnExtension(g_ExtState.pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_ATTACH, &parms, sizeof(parms));
        if (RT_SUCCESS(rc))
            rc = pTransfer->pArea->OpenTemp(parms.uID /* Area ID */);

        LogFlowFunc(("Attached client %RU32 to clipboard area %RU32 with rc=%Rrc\n",
                     pClientState->uClientID, parms.uID, rc));
    }
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Detaches from an clipboard transfer area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           Shared Clipboard transfer to detach a clipboard area from.
 */
int shClSvcTransferAreaDetach(PSHCLCLIENTSTATE pClientState, PSHCLTRANSFER pTransfer)
{
    RT_NOREF(pClientState);

    LogFlowFuncEnter();

    if (!pTransfer->pArea)
        return VINF_SUCCESS;

    const uint32_t uAreaID = pTransfer->pArea->GetID();

    int rc = VINF_SUCCESS;

    if (g_ExtState.pfnExtension)
    {
        SHCLEXTAREAPARMS parms;
        RT_ZERO(parms);
        parms.uID = uAreaID;

        rc = g_ExtState.pfnExtension(g_ExtState.pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_DETACH, &parms, sizeof(parms));

        LogFlowFunc(("Detached client %RU32 from clipboard area %RU32 with rc=%Rrc\n",
                     pClientState->uClientID, uAreaID, rc));
    }

    delete pTransfer->pArea;
    pTransfer->pArea = NULL;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reports a transfer status to the guest.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   pTransfer           Transfer to report status for.
 * @param   uStatus             Status to report.
 * @param   rcTransfer          Result code to report. Optional and depending on status.
 * @param   puEvent             Where to store the created wait event. Optional.
 */
int shClSvcTransferSendStatus(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer, SHCLTRANSFERSTATUS uStatus,
                              int rcTransfer, PSHCLEVENTID puEvent)
{
    AssertPtrReturn(pClient,   VERR_INVALID_POINTER);
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    /* puEvent is optional. */

    PSHCLCLIENTMSG pMsgReadData = shClSvcMsgAlloc(VBOX_SHCL_HOST_MSG_TRANSFER_STATUS,
                                                  VBOX_SHCL_CPARMS_TRANSFER_STATUS);
    if (!pMsgReadData)
        return VERR_NO_MEMORY;

    const SHCLEVENTID uEvent = ShClEventIDGenerate(&pTransfer->Events);

    HGCMSvcSetU64(&pMsgReadData->paParms[0], VBOX_SHCL_CONTEXTID_MAKE(pClient->State.uSessionID,
                                                                      pTransfer->State.uID, uEvent));
    HGCMSvcSetU32(&pMsgReadData->paParms[1], pTransfer->State.enmDir);
    HGCMSvcSetU32(&pMsgReadData->paParms[2], uStatus);
    HGCMSvcSetU32(&pMsgReadData->paParms[3], (uint32_t)rcTransfer); /** @todo uint32_t vs. int. */
    HGCMSvcSetU32(&pMsgReadData->paParms[4], 0 /* fFlags, unused */);

    int rc = shClSvcMsgAdd(pClient, pMsgReadData, true /* fAppend */);
    if (RT_SUCCESS(rc))
    {
        rc = ShClEventRegister(&pTransfer->Events, uEvent);
        if (RT_SUCCESS(rc))
        {
            rc = shClSvcClientWakeup(pClient);
            if (RT_SUCCESS(rc))
            {
                LogRel2(("Shared Clipboard: Reported status %s (rc=%Rrc) of transfer %RU32 to guest\n",
                         ShClTransferStatusToStr(uStatus), rcTransfer, pTransfer->State.uID));

                if (puEvent)
                    *puEvent = uEvent;
            }
            else
                ShClEventUnregister(&pTransfer->Events, uEvent);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Starts a new transfer, waiting for acknowledgement by the guest side.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   enmDir              Transfer direction to start.
 * @param   enmSource           Transfer source to start.
 * @param   ppTransfer          Where to return the created transfer on success. Optional.
 */
int shClSvcTransferStart(PSHCLCLIENT pClient,
                         SHCLTRANSFERDIR enmDir, SHCLSOURCE enmSource,
                         PSHCLTRANSFER *ppTransfer)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);
    /* ppTransfer is optional. */

    LogFlowFuncEnter();

    ShClTransferCtxCleanup(&pClient->TransferCtx);

    int rc;

    if (!ShClTransferCtxTransfersMaximumReached(&pClient->TransferCtx))
    {
        LogRel2(("Shared Clipboard: Starting %s transfer ...\n", enmDir == SHCLTRANSFERDIR_FROM_REMOTE ? "read" : "write"));

        PSHCLTRANSFER pTransfer;
        rc = ShClTransferCreate(&pTransfer);
        if (RT_SUCCESS(rc))
        {
            rc = ShClSvcImplTransferCreate(pClient, pTransfer);
            if (RT_SUCCESS(rc))
            {
                SHCLPROVIDERCREATIONCTX creationCtx;
                RT_ZERO(creationCtx);

                if (enmDir == SHCLTRANSFERDIR_FROM_REMOTE)
                {
                    rc = shClSvcTransferAreaRegister(&pClient->State, pTransfer);
                    if (RT_SUCCESS(rc))
                    {
                        creationCtx.Interface.pfnTransferOpen  = shClSvcTransferIfaceOpen;
                        creationCtx.Interface.pfnTransferClose = shClSvcTransferIfaceClose;

                        creationCtx.Interface.pfnRootsGet      = shClSvcTransferIfaceGetRoots;

                        creationCtx.Interface.pfnListOpen      = shClSvcTransferIfaceListOpen;
                        creationCtx.Interface.pfnListClose     = shClSvcTransferIfaceListClose;
                        creationCtx.Interface.pfnListHdrRead   = shClSvcTransferIfaceListHdrRead;
                        creationCtx.Interface.pfnListEntryRead = shClSvcTransferIfaceListEntryRead;

                        creationCtx.Interface.pfnObjOpen       = shClSvcTransferIfaceObjOpen;
                        creationCtx.Interface.pfnObjClose      = shClSvcTransferIfaceObjClose;
                        creationCtx.Interface.pfnObjRead       = shClSvcTransferIfaceObjRead;
                    }
                }
                else if (enmDir == SHCLTRANSFERDIR_TO_REMOTE)
                {
                    creationCtx.Interface.pfnListHdrWrite   = shClSvcTransferIfaceListHdrWrite;
                    creationCtx.Interface.pfnListEntryWrite = shClSvcTransferIfaceListEntryWrite;
                    creationCtx.Interface.pfnObjWrite       = shClSvcTransferIfaceObjWrite;
                }
                else
                    AssertFailed();

                creationCtx.enmSource = pClient->State.enmSource;
                creationCtx.pvUser    = pClient;

                uint32_t uTransferID = 0;

                rc = ShClTransferSetInterface(pTransfer, &creationCtx);
                if (RT_SUCCESS(rc))
                {
                    rc = ShClTransferCtxTransferRegister(&pClient->TransferCtx, pTransfer, &uTransferID);
                    if (RT_SUCCESS(rc))
                    {
                        rc = ShClTransferInit(pTransfer, uTransferID, enmDir, enmSource);
                        if (RT_SUCCESS(rc))
                        {
                            if (RT_SUCCESS(rc))
                                rc = ShClTransferStart(pTransfer);

                            if (RT_SUCCESS(rc))
                            {
                                SHCLEVENTID uEvent;
                                rc = shClSvcTransferSendStatus(pClient, pTransfer,
                                                               SHCLTRANSFERSTATUS_INITIALIZED, VINF_SUCCESS,
                                                               &uEvent);
                                if (RT_SUCCESS(rc))
                                {
                                    LogRel2(("Shared Clipboard: Waiting for start of transfer %RU32 on guest ...\n",
                                             pTransfer->State.uID));

                                    PSHCLEVENTPAYLOAD pPayload;
                                    rc = ShClEventWait(&pTransfer->Events, uEvent, pTransfer->uTimeoutMs, &pPayload);
                                    if (RT_SUCCESS(rc))
                                    {
                                        Assert(pPayload->cbData == sizeof(SHCLREPLY));
                                        PSHCLREPLY pReply = (PSHCLREPLY)pPayload->pvData;
                                        AssertPtr(pReply);

                                        Assert(pReply->uType == VBOX_SHCL_REPLYMSGTYPE_TRANSFER_STATUS);

                                        if (pReply->u.TransferStatus.uStatus == SHCLTRANSFERSTATUS_STARTED)
                                        {
                                            LogRel2(("Shared Clipboard: Started transfer %RU32 on guest\n", pTransfer->State.uID));
                                        }
                                        else
                                            LogRel(("Shared Clipboard: Guest reported status %s (error %Rrc) while starting transfer %RU32\n",
                                                    ShClTransferStatusToStr(pReply->u.TransferStatus.uStatus),
                                                    pReply->rc, pTransfer->State.uID));
                                    }
                                    else
                                       LogRel(("Shared Clipboard: Unable to start transfer %RU32 on guest, rc=%Rrc\n",
                                               pTransfer->State.uID, rc));
                                }
                            }
                        }

                        if (RT_FAILURE(rc))
                            ShClTransferCtxTransferUnregister(&pClient->TransferCtx, uTransferID);
                    }
                }
            }

            if (RT_FAILURE(rc))
            {
                ShClSvcImplTransferDestroy(pClient, pTransfer);
                ShClTransferDestroy(pTransfer);

                RTMemFree(pTransfer);
                pTransfer = NULL;
            }
            else
            {
                if (ppTransfer)
                    *ppTransfer = pTransfer;
            }
        }

        if (RT_FAILURE(rc))
            LogRel(("Shared Clipboard: Starting transfer failed with %Rrc\n", rc));
    }
    else
        rc = VERR_SHCLPB_MAX_TRANSFERS_REACHED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Stops (and destroys) a transfer, communicating the status to the guest side.
 *
 * @returns VBox status code.
 * @param   pClient             Client that owns the transfer.
 * @param   pTransfer           Transfer to stop.
 */
int shClSvcTransferStop(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    SHCLEVENTID uEvent;
    int rc = shClSvcTransferSendStatus(pClient, pTransfer,
                                       SHCLTRANSFERSTATUS_STOPPED, VINF_SUCCESS,
                                       &uEvent);
    if (RT_SUCCESS(rc))
    {
        LogRel2(("Shared Clipboard: Waiting for stop of transfer %RU32 on guest ...\n", pTransfer->State.uID));

        rc = ShClEventWait(&pTransfer->Events, uEvent, pTransfer->uTimeoutMs, NULL);
        if (RT_SUCCESS(rc))
            LogRel2(("Shared Clipboard: Stopped transfer %RU32 on guest\n", pTransfer->State.uID));
    }

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Unable to stop transfer %RU32 on guest, rc=%Rrc\n",
                pTransfer->State.uID, rc));

    /* Regardless of whether the guest was able to report back and/or stop the transfer, remove the transfer on the host
     * so that we don't risk of having stale transfers here. */
    int rc2 = ShClTransferCtxTransferUnregister(&pClient->TransferCtx, ShClTransferGetID(pTransfer));
    if (RT_SUCCESS(rc2))
    {
        ShClTransferDestroy(pTransfer);
        pTransfer = NULL;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets the host service's (file) transfer mode.
 *
 * @returns VBox status code.
 * @param   fMode               Transfer mode to set.
 */
int shClSvcTransferModeSet(uint32_t fMode)
{
    if (fMode & ~VBOX_SHCL_TRANSFER_MODE_VALID_MASK)
        return VERR_INVALID_FLAGS;

    g_fTransferMode = fMode;

    LogRel2(("Shared Clipboard: File transfers are now %s\n",
             g_fTransferMode != VBOX_SHCL_TRANSFER_MODE_DISABLED ? "enabled" : "disabled"));

    /* If file transfers are being disabled, make sure to also reset (destroy) all pending transfers. */
    if (g_fTransferMode == VBOX_SHCL_TRANSFER_MODE_DISABLED)
    {
        ClipboardClientMap::const_iterator itClient = g_mapClients.begin();
        while (itClient != g_mapClients.end())
        {
            PSHCLCLIENT pClient = itClient->second;
            AssertPtr(pClient);

            shClSvcClientTransfersReset(pClient);

            ++itClient;
        }
    }

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}
