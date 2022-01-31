/* $Id$ */
/** @file
 * Shared Clipboard host service test case.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "../VBoxSharedClipboardSvc-internal.h"

#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/VBoxGuestLib.h>

#ifdef RT_OS_LINUX
# include <VBox/GuestHost/SharedClipboard-x11.h>
#endif

#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/utf16.h>

static RTTEST g_hTest;

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *ptable);

static SHCLCLIENT g_Client;

typedef uint32_t HGCMCLIENTID;
# define VBGLR3DECL(type) DECL_HIDDEN_NOTHROW(type) VBOXCALL

RT_C_DECLS_BEGIN

/** Simple call handle structure for the guest call completion callback. */
struct VBOXHGCMCALLHANDLE_TYPEDEF
{
    /** Where to store the result code on call completion. */
    int32_t rc;
};

typedef enum TSTHGCMMOCKFNTYPE
{
    TSTHGCMMOCKFNTYPE_NONE = 0,
    TSTHGCMMOCKFNTYPE_CONNECT,
    TSTHGCMMOCKFNTYPE_DISCONNECT,
    TSTHGCMMOCKFNTYPE_CALL,
    TSTHGCMMOCKFNTYPE_HOST_CALL
} TSTHGCMMOCKFNTYPE;

struct TSTHGCMMOCKSVC;

typedef struct TSTHGCMMOCKCLIENT
{
    TSTHGCMMOCKSVC            *pSvc;
    uint32_t                   idClient;
    SHCLCLIENT                 Client;
    VBOXHGCMCALLHANDLE_TYPEDEF hCall;
    bool                       fAsyncExec;
    RTSEMEVENT                 hEvent;
} TSTHGCMMOCKCLIENT;
/** Pointer to a mock HGCM client. */
typedef TSTHGCMMOCKCLIENT *PTSTHGCMMOCKCLIENT;

typedef struct TSTHGCMMOCKFN
{
    RTLISTNODE         Node;
    TSTHGCMMOCKFNTYPE  enmType;
    PTSTHGCMMOCKCLIENT pClient;
    union
    {
        struct
        {
        } Connect;
        struct
        {
        } Disconnect;
        struct
        {
            int32_t             iFunc;
            uint32_t            cParms;
            PVBOXHGCMSVCPARM    pParms;
            VBOXHGCMCALLHANDLE  hCall;
        } Call;
        struct
        {
            int32_t             iFunc;
            uint32_t            cParms;
            PVBOXHGCMSVCPARM    pParms;
        } HostCall;
    } u;
} TSTHGCMMOCKFN;
typedef TSTHGCMMOCKFN *PTSTHGCMMOCKFN;

typedef struct TSTHGCMMOCKSVC
{
    VBOXHGCMSVCHELPERS fnHelpers;
    HGCMCLIENTID       uNextClientId;
    TSTHGCMMOCKCLIENT  aHgcmClient[4];
    VBOXHGCMSVCFNTABLE fnTable;
    RTTHREAD           hThread;
    RTSEMEVENT         hEventQueue;
    RTSEMEVENT         hEventWait;
    /** Event semaphore for host calls. */
    RTSEMEVENT         hEventHostCall;
    RTLISTANCHOR       lstCall;
    volatile bool      fShutdown;
} TSTHGCMMOCKSVC;
/** Pointer to a mock HGCM service. */
typedef TSTHGCMMOCKSVC *PTSTHGCMMOCKSVC;

static TSTHGCMMOCKSVC    s_tstHgcmSvc;

struct TESTDESC;
/** Pointer to a test description. */
typedef TESTDESC *PTESTDESC;

struct TESTPARMS;
/** Pointer to a test parameter structure. */
typedef TESTPARMS *PTESTPARMS;

struct TESTCTX;
/** Pointer to a test context. */
typedef TESTCTX *PTESTCTX;

/** Pointer a test descriptor. */
typedef TESTDESC *PTESTDESC;

typedef DECLCALLBACKTYPE(int, FNTESTSETUP,(PTESTPARMS pTstParms, void **ppvCtx));
/** Pointer to an test setup callback. */
typedef FNTESTSETUP *PFNTESTSETUP;

typedef DECLCALLBACKTYPE(int, FNTESTEXEC,(PTESTPARMS pTstParms, void *pvCtx));
/** Pointer to an test exec callback. */
typedef FNTESTEXEC *PFNTESTEXEC;

typedef DECLCALLBACKTYPE(int, FNTESTGSTTHREAD,(PTESTCTX pCtx, void *pvCtx));
/** Pointer to an test guest thread callback. */
typedef FNTESTGSTTHREAD *PFNTESTGSTTHREAD;

typedef DECLCALLBACKTYPE(int, FNTESTDESTROY,(PTESTPARMS pTstParms, void *pvCtx));
/** Pointer to an test destroy callback. */
typedef FNTESTDESTROY *PFNTESTDESTROY;

static int tstHgcmMockClientInit(PTSTHGCMMOCKCLIENT pClient, uint32_t idClient)
{
    RT_BZERO(pClient, sizeof(TSTHGCMMOCKCLIENT));

    pClient->idClient = idClient;

    return RTSemEventCreate(&pClient->hEvent);
}

static int tstHgcmMockClientDestroy(PTSTHGCMMOCKCLIENT pClient)
{
    int rc = RTSemEventDestroy(pClient->hEvent);
    if (RT_SUCCESS(rc))
    {
        pClient->hEvent = NIL_RTSEMEVENT;
    }

    return rc;
}

#if 0
static void tstBackendWriteData(HGCMCLIENTID idClient, SHCLFORMAT uFormat, void *pvData, size_t cbData)
{
    ShClBackendSetClipboardData(&s_tstHgcmClient[idClient].Client, uFormat, pvData, cbData);
}

/** Adds a host data read request message to the client's message queue. */
static int tstSvcMockRequestDataFromGuest(uint32_t idClient, SHCLFORMATS fFormats, PSHCLEVENT *ppEvent)
{
    AssertPtrReturn(ppEvent, VERR_INVALID_POINTER);

    int rc = ShClSvcGuestDataRequest(&s_tstHgcmClient[idClient].Client, fFormats, ppEvent);
    RTTESTI_CHECK_RC_OK_RET(rc, rc);

    return rc;
}
#endif

static DECLCALLBACK(int) tstHgcmMockSvcConnect(PTSTHGCMMOCKSVC pSvc, void *pvService, uint32_t *pidClient)
{
    RT_NOREF(pvService);

    PTSTHGCMMOCKFN pFn = (PTSTHGCMMOCKFN)RTMemAllocZ(sizeof(TSTHGCMMOCKFN));
    AssertPtrReturn(pFn, VERR_NO_MEMORY);

    PTSTHGCMMOCKCLIENT pClient = &pSvc->aHgcmClient[pSvc->uNextClientId];

    int rc = tstHgcmMockClientInit(pClient, pSvc->uNextClientId);
    if (RT_FAILURE(rc))
        return rc;

    pFn->enmType = TSTHGCMMOCKFNTYPE_CONNECT;
    pFn->pClient = pClient;

    RTListAppend(&pSvc->lstCall, &pFn->Node);
    pFn = NULL; /* Thread takes ownership now. */

    int rc2 = RTSemEventSignal(pSvc->hEventQueue);
    AssertRCReturn(rc2, rc2);

    rc2 = RTSemEventWait(pClient->hEvent, RT_MS_30SEC);
    AssertRCReturn(rc2, rc2);

    ASMAtomicIncU32(&pSvc->uNextClientId);

    *pidClient = pClient->idClient;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstHgcmMockSvcDisconnect(PTSTHGCMMOCKSVC pSvc, void *pvService, uint32_t idClient)
{
    RT_NOREF(pvService);

    PTSTHGCMMOCKCLIENT pClient = &pSvc->aHgcmClient[idClient];

    PTSTHGCMMOCKFN pFn = (PTSTHGCMMOCKFN)RTMemAllocZ(sizeof(TSTHGCMMOCKFN));
    AssertPtrReturn(pFn, VERR_NO_MEMORY);

    pFn->enmType = TSTHGCMMOCKFNTYPE_DISCONNECT;
    pFn->pClient = pClient;

    RTListAppend(&pSvc->lstCall, &pFn->Node);
    pFn = NULL; /* Thread takes ownership now. */

    int rc2 = RTSemEventSignal(pSvc->hEventQueue);
    AssertRCReturn(rc2, rc2);

    rc2 = RTSemEventWait(pClient->hEvent, RT_MS_30SEC);
    AssertRCReturn(rc2, rc2);

    return tstHgcmMockClientDestroy(pClient);
}

static DECLCALLBACK(int) tstHgcmMockSvcCall(PTSTHGCMMOCKSVC pSvc, void *pvService, VBOXHGCMCALLHANDLE callHandle, uint32_t idClient, void *pvClient,
                                            int32_t function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(pvService, pvClient);

    PTSTHGCMMOCKCLIENT pClient = &pSvc->aHgcmClient[idClient];

    PTSTHGCMMOCKFN pFn = (PTSTHGCMMOCKFN)RTMemAllocZ(sizeof(TSTHGCMMOCKFN));
    AssertPtrReturn(pFn, VERR_NO_MEMORY);

    const size_t cbParms = cParms * sizeof(VBOXHGCMSVCPARM);

    pFn->enmType         = TSTHGCMMOCKFNTYPE_CALL;
    pFn->pClient         = pClient;

    pFn->u.Call.hCall    = callHandle;
    pFn->u.Call.iFunc    = function;
    pFn->u.Call.pParms   = (PVBOXHGCMSVCPARM)RTMemDup(paParms, cbParms);
    AssertPtrReturn(pFn->u.Call.pParms, VERR_NO_MEMORY);
    pFn->u.Call.cParms   = cParms;

    RTListAppend(&pSvc->lstCall, &pFn->Node);

    int rc2 = RTSemEventSignal(pSvc->hEventQueue);
    AssertRCReturn(rc2, rc2);

    rc2 = RTSemEventWait(pSvc->aHgcmClient[idClient].hEvent, RT_INDEFINITE_WAIT);
    AssertRCReturn(rc2, rc2);

    memcpy(paParms, pFn->u.Call.pParms, cbParms);

    return VINF_SUCCESS; /** @todo Return host call rc */
}

static DECLCALLBACK(int) tstHgcmMockSvcHostCall(PTSTHGCMMOCKSVC pSvc, void *pvService, int32_t function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(pvService);

    PTSTHGCMMOCKFN pFn = (PTSTHGCMMOCKFN)RTMemAllocZ(sizeof(TSTHGCMMOCKFN));
    AssertPtrReturn(pFn, VERR_INVALID_POINTER);

    pFn->enmType           = TSTHGCMMOCKFNTYPE_HOST_CALL;
    pFn->u.HostCall.iFunc  = function;
    pFn->u.HostCall.pParms = (PVBOXHGCMSVCPARM)RTMemDup(paParms, cParms * sizeof(VBOXHGCMSVCPARM));
    AssertPtrReturn(pFn->u.HostCall.pParms, VERR_NO_MEMORY);
    pFn->u.HostCall.cParms = cParms;

    RTListAppend(&pSvc->lstCall, &pFn->Node);
    pFn = NULL; /* Thread takes ownership now. */

    int rc2 = RTSemEventSignal(pSvc->hEventQueue);
    AssertRC(rc2);

    rc2 = RTSemEventWait(pSvc->hEventHostCall, RT_INDEFINITE_WAIT);
    AssertRCReturn(rc2, rc2);

    return VINF_SUCCESS; /** @todo Return host call rc */
}

/** Call completion callback for guest calls. */
static DECLCALLBACK(int) tstHgcmMockSvcCallComplete(VBOXHGCMCALLHANDLE callHandle, int32_t rc)
{
    PTSTHGCMMOCKSVC pSvc = &s_tstHgcmSvc;

    for (size_t i = 0; RT_ELEMENTS(pSvc->aHgcmClient); i++)
    {
        PTSTHGCMMOCKCLIENT pClient = &pSvc->aHgcmClient[i];
        if (&pClient->hCall == callHandle) /* Slow, but works for now. */
        {
            if (rc == VINF_HGCM_ASYNC_EXECUTE)
            {
                Assert(pClient->fAsyncExec == false);
            }
            else /* Complete call + notify client. */
            {
                callHandle->rc = rc;

                int rc2 = RTSemEventSignal(pClient->hEvent);
                AssertRCReturn(rc2, rc2);
            }

            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

static DECLCALLBACK(int) tstHgcmMockSvcThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);
    PTSTHGCMMOCKSVC pSvc = (PTSTHGCMMOCKSVC)pvUser;

    pSvc->uNextClientId  = 0;
    pSvc->fnTable.cbSize     = sizeof(pSvc->fnTable);
    pSvc->fnTable.u32Version = VBOX_HGCM_SVC_VERSION;

    RT_ZERO(pSvc->fnHelpers);
    pSvc->fnHelpers.pfnCallComplete = tstHgcmMockSvcCallComplete;
    pSvc->fnTable.pHelpers          = &pSvc->fnHelpers;

    int rc = VBoxHGCMSvcLoad(&pSvc->fnTable);
    if (RT_SUCCESS(rc))
    {
        RTThreadUserSignal(hThread);

        for (;;)
        {
            rc = RTSemEventWait(pSvc->hEventQueue, 10 /* ms */);
            if (ASMAtomicReadBool(&pSvc->fShutdown))
            {
                rc = VINF_SUCCESS;
                break;
            }
            if (rc == VERR_TIMEOUT)
                continue;

            PTSTHGCMMOCKFN pFn = RTListGetFirst(&pSvc->lstCall, TSTHGCMMOCKFN, Node);
            if (pFn)
            {
                switch (pFn->enmType)
                {
                    case TSTHGCMMOCKFNTYPE_CONNECT:
                    {
                        rc = pSvc->fnTable.pfnConnect(pSvc->fnTable.pvService,
                                                      pFn->pClient->idClient, &pFn->pClient->Client,
                                                      VMMDEV_REQUESTOR_USR_NOT_GIVEN /* fRequestor */, false /* fRestoring */);

                        int rc2 = RTSemEventSignal(pFn->pClient->hEvent);
                        AssertRC(rc2);

                        break;
                    }

                    case TSTHGCMMOCKFNTYPE_DISCONNECT:
                    {
                        rc = pSvc->fnTable.pfnDisconnect(pSvc->fnTable.pvService,
                                                         pFn->pClient->idClient, &pFn->pClient->Client);
                        break;
                    }

                    case TSTHGCMMOCKFNTYPE_CALL:
                    {
                        pSvc->fnTable.pfnCall(NULL, pFn->u.Call.hCall, pFn->pClient->idClient, &pFn->pClient->Client,
                                              pFn->u.Call.iFunc, pFn->u.Call.cParms, pFn->u.Call.pParms, RTTimeMilliTS());

                        /* Note: Call will be completed in the call completion callback. */
                        break;
                    }

                    case TSTHGCMMOCKFNTYPE_HOST_CALL:
                    {
                        rc = pSvc->fnTable.pfnHostCall(NULL, pFn->u.HostCall.iFunc, pFn->u.HostCall.cParms, pFn->u.HostCall.pParms);

                        int rc2 = RTSemEventSignal(pSvc->hEventHostCall);
                        AssertRC(rc2);
                        break;
                    }

                    default:
                        AssertFailed();
                        break;
                }
                RTListNodeRemove(&pFn->Node);
                RTMemFree(pFn);
            }
        }
    }

    return rc;
}

static PTSTHGCMMOCKCLIENT tstHgcmMockSvcWaitForConnect(PTSTHGCMMOCKSVC pSvc)
{
    int rc = RTSemEventWait(pSvc->hEventWait, RT_MS_30SEC);
    if (RT_SUCCESS(rc))
    {
        Assert(pSvc->uNextClientId);
        return &pSvc->aHgcmClient[pSvc->uNextClientId - 1];
    }
    return NULL;
}

static int tstHgcmMockSvcCreate(PTSTHGCMMOCKSVC pSvc)
{
    RT_ZERO(pSvc->aHgcmClient);
    pSvc->fShutdown = false;
    int rc = RTSemEventCreate(&pSvc->hEventQueue);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pSvc->hEventHostCall);
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventCreate(&pSvc->hEventWait);
            if (RT_SUCCESS(rc))
                RTListInit(&pSvc->lstCall);
        }
    }

    return rc;
}

static int tstHgcmMockSvcDestroy(PTSTHGCMMOCKSVC pSvc)
{
    int rc = RTSemEventDestroy(pSvc->hEventQueue);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventDestroy(pSvc->hEventHostCall);
        if (RT_SUCCESS(rc))
            RTSemEventDestroy(pSvc->hEventWait);
    }
    return rc;
}

static int tstHgcmMockSvcStart(PTSTHGCMMOCKSVC pSvc)
{
    int rc = RTThreadCreate(&pSvc->hThread, tstHgcmMockSvcThread, pSvc, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "MockSvc");
    if (RT_SUCCESS(rc))
        rc = RTThreadUserWait(pSvc->hThread, RT_MS_30SEC);

    return rc;
}

static int tstHgcmMockSvcStop(PTSTHGCMMOCKSVC pSvc)
{
    ASMAtomicWriteBool(&pSvc->fShutdown, true);

    int rcThread;
    int rc = RTThreadWait(pSvc->hThread, RT_MS_30SEC, &rcThread);
    if (RT_SUCCESS(rc))
        rc = rcThread;
    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Shutting down mock service failed with %Rrc\n", rc);

    pSvc->hThread = NIL_RTTHREAD;

    return rc;
}

VBGLR3DECL(int) VbglR3HGCMConnect(const char *pszServiceName, HGCMCLIENTID *pidClient)
{
    RT_NOREF(pszServiceName);

    PTSTHGCMMOCKSVC pSvc = &s_tstHgcmSvc;

    return tstHgcmMockSvcConnect(pSvc, pSvc->fnTable.pvService, pidClient);
}

VBGLR3DECL(int) VbglR3HGCMDisconnect(HGCMCLIENTID idClient)
{
    PTSTHGCMMOCKSVC pSvc = &s_tstHgcmSvc;

    return tstHgcmMockSvcDisconnect(pSvc, pSvc->fnTable.pvService, idClient);
}

VBGLR3DECL(int) VbglR3HGCMCall(PVBGLIOCHGCMCALL pInfo, size_t cbInfo)
{
    AssertMsg(pInfo->Hdr.cbIn  == cbInfo, ("cbIn=%#x cbInfo=%#zx\n", pInfo->Hdr.cbIn, cbInfo));
    AssertMsg(pInfo->Hdr.cbOut == cbInfo, ("cbOut=%#x cbInfo=%#zx\n", pInfo->Hdr.cbOut, cbInfo));
    Assert(sizeof(*pInfo) + pInfo->cParms * sizeof(HGCMFunctionParameter) <= cbInfo);

    HGCMFunctionParameter *offSrcParms = VBGL_HGCM_GET_CALL_PARMS(pInfo);
    PVBOXHGCMSVCPARM       paDstParms  = (PVBOXHGCMSVCPARM)RTMemAlloc(pInfo->cParms * sizeof(VBOXHGCMSVCPARM));
    for (uint16_t i = 0; i < pInfo->cParms; i++)
    {
        switch (offSrcParms->type)
        {
            case VMMDevHGCMParmType_32bit:
            {
                paDstParms[i].type     = VBOX_HGCM_SVC_PARM_32BIT;
                paDstParms[i].u.uint32 = offSrcParms->u.value32;
                break;
            }

            case VMMDevHGCMParmType_64bit:
            {
                paDstParms[i].type     = VBOX_HGCM_SVC_PARM_64BIT;
                paDstParms[i].u.uint64 = offSrcParms->u.value64;
                break;
            }

            case VMMDevHGCMParmType_LinAddr:
            {
                paDstParms[i].type           = VBOX_HGCM_SVC_PARM_PTR;
                paDstParms[i].u.pointer.addr = (void *)offSrcParms->u.LinAddr.uAddr;
                paDstParms[i].u.pointer.size = offSrcParms->u.LinAddr.cb;
                break;
            }

            default:
                AssertFailed();
                break;
        }

        offSrcParms++;
    }

    PTSTHGCMMOCKSVC const pSvc = &s_tstHgcmSvc;

    int rc2 = tstHgcmMockSvcCall(pSvc, pSvc->fnTable.pvService, &pSvc->aHgcmClient[pInfo->u32ClientID].hCall,
                                 pInfo->u32ClientID, &pSvc->aHgcmClient[pInfo->u32ClientID].Client,
                                 pInfo->u32Function, pInfo->cParms, paDstParms);
    if (RT_SUCCESS(rc2))
    {
        offSrcParms = VBGL_HGCM_GET_CALL_PARMS(pInfo);

        for (uint16_t i = 0; i < pInfo->cParms; i++)
        {
            paDstParms[i].type = offSrcParms->type;
            switch (paDstParms[i].type)
            {
                case VMMDevHGCMParmType_32bit:
                    offSrcParms->u.value32 = paDstParms[i].u.uint32;
                    break;

                case VMMDevHGCMParmType_64bit:
                    offSrcParms->u.value64 = paDstParms[i].u.uint64;
                    break;

                case VMMDevHGCMParmType_LinAddr:
                {
                    offSrcParms->u.LinAddr.cb = paDstParms[i].u.pointer.size;
                    break;
                }

                default:
                    AssertFailed();
                    break;
            }

            offSrcParms++;
        }
    }

    RTMemFree(paDstParms);

    if (RT_SUCCESS(rc2))
        rc2 = pSvc->aHgcmClient[pInfo->u32ClientID].hCall.rc;

    return rc2;
}

RT_C_DECLS_END


/*********************************************************************************************************************************
*   Shared Clipboard testing                                                                                                     *
*********************************************************************************************************************************/

typedef struct TESTTASK
{
    RTSEMEVENT  hEvent;
    int         rcCompleted;
    int         rcExpected;
    SHCLFORMATS enmFmtHst;
    SHCLFORMATS enmFmtGst;
    /** For chunked reads / writes. */
    size_t      cbChunk;
    size_t      cbData;
    void       *pvData;
} TESTTASK;
typedef TESTTASK *PTESTTASK;

/**
 * Structure for keeping a test context.
 */
typedef struct TESTCTX
{
    PTSTHGCMMOCKSVC      pSvc;
    /** Currently we only support one task at a time. */
    TESTTASK             Task;
    struct
    {
        RTTHREAD         hThread;
        VBGLR3SHCLCMDCTX CmdCtx;
        volatile bool    fShutdown;
        PFNTESTGSTTHREAD pfnThread;
    } Guest;
    struct
    {
        RTTHREAD         hThread;
        volatile bool    fShutdown;
    } Host;
} TESTCTX;

/** The one and only test context. */
TESTCTX  g_TstCtx;

/**
 * Test parameters.
 */
typedef struct TESTPARMS
{
    /** Pointer to test context to use. */
    PTESTCTX             pTstCtx;
} TESTPARMS;

typedef struct TESTDESC
{
    /** The setup callback. */
    PFNTESTSETUP         pfnSetup;
    /** The exec callback. */
    PFNTESTEXEC          pfnExec;
    /** The destruction callback. */
    PFNTESTDESTROY       pfnDestroy;
} TESTDESC;

typedef struct SHCLCONTEXT
{
} SHCLCONTEXT;

static int tstSetModeRc(PTSTHGCMMOCKSVC pSvc, uint32_t uMode, int rc)
{
    VBOXHGCMSVCPARM aParms[2];
    HGCMSvcSetU32(&aParms[0], uMode);
    int rc2 = tstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, aParms);
    RTTESTI_CHECK_MSG_RET(rc == rc2, ("Expected %Rrc, got %Rrc\n", rc, rc2), rc2);
    uint32_t const uModeRet = ShClSvcGetMode();
    RTTESTI_CHECK_MSG_RET(uMode == uModeRet, ("Expected mode %RU32, got %RU32\n", uMode, uModeRet), VERR_WRONG_TYPE);
    return rc2;
}

static int tstSetMode(PTSTHGCMMOCKSVC pSvc, uint32_t uMode)
{
    return tstSetModeRc(pSvc, uMode, VINF_SUCCESS);
}

static bool tstGetMode(PTSTHGCMMOCKSVC pSvc, uint32_t uModeExpected)
{
    RT_NOREF(pSvc);
    RTTESTI_CHECK_RET(ShClSvcGetMode() == uModeExpected, false);
    return true;
}

static void tstOperationModes(void)
{
    struct VBOXHGCMSVCPARM parms[2];
    uint32_t u32Mode;
    int rc;

    RTTestISub("Testing VBOX_SHCL_HOST_FN_SET_MODE");

    PTSTHGCMMOCKSVC pSvc = &s_tstHgcmSvc;

    /* Reset global variable which doesn't reset itself. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_MODE_OFF);
    rc = tstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    u32Mode = ShClSvcGetMode();
    RTTESTI_CHECK_MSG(u32Mode == VBOX_SHCL_MODE_OFF, ("u32Mode=%u\n", (unsigned) u32Mode));

    rc = tstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 0, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    rc = tstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 2, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    HGCMSvcSetU64(&parms[0], 99);
    rc = tstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    tstSetMode(pSvc, VBOX_SHCL_MODE_HOST_TO_GUEST);
    tstSetModeRc(pSvc, 99, VERR_NOT_SUPPORTED);
    tstGetMode(pSvc, VBOX_SHCL_MODE_OFF);
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
static void testSetTransferMode(void)
{
    RTTestISub("Testing VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE");

    PTSTHGCMMOCKSVC pSvc = &s_tstHgcmSvc;

    /* Invalid parameter. */
    VBOXHGCMSVCPARM parms[2];
    HGCMSvcSetU64(&parms[0], 99);
    int rc = tstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    /* Invalid mode. */
    HGCMSvcSetU32(&parms[0], 99);
    rc = tstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_FLAGS);

    /* Enable transfers. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_TRANSFER_MODE_ENABLED);
    rc = tstHgcmMockSvcHostCall(pSvc, NULL, VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

    /* Disable transfers again. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_TRANSFER_MODE_DISABLED);
    rc = s_tstHgcmSvc.fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

/* Does testing of VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, needed for providing compatibility to older Guest Additions clients. */
static void testHostGetMsgOld(void)
{
    RTTestISub("Setting up VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT test");

    VBOXHGCMSVCPARM parms[2];
    RT_ZERO(parms);

    /* Unless we are bidirectional the host message requests will be dropped. */
    HGCMSvcSetU32(&parms[0], VBOX_SHCL_MODE_BIDIRECTIONAL);
    int rc = s_tstHgcmSvc.fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);

    RTTestISub("Testing one format, waiting guest u.Call.");
    RT_ZERO(g_Client);
    VBOXHGCMCALLHANDLE_TYPEDEF call;
    rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSvc.fnTable.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);

    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    s_tstHgcmSvc.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC_OK(rc);

    //testMsgAddReadData(&g_Client, VBOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_UNICODETEXT);
#if 0
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VINF_SUCCESS);
    s_tstHgcmSrv.fnTable.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing one format, no waiting guest calls.");
    RT_ZERO(g_Client);
    s_tstHgcmSrv.fnTable.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    testMsgAddReadData(&g_Client, VBOX_SHCL_FMT_HTML);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VINF_SUCCESS);
    s_tstHgcmSrv.fnTable.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing two formats, waiting guest u.Call.");
    RT_ZERO(g_Client);
    s_tstHgcmSrv.fnTable.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This should get updated only when the guest call completes. */
    testMsgAddReadData(&g_Client, VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_HTML);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This call should not complete yet. */
    s_tstHgcmSrv.fnTable.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing two formats, no waiting guest calls.");
    RT_ZERO(g_Client);
    s_tstHgcmSrv.fnTable.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    testMsgAddReadData(&g_Client, VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_HTML);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VBOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VBOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(u.Call.rc);
    u.Call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    s_tstHgcmSrv.fnTable.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(u.Call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This call should not complete yet. */
#endif
    s_tstHgcmSvc.fnTable.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);
}

static void testGuestSimple(void)
{
    RTTestISub("Testing client (guest) API - Simple");

    PTSTHGCMMOCKSVC pSvc = &s_tstHgcmSvc;

    /* Preparations. */
    VBGLR3SHCLCMDCTX Ctx;
    RT_ZERO(Ctx);

    /*
     * Multiple connects / disconnects.
     */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardConnectEx(&Ctx, VBOX_SHCL_GF_0_CONTEXT_ID));
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardDisconnectEx(&Ctx));
    /* Report bogus guest features while connecting. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardConnectEx(&Ctx, 0xdeadbeef));
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardDisconnectEx(&Ctx));

    RTTESTI_CHECK_RC_OK(VbglR3ClipboardConnectEx(&Ctx, VBOX_SHCL_GF_0_CONTEXT_ID));

    /*
     * Feature tests.
     */

    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFeatures(Ctx.idClient, 0x0,        NULL /* pfHostFeatures */));
    /* Report bogus features to the host. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFeatures(Ctx.idClient, 0xdeadb33f, NULL /* pfHostFeatures */));

    /*
     * Access denied tests.
     */

    /* Try reading data from host. */
    uint8_t abData[32]; uint32_t cbIgnored;
    RTTESTI_CHECK_RC(VbglR3ClipboardReadData(Ctx.idClient, VBOX_SHCL_FMT_UNICODETEXT,
                                             abData, sizeof(abData), &cbIgnored), VERR_ACCESS_DENIED);
    /* Try writing data without reporting formats before (legacy). */
    RTTESTI_CHECK_RC(VbglR3ClipboardWriteData(Ctx.idClient, 0xdeadb33f, abData, sizeof(abData)), VERR_ACCESS_DENIED);
    /* Try writing data without reporting formats before. */
    RTTESTI_CHECK_RC(VbglR3ClipboardWriteDataEx(&Ctx, 0xdeadb33f, abData, sizeof(abData)), VERR_ACCESS_DENIED);
    /* Report bogus formats to the host. */
    RTTESTI_CHECK_RC(VbglR3ClipboardReportFormats(Ctx.idClient, 0xdeadb33f), VERR_ACCESS_DENIED);
    /* Report supported formats to host. */
    RTTESTI_CHECK_RC(VbglR3ClipboardReportFormats(Ctx.idClient,
                                                  VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_BITMAP | VBOX_SHCL_FMT_HTML),
                                                  VERR_ACCESS_DENIED);
    /*
     * Access allowed tests.
     */
    tstSetMode(pSvc, VBOX_SHCL_MODE_BIDIRECTIONAL);

    /* Try writing data without reporting formats before. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardWriteDataEx(&Ctx, 0xdeadb33f, abData, sizeof(abData)));
    /* Try reading data from host. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReadData(Ctx.idClient, VBOX_SHCL_FMT_UNICODETEXT,
                                                abData, sizeof(abData), &cbIgnored));
    /* Report bogus formats to the host. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFormats(Ctx.idClient, 0xdeadb33f));
    /* Report supported formats to host. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFormats(Ctx.idClient,
                                                     VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_BITMAP | VBOX_SHCL_FMT_HTML));
    /* Tear down. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardDisconnectEx(&Ctx));
}

static void testGuestWrite(void)
{
    RTTestISub("Testing client (guest) API - Writing");
}

#if 0
/**
 * Generate a random codepoint for simple UTF-16 encoding.
 */
static RTUTF16 tstGetRandUtf16(void)
{
    RTUTF16 wc;
    do
    {
        wc = (RTUTF16)RTRandU32Ex(1, 0xfffd);
    } while (wc >= 0xd800 && wc <= 0xdfff);
    return wc;
}

static PRTUTF16 tstGenerateUtf16StringA(uint32_t uCch)
{
    PRTUTF16 pwszRand = (PRTUTF16)RTMemAlloc((uCch + 1) * sizeof(RTUTF16));
    for (uint32_t i = 0; i < uCch; i++)
        pwszRand[i] = tstGetRandUtf16();
    pwszRand[uCch] = 0;
    return pwszRand;
}
#endif

#if 0
static void testGuestRead(void)
{
    RTTestISub("Testing client (guest) API - Reading");

    /* Preparations. */
    tstSetMode(VBOX_SHCL_MODE_BIDIRECTIONAL);

    VBGLR3SHCLCMDCTX Ctx;
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardConnectEx(&Ctx, VBOX_SHCL_GF_0_CONTEXT_ID));
    RTThreadSleep(500); /** @todo BUGBUG -- Seems to be a startup race when querying the initial clipboard formats. */

    uint8_t abData[_4K]; uint32_t cbData; uint32_t cbRead;

    /* Issue a host request that we want to read clipboard data from the guest. */
    PSHCLEVENT pEvent;
    tstSvcMockRequestDataFromGuest(Ctx.idClient, VBOX_SHCL_FMT_UNICODETEXT, &pEvent);

    /* Write guest clipboard data to the host side. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardReportFormats(Ctx.idClient, VBOX_SHCL_FMT_UNICODETEXT));
    cbData = RTRandU32Ex(1, sizeof(abData));
    PRTUTF16 pwszStr = tstGenerateUtf16String(cbData);
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardWriteDataEx(&Ctx, VBOX_SHCL_FMT_UNICODETEXT, pwszStr, cbData));
    RTMemFree(pwszStr);

    PSHCLEVENTPAYLOAD pPayload;
    int rc = ShClEventWait(pEvent, RT_MS_30SEC, &pPayload);
    if (RT_SUCCESS(rc))
    {

    }
    ShClEventRelease(pEvent);
    pEvent = NULL;


    /* Read clipboard data from the host back to the guest side. */
    /* Note: Also could return VINF_BUFFER_OVERFLOW, so check for VINF_SUCCESS explicitly here. */
    RTTESTI_CHECK_RC(VbglR3ClipboardReadDataEx(&Ctx, VBOX_SHCL_FMT_UNICODETEXT,
                                               abData, sizeof(abData), &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK(cbRead == cbData);

    RTPrintf("Data (%RU32): %ls\n", cbRead, (PCRTUTF16)abData);

    /* Tear down. */
    RTTESTI_CHECK_RC_OK(VbglR3ClipboardDisconnectEx(&Ctx));
}
#endif

static DECLCALLBACK(int) tstGuestThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);
    PTESTCTX pCtx = (PTESTCTX)pvUser;
    AssertPtr(pCtx);

    RTThreadUserSignal(hThread);

    if (pCtx->Guest.pfnThread)
        return pCtx->Guest.pfnThread(pCtx, NULL);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstHostThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);
    PTESTCTX pCtx = (PTESTCTX)pvUser;
    AssertPtr(pCtx);

    int rc = VINF_SUCCESS;

    RTThreadUserSignal(hThread);

    for (;;)
    {
        RTThreadSleep(100);

        if (ASMAtomicReadBool(&pCtx->Host.fShutdown))
            break;
    }

    return rc;
}

static void testSetHeadless(void)
{
    RTTestISub("Testing HOST_FN_SET_HEADLESS");

    VBOXHGCMSVCPARM parms[2];
    HGCMSvcSetU32(&parms[0], false);
    int rc = s_tstHgcmSvc.fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    bool fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == false, ("fHeadless=%RTbool\n", fHeadless));
    rc = s_tstHgcmSvc.fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 0, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    rc = s_tstHgcmSvc.fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 2, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    HGCMSvcSetU64(&parms[0], 99);
    rc = s_tstHgcmSvc.fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    HGCMSvcSetU32(&parms[0], true);
    rc = s_tstHgcmSvc.fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == true, ("fHeadless=%RTbool\n", fHeadless));
    HGCMSvcSetU32(&parms[0], 99);
    rc = s_tstHgcmSvc.fnTable.pfnHostCall(NULL, VBOX_SHCL_HOST_FN_SET_HEADLESS, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == true, ("fHeadless=%RTbool\n", fHeadless));
}

static void testHostCall(void)
{
    tstOperationModes();
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    testSetTransferMode();
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */
    testSetHeadless();
}

static int tstGuestStart(PTESTCTX pTstCtx, PFNTESTGSTTHREAD pFnThread)
{
    pTstCtx->Guest.pfnThread = pFnThread;

    int rc = RTThreadCreate(&pTstCtx->Guest.hThread, tstGuestThread, pTstCtx, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "tstShClGst");
    if (RT_SUCCESS(rc))
        rc = RTThreadUserWait(pTstCtx->Guest.hThread, RT_MS_30SEC);

    return rc;
}

static int tstGuestStop(PTESTCTX pTstCtx)
{
    ASMAtomicWriteBool(&pTstCtx->Guest.fShutdown, true);

    int rcThread;
    int rc = RTThreadWait(pTstCtx->Guest.hThread, RT_MS_30SEC, &rcThread);
    if (RT_SUCCESS(rc))
        rc = rcThread;
    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Shutting down guest thread failed with %Rrc\n", rc);

    pTstCtx->Guest.hThread = NIL_RTTHREAD;

    return rc;
}

static int tstHostStart(PTESTCTX pTstCtx)
{
    int rc = RTThreadCreate(&pTstCtx->Host.hThread, tstHostThread, pTstCtx, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "tstShClHst");
    if (RT_SUCCESS(rc))
        rc = RTThreadUserWait(pTstCtx->Host.hThread, RT_MS_30SEC);

    return rc;
}

static int tstHostStop(PTESTCTX pTstCtx)
{
    ASMAtomicWriteBool(&pTstCtx->Host.fShutdown, true);

    int rcThread;
    int rc = RTThreadWait(pTstCtx->Host.hThread, RT_MS_30SEC, &rcThread);
    if (RT_SUCCESS(rc))
        rc = rcThread;
    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Shutting down host thread failed with %Rrc\n", rc);

    pTstCtx->Host.hThread = NIL_RTTHREAD;

    return rc;
}

#if defined(RT_OS_LINUX)
static DECLCALLBACK(int) tstShClUserMockReportFormatsCallback(PSHCLCONTEXT pCtx, uint32_t fFormats, void *pvUser)
{
    RT_NOREF(pCtx, fFormats, pvUser);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "tstShClUserMockReportFormatsCallback: fFormats=%#x\n", fFormats);
    return VINF_SUCCESS;
}

/*
static DECLCALLBACK(int) tstTestReadFromHost_RequestDataFromSourceCallback(PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, uint32_t *pcb, void *pvUser)
{
    RT_NOREF(pCtx, uFmt, ppv, pvUser);

    PTESTTASK pTask = &TaskRead;

    uint8_t *pvData = (uint8_t *)RTMemDup(pTask->pvData, pTask->cbData);

    *ppv = pvData;
    *pcb = pTask->cbData;

    return VINF_SUCCESS;
}
*/

#if 0
static DECLCALLBACK(int) tstShClUserMockSendDataCallback(PSHCLCONTEXT pCtx, void *pv, uint32_t cb, void *pvUser)
{
    RT_NOREF(pCtx, pv, cb, pvUser);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "tstShClUserMockSendDataCallback\n");

    PTESTTASK pTask = &TaskRead;

    memcpy(pv, pTask->pvData, RT_MIN(pTask->cbData, cb));

    return VINF_SUCCESS;
}
#endif

static DECLCALLBACK(int) tstShClUserMockOnGetDataCallback(PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, size_t *pcb, void *pvUser)
{
    RT_NOREF(pCtx, uFmt, pvUser);

    PTESTTASK pTask = &g_TstCtx.Task;

    uint8_t *pvData = pTask->cbData ? (uint8_t *)RTMemDup(pTask->pvData, pTask->cbData) : NULL;
    size_t   cbData = pTask->cbData;

    *ppv = pvData;
    *pcb = cbData;

    return VINF_SUCCESS;
}
#endif /* RT_OS_LINUX */

typedef struct TSTUSERMOCK
{
#if defined(RT_OS_LINUX)
    SHCLX11CTX   X11Ctx;
#endif
    PSHCLCONTEXT pCtx;
} TSTUSERMOCK;
typedef TSTUSERMOCK *PTSTUSERMOCK;

static void tstShClUserMockInit(PTSTUSERMOCK pUsrMock, const char *pszName)
{
#if defined(RT_OS_LINUX)
    SHCLCALLBACKS Callbacks;
    RT_ZERO(Callbacks);
    Callbacks.pfnReportFormats   = tstShClUserMockReportFormatsCallback;
    Callbacks.pfnOnClipboardRead = tstShClUserMockOnGetDataCallback;

    pUsrMock->pCtx = (PSHCLCONTEXT)RTMemAllocZ(sizeof(SHCLCONTEXT));
    AssertPtrReturnVoid(pUsrMock->pCtx);

    ShClX11Init(&pUsrMock->X11Ctx, &Callbacks, pUsrMock->pCtx, false);
    ShClX11ThreadStartEx(&pUsrMock->X11Ctx, pszName, false /* fGrab */);
    /* Give the clipboard time to synchronise. */
    RTThreadSleep(500);
#else
    RT_NOREF(pUsrMock);
#endif /* RT_OS_LINUX */
}

static void tstShClUserMockDestroy(PTSTUSERMOCK pUsrMock)
{
#if defined(RT_OS_LINUX)
    ShClX11ThreadStop(&pUsrMock->X11Ctx);
    ShClX11Destroy(&pUsrMock->X11Ctx);
    RTMemFree(pUsrMock->pCtx);
#else
    RT_NOREF(pUsrMock);
#endif
}

static int tstTaskGuestRead(PTESTCTX pCtx, PTESTTASK pTask)
{
    size_t   cbReadTotal = 0;
    size_t   cbToRead    = pTask->cbData;

    switch (pTask->enmFmtGst)
    {
        case VBOX_SHCL_FMT_UNICODETEXT:
            cbToRead *= sizeof(RTUTF16);
            break;

        default:
            break;
    }

    size_t   cbDst       = _64K;
    uint8_t *pabDst      = (uint8_t *)RTMemAllocZ(cbDst);
    AssertPtrReturn(pabDst, VERR_NO_MEMORY);

    Assert(pTask->cbChunk);                  /* Buggy test? */
    Assert(pTask->cbChunk <= pTask->cbData); /* Ditto. */

    uint8_t *pabSrc = (uint8_t *)pTask->pvData;

    do
    {
        /* Note! VbglR3ClipboardReadData() currently does not support chunked reads!
          *      It in turn returns VINF_BUFFER_OVERFLOW when the supplied buffer was too small. */
        uint32_t const cbChunk    = cbDst;
        uint32_t const cbExpected = cbToRead;

        uint32_t cbRead = 0;
        RTTEST_CHECK_RC(g_hTest, VbglR3ClipboardReadData(pCtx->Guest.CmdCtx.idClient,
                                                         pTask->enmFmtGst, pabDst, cbChunk, &cbRead), pTask->rcExpected);
        RTTEST_CHECK_MSG(g_hTest, cbRead == cbExpected, (g_hTest, "Read %RU32 bytes, expected %RU32\n", cbRead, cbExpected));
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Guest side received %RU32 bytes\n", cbRead);
        cbReadTotal += cbRead;
        Assert(cbReadTotal <= cbToRead);

    } while (cbReadTotal < cbToRead);

    if (pTask->enmFmtGst == VBOX_SHCL_FMT_UNICODETEXT)
    {
        RTTEST_CHECK_RC_OK(g_hTest, RTUtf16ValidateEncoding((PRTUTF16)pabDst));
    }
    else
        RTTEST_CHECK(g_hTest, memcmp(pabSrc, pabDst, RT_MIN(pTask->cbData, cbDst) == 0));

    RTMemFree(pabDst);

    return VINF_SUCCESS;
}

static void tstTaskInit(PTESTTASK pTask)
{
    RTSemEventCreate(&pTask->hEvent);
}

static void tstTaskDestroy(PTESTTASK pTask)
{
    RTSemEventDestroy(pTask->hEvent);
}

static void tstTaskWait(PTESTTASK pTask, RTMSINTERVAL msTimeout)
{
    RTTEST_CHECK_RC_OK(g_hTest, RTSemEventWait(pTask->hEvent, msTimeout));
    RTTEST_CHECK_RC(g_hTest, pTask->rcCompleted, pTask->rcExpected);
}

static void tstTaskSignal(PTESTTASK pTask, int rc)
{
    pTask->rcCompleted = rc;
    RTTEST_CHECK_RC_OK(g_hTest, RTSemEventSignal(pTask->hEvent));
}

static DECLCALLBACK(int) tstTestReadFromHostThreadGuest(PTESTCTX pCtx, void *pvCtx)
{
    RT_NOREF(pvCtx);

    RTThreadSleep(5000);

    RT_ZERO(pCtx->Guest.CmdCtx);
    RTTEST_CHECK_RC_OK(g_hTest, VbglR3ClipboardConnectEx(&pCtx->Guest.CmdCtx, VBOX_SHCL_GF_0_CONTEXT_ID));

#if 1
    PTESTTASK pTask = &pCtx->Task;
    tstTaskGuestRead(pCtx, pTask);
    tstTaskSignal(pTask, VINF_SUCCESS);
#endif

#if 0
    for (;;)
    {
        PVBGLR3CLIPBOARDEVENT pEvent = (PVBGLR3CLIPBOARDEVENT)RTMemAllocZ(sizeof(VBGLR3CLIPBOARDEVENT));
        AssertPtrBreakStmt(pEvent, rc = VERR_NO_MEMORY);

        uint32_t idMsg  = 0;
        uint32_t cParms = 0;
        RTTEST_CHECK_RC_OK(g_hTest, VbglR3ClipboardMsgPeekWait(&pCtx->Guest.CmdCtx, &idMsg, &cParms, NULL /* pidRestoreCheck */));
        RTTEST_CHECK_RC_OK(g_hTest, VbglR3ClipboardEventGetNext(idMsg, cParms, &pCtx->Guest.CmdCtx, pEvent));

        if (pEvent)
        {
            VbglR3ClipboardEventFree(pEvent);
            pEvent = NULL;
        }

        if (ASMAtomicReadBool(&pCtx->Guest.fShutdown))
            break;

        RTThreadSleep(100);
    }
#endif

    RTTEST_CHECK_RC_OK(g_hTest, VbglR3ClipboardDisconnectEx(&pCtx->Guest.CmdCtx));

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstTestReadFromHostExec(PTESTPARMS pTstParms, void *pvCtx)
{
    RT_NOREF(pvCtx, pTstParms);

    PTESTTASK pTask = &pTstParms->pTstCtx->Task;

    pTask->enmFmtGst = VBOX_SHCL_FMT_UNICODETEXT;
    pTask->enmFmtHst = pTask->enmFmtGst;
    pTask->pvData    = RTStrAPrintf2("foo!");
    pTask->cbData    = strlen((char *)pTask->pvData) + 1;
    pTask->cbChunk   = pTask->cbData;

    PTSTHGCMMOCKSVC    pSvc        = &s_tstHgcmSvc;
    PTSTHGCMMOCKCLIENT pMockClient = tstHgcmMockSvcWaitForConnect(pSvc);
    AssertPtrReturn(pMockClient, VERR_INVALID_POINTER);

    bool fUseMock = false;
    TSTUSERMOCK UsrMock;
    if (fUseMock)
        tstShClUserMockInit(&UsrMock, "tstX11Hst");

    RTThreadSleep(RT_MS_1SEC * 4);

#if 1
    PSHCLBACKEND pBackend = ShClSvcGetBackend();

    ShClBackendFormatAnnounce(pBackend, &pMockClient->Client, pTask->enmFmtHst);
    tstTaskWait(pTask, RT_MS_30SEC);
#endif

RTThreadSleep(RT_MS_30SEC);

    //PSHCLCLIENT pClient = &pMockClient->Client;

#if 1
    if (1)
    {
        //RTTEST_CHECK_RC_OK(g_hTest, ShClBackendMockSetData(pBackend, pTask->enmFmt, pwszStr, cbData));
        //RTMemFree(pwszStr);
    }
#endif

    if (fUseMock)
        tstShClUserMockDestroy(&UsrMock);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstTestReadFromHostSetup(PTESTPARMS pTstParms, void **ppvCtx)
{
    RT_NOREF(ppvCtx);

    PTESTCTX pCtx = pTstParms->pTstCtx;

    tstHostStart(pCtx);

    PSHCLBACKEND pBackend = ShClSvcGetBackend();

    SHCLCALLBACKS Callbacks;
    RT_ZERO(Callbacks);
    Callbacks.pfnReportFormats = tstShClUserMockReportFormatsCallback;
    //Callbacks.pfnOnRequestDataFromSource = tstTestReadFromHost_RequestDataFromSourceCallback;
    Callbacks.pfnOnClipboardRead     = tstShClUserMockOnGetDataCallback;
    ShClBackendSetCallbacks(pBackend, &Callbacks);

    tstGuestStart(pCtx, tstTestReadFromHostThreadGuest);

    RTThreadSleep(1000);

    tstSetMode(pCtx->pSvc, VBOX_SHCL_MODE_BIDIRECTIONAL);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstTestReadFromHostDestroy(PTESTPARMS pTstParms, void *pvCtx)
{
    RT_NOREF(pvCtx);

    int rc = VINF_SUCCESS;

    tstGuestStop(pTstParms->pTstCtx);
    tstHostStop(pTstParms->pTstCtx);

    return rc;
}

/** Test definition table. */
TESTDESC g_aTests[] =
{
    { tstTestReadFromHostSetup,       tstTestReadFromHostExec,      tstTestReadFromHostDestroy }
};
/** Number of tests defined. */
unsigned g_cTests = RT_ELEMENTS(g_aTests);

static int tstOne(PTSTHGCMMOCKSVC pSvc, PTESTDESC pTstDesc)
{
    PTESTCTX pTstCtx = &g_TstCtx;

    TESTPARMS TstParms;
    RT_ZERO(TstParms);

    pTstCtx->pSvc    = pSvc;
    TstParms.pTstCtx = pTstCtx;

    void *pvCtx;
    int rc = pTstDesc->pfnSetup(&TstParms, &pvCtx);
    if (RT_SUCCESS(rc))
    {
        rc = pTstDesc->pfnExec(&TstParms, pvCtx);

        int rc2 = pTstDesc->pfnDestroy(&TstParms, pvCtx);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

int main(int argc, char *argv[])
{
    /*
     * Init the runtime, test and say hello.
     */
    const char *pcszExecName;
    NOREF(argc);
    pcszExecName = strrchr(argv[0], '/');
    pcszExecName = pcszExecName ? pcszExecName + 1 : argv[0];
    RTEXITCODE rcExit = RTTestInitAndCreate(pcszExecName, &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    /* Don't let assertions in the host service panic (core dump) the test cases. */
    RTAssertSetMayPanic(false);

    /*
     * Run the tests.
     */
    testGuestSimple();
    testGuestWrite();
    testHostCall();
    testHostGetMsgOld();

    PTSTHGCMMOCKSVC pSvc = &s_tstHgcmSvc;

    tstHgcmMockSvcCreate(pSvc);
    tstHgcmMockSvcStart(pSvc);

    RT_ZERO(g_TstCtx);
    tstTaskInit(&g_TstCtx.Task);
    for (unsigned i = 0; i < RT_ELEMENTS(g_aTests); i++)
        tstOne(pSvc, &g_aTests[i]);
    tstTaskDestroy(&g_TstCtx.Task);

    tstHgcmMockSvcStop(pSvc);
    tstHgcmMockSvcDestroy(pSvc);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(g_hTest);
}

