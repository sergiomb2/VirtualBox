/* $Id$ */
/** @file
 * VRDE audio backend for Main.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include "LoggingNew.h"

#include <VBox/log.h>
#include "DrvAudioVRDE.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"

#include <iprt/mem.h>
#include <iprt/cdefs.h>
#include <iprt/circbuf.h>

#include <VBox/vmm/cfgm.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/err.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Audio VRDE driver instance data.
 */
typedef struct DRVAUDIOVRDE
{
    /** Pointer to audio VRDE object. */
    AudioVRDE           *pAudioVRDE;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS           pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO        IHostAudio;
    /** Pointer to the VRDP's console object. */
    ConsoleVRDPServer   *pConsoleVRDPServer;
    /** Pointer to the DrvAudio port interface that is above us. */
    PPDMIAUDIOCONNECTOR  pDrvAudio;
    /** Number of connected clients to this VRDE instance. */
    uint32_t             cClients;
} DRVAUDIOVRDE, *PDRVAUDIOVRDE;

typedef struct VRDESTREAM
{
    /** The stream's acquired configuration. */
    PPDMAUDIOSTREAMCFG pCfg;
    union
    {
        struct
        {
            /** Circular buffer for holding the recorded audio frames from the host. */
            PRTCIRCBUF  pCircBuf;
        } In;
    };
} VRDESTREAM, *PVRDESTREAM;

/* Sanity. */
AssertCompileSize(PDMAUDIOFRAME, sizeof(int64_t) * 2 /* st_sample_t using by VRDP server */);

static int vrdeCreateStreamIn(PVRDESTREAM pStreamVRDE, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pCfgReq);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    /*
     * The VRDP server does its own mixing and resampling as it may server
     * multiple clients all with different sound formats.  So, it feeds us
     * raw mixer frames (somewhat akind to stereo signed 64-bit, see
     * st_sample_t and PDMAUDIOFRAME).
     */
    pCfgAcq->enmLayout = PDMAUDIOSTREAMLAYOUT_RAW;
    PDMAudioPropsInitEx(&pCfgAcq->Props, 8 /*64-bit*/, true /*fSigned*/, 2 /*stereo*/, 22050 /*Hz*/,
                        true /*fLittleEndian*/, true /*fRaw*/);

    /* According to the VRDP docs, the VRDP server stores audio in 200ms chunks. */
    const uint32_t cFramesVrdpServer = PDMAudioPropsMilliToFrames(&pCfgAcq->Props, 200 /*ms*/);

    int rc = RTCircBufCreate(&pStreamVRDE->In.pCircBuf, PDMAudioPropsFramesToBytes(&pCfgAcq->Props, cFramesVrdpServer));
    if (RT_SUCCESS(rc))
    {
        pCfgAcq->Backend.cFramesPeriod          = cFramesVrdpServer;
/** @todo r=bird: This is inconsistent with the above buffer allocation and I
 * think also ALSA and Pulse backends way of setting cFramesBufferSize. */
        pCfgAcq->Backend.cFramesBufferSize      = cFramesVrdpServer * 2; /* Use "double buffering". */
        pCfgAcq->Backend.cFramesPreBuffering    = cFramesVrdpServer;
    }

    return rc;
}


static int vrdeCreateStreamOut(PVRDESTREAM pStreamVRDE, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pStreamVRDE, pCfgReq);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    /*
     * The VRDP server does its own mixing and resampling because it may be
     * sending the audio to any number of different clients all with different
     * formats (including clients which hasn't yet connected).  So, it desires
     * the raw data from the mixer (somewhat akind to stereo signed 64-bit,
     * see st_sample_t and PDMAUDIOFRAME).
     */
    pCfgAcq->enmLayout = PDMAUDIOSTREAMLAYOUT_RAW;
    PDMAudioPropsInitEx(&pCfgAcq->Props, 8 /*64-bit*/, true /*fSigned*/, 2 /*stereo*/, 22050 /*Hz*/,
                        true /*fLittleEndian*/, true /*fRaw*/);

    /* According to the VRDP docs, the VRDP server stores audio in 200ms chunks. */
    /** @todo r=bird: So, if VRDP does 200ms chunks, why do we report 100ms
     *        buffer and 20ms period?  How does these parameters at all correlate
     *        with the above comment?!? */
    pCfgAcq->Backend.cFramesPeriod       = PDMAudioPropsMilliToFrames(&pCfgAcq->Props, 20  /*ms*/);
    pCfgAcq->Backend.cFramesBufferSize   = PDMAudioPropsMilliToFrames(&pCfgAcq->Props, 100 /*ms*/);
    pCfgAcq->Backend.cFramesPreBuffering = pCfgAcq->Backend.cFramesPeriod * 2;

    return VINF_SUCCESS;
}


static int vrdeControlStreamOut(PDRVAUDIOVRDE pDrv, PVRDESTREAM pStreamVRDE, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(pDrv, pStreamVRDE, enmStreamCmd);

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    return VINF_SUCCESS;
}


static int vrdeControlStreamIn(PDRVAUDIOVRDE pDrv, PVRDESTREAM pStreamVRDE, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    if (!pDrv->pConsoleVRDPServer)
    {
        LogRel(("Audio: VRDP console not ready yet\n"));
        return VERR_AUDIO_STREAM_NOT_READY;
    }

    int rc;

    /* Initialize only if not already done. */
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            rc = pDrv->pConsoleVRDPServer->SendAudioInputBegin(NULL, pStreamVRDE,
                                                               PDMAudioPropsMilliToFrames(&pStreamVRDE->pCfg->Props, 200 /*ms*/),
                                                               PDMAudioPropsHz(&pStreamVRDE->pCfg->Props),
                                                               PDMAudioPropsChannels(&pStreamVRDE->pCfg->Props),
                                                               PDMAudioPropsSampleBits(&pStreamVRDE->pCfg->Props));
            if (rc == VERR_NOT_SUPPORTED)
            {
                LogRel(("Audio: No VRDE client connected, so no input recording available\n"));
                rc = VERR_AUDIO_STREAM_NOT_READY;
            }

            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL /* pvUserCtx */);
            rc = VINF_SUCCESS;

            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            rc = VINF_SUCCESS;
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            rc = VINF_SUCCESS;
            break;
        }

        default:
        {
            rc = VERR_NOT_SUPPORTED;
            break;
        }
    }

    if (RT_FAILURE(rc))
        LogFunc(("Failed with %Rrc\n", rc));

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvAudioVrdeHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                      void *pvBuf, uint32_t uBufSize, uint32_t *puRead)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(uBufSize,         VERR_INVALID_PARAMETER);
    /* puRead is optional. */

    PVRDESTREAM pStreamVRDE = (PVRDESTREAM)pStream;

    size_t cbData = 0;

    if (RTCircBufUsed(pStreamVRDE->In.pCircBuf))
    {
        void *pvData;

        RTCircBufAcquireReadBlock(pStreamVRDE->In.pCircBuf, uBufSize, &pvData, &cbData);

        if (cbData)
            memcpy(pvBuf, pvData, cbData);

        RTCircBufReleaseReadBlock(pStreamVRDE->In.pCircBuf, cbData);
    }

    if (puRead)
        *puRead = (uint32_t)cbData;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvAudioVrdeHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                   const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    AssertPtr(pDrv);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    PVRDESTREAM pStreamVRDE = (PVRDESTREAM)pStream;
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);

    if (!pDrv->pConsoleVRDPServer)
        return VERR_NOT_AVAILABLE;

    /* Prepate the format. */
    PPDMAUDIOPCMPROPS pProps = &pStreamVRDE->pCfg->Props;
    VRDEAUDIOFORMAT const uVrdpFormat = VRDE_AUDIO_FMT_MAKE(PDMAudioPropsHz(pProps),
                                                            PDMAudioPropsChannels(pProps),
                                                            PDMAudioPropsSampleBits(pProps),
                                                            pProps->fSigned);
    Assert(uVrdpFormat == VRDE_AUDIO_FMT_MAKE(PDMAudioPropsHz(pProps), 2, 64, true));

    /* We specified PDMAUDIOSTREAMLAYOUT_RAW (== S64), so
       convert the buffer pointe and size accordingly:  */
    PCPDMAUDIOFRAME paSampleBuf    = (PCPDMAUDIOFRAME)pvBuf;
    uint32_t const  cFramesToWrite = cbBuf / sizeof(paSampleBuf[0]);
    Assert(cFramesToWrite * sizeof(paSampleBuf[0]) == cbBuf);

    /** @todo r=bird: there was some incoherent mumbling about "using the
     *        internal counter to track if we (still) can write to the VRDP
     *        server or if need to wait anothe round (time slot)".  However it
     *        wasn't accessing any internal counter nor doing anything else
     *        sensible, so I've removed it. */

    /*
     * Call the VRDP server with the data.
     */
    uint32_t cFramesWritten = 0;
    while (cFramesWritten < cFramesToWrite)
    {
        uint32_t const cFramesChunk = cFramesToWrite - cFramesWritten; /** @todo For now write all at once. */

        /* Note: The VRDP server expects int64_t samples per channel, regardless
                 of the actual  sample bits (e.g 8 or 16 bits). */
        pDrv->pConsoleVRDPServer->SendAudioSamples(&paSampleBuf[cFramesWritten], cFramesChunk /* Frames */, uVrdpFormat);

        cFramesWritten += cFramesChunk;
    }

    Log3Func(("cFramesWritten=%RU32\n", cFramesWritten));
    if (pcbWritten)
        *pcbWritten = cFramesWritten * sizeof(PDMAUDIOFRAME);
    return VINF_SUCCESS;
}


static int vrdeDestroyStreamIn(PDRVAUDIOVRDE pDrv, PVRDESTREAM pStreamVRDE)
{
    if (pDrv->pConsoleVRDPServer)
        pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);

    if (pStreamVRDE->In.pCircBuf)
    {
        RTCircBufDestroy(pStreamVRDE->In.pCircBuf);
        pStreamVRDE->In.pCircBuf = NULL;
    }

    return VINF_SUCCESS;
}


static int vrdeDestroyStreamOut(PDRVAUDIOVRDE pDrv, PVRDESTREAM pStreamVRDE)
{
    RT_NOREF(pDrv, pStreamVRDE);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvAudioVrdeHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    RTStrPrintf2(pBackendCfg->szName, sizeof(pBackendCfg->szName), "VRDE");

    pBackendCfg->cbStreamOut    = sizeof(VRDESTREAM);
    pBackendCfg->cbStreamIn     = sizeof(VRDESTREAM);
    pBackendCfg->cMaxStreamsIn  = UINT32_MAX;
    pBackendCfg->cMaxStreamsOut = UINT32_MAX;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvAudioVrdeHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    AssertPtrReturn(pDrv, PDMAUDIOBACKENDSTS_ERROR);

    RT_NOREF(enmDir);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvAudioVrdeHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                     PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    RT_NOREF(pInterface);

    PVRDESTREAM pStreamVRDE = (PVRDESTREAM)pStream;

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = vrdeCreateStreamIn( pStreamVRDE, pCfgReq, pCfgAcq);
    else
        rc = vrdeCreateStreamOut(pStreamVRDE, pCfgReq, pCfgAcq);

    if (RT_SUCCESS(rc))
    {
        pStreamVRDE->pCfg = PDMAudioStrmCfgDup(pCfgAcq);
        if (!pStreamVRDE->pCfg)
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioVrdeHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVAUDIOVRDE pDrv        = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    PVRDESTREAM   pStreamVRDE = (PVRDESTREAM)pStream;

    if (!pStreamVRDE->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamVRDE->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = vrdeDestroyStreamIn(pDrv, pStreamVRDE);
    else
        rc = vrdeDestroyStreamOut(pDrv, pStreamVRDE);

    if (RT_SUCCESS(rc))
    {
        PDMAudioStrmCfgFree(pStreamVRDE->pCfg);
        pStreamVRDE->pCfg = NULL;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvAudioVrdeHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                      PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVAUDIOVRDE pDrv        = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    PVRDESTREAM   pStreamVRDE = (PVRDESTREAM)pStream;

    if (!pStreamVRDE->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamVRDE->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = vrdeControlStreamIn(pDrv, pStreamVRDE, enmStreamCmd);
    else
        rc = vrdeControlStreamOut(pDrv, pStreamVRDE, enmStreamCmd);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvAudioVrdeHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);

    PVRDESTREAM pStreamVRDE = (PVRDESTREAM)pStream;

    if (pStreamVRDE->pCfg->enmDir == PDMAUDIODIR_IN)
    {
        /* Return frames instead of bytes here
         * (since we specified PDMAUDIOSTREAMLAYOUT_RAW as the audio data layout). */
        return (uint32_t)PDMAUDIOSTREAMCFG_B2F(pStreamVRDE->pCfg, RTCircBufUsed(pStreamVRDE->In.pCircBuf));
    }

    return 0;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvAudioVrdeHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    RT_NOREF(pStream);

    /** @todo Find some sane value here. We probably need a VRDE API VRDE to specify this. */
    if (pDrv->cClients)
        return _16K * sizeof(PDMAUDIOFRAME);
    return 0;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvAudioVrdeHA_StreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    RT_NOREF(pStream);

    PDMAUDIOSTREAMSTS fStrmStatus = PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED;

    if (pDrv->cClients) /* If any clients are connected, flag the stream as enabled. */
       fStrmStatus |= PDMAUDIOSTREAMSTS_FLAGS_ENABLED;

    return fStrmStatus;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioVrdeQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


AudioVRDE::AudioVRDE(Console *pConsole)
    : AudioDriver(pConsole)
    , mpDrv(NULL)
{
}


AudioVRDE::~AudioVRDE(void)
{
    if (mpDrv)
    {
        mpDrv->pAudioVRDE = NULL;
        mpDrv = NULL;
    }
}


/**
 * @copydoc AudioDriver::configureDriver
 */
int AudioVRDE::configureDriver(PCFGMNODE pLunCfg)
{
    int rc = CFGMR3InsertInteger(pLunCfg, "Object", (uintptr_t)this);
    AssertRCReturn(rc, rc);
    CFGMR3InsertInteger(pLunCfg, "ObjectVRDPServer", (uintptr_t)mpConsole->i_consoleVRDPServer());
    AssertRCReturn(rc, rc);

    return AudioDriver::configureDriver(pLunCfg);
}


void AudioVRDE::onVRDEClientConnect(uint32_t uClientID)
{
    RT_NOREF(uClientID);

    LogRel2(("Audio: VRDE client connected\n"));
    if (mpDrv)
        mpDrv->cClients++;
}


void AudioVRDE::onVRDEClientDisconnect(uint32_t uClientID)
{
    RT_NOREF(uClientID);

    LogRel2(("Audio: VRDE client disconnected\n"));
    Assert(mpDrv->cClients);
    if (mpDrv)
        mpDrv->cClients--;
}


int AudioVRDE::onVRDEControl(bool fEnable, uint32_t uFlags)
{
    RT_NOREF(fEnable, uFlags);
    LogFlowThisFunc(("fEnable=%RTbool, uFlags=0x%x\n", fEnable, uFlags));

    if (mpDrv == NULL)
        return VERR_INVALID_STATE;

    return VINF_SUCCESS; /* Never veto. */
}


/**
 * Marks the beginning of sending captured audio data from a connected
 * RDP client.
 *
 * @returns VBox status code.
 * @param   pvContext               The context; in this case a pointer to a
 *                                  VRDESTREAMIN structure.
 * @param   pVRDEAudioBegin         Pointer to a VRDEAUDIOINBEGIN structure.
 */
int AudioVRDE::onVRDEInputBegin(void *pvContext, PVRDEAUDIOINBEGIN pVRDEAudioBegin)
{
    AssertPtrReturn(pvContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pVRDEAudioBegin, VERR_INVALID_POINTER);

    PVRDESTREAM pVRDEStrmIn = (PVRDESTREAM)pvContext;
    AssertPtrReturn(pVRDEStrmIn, VERR_INVALID_POINTER);

    VRDEAUDIOFORMAT audioFmt = pVRDEAudioBegin->fmt;

    int iSampleHz  = VRDE_AUDIO_FMT_SAMPLE_FREQ(audioFmt);     RT_NOREF(iSampleHz);
    int cChannels  = VRDE_AUDIO_FMT_CHANNELS(audioFmt);        RT_NOREF(cChannels);
    int cBits      = VRDE_AUDIO_FMT_BITS_PER_SAMPLE(audioFmt); RT_NOREF(cBits);
    bool fUnsigned = VRDE_AUDIO_FMT_SIGNED(audioFmt);          RT_NOREF(fUnsigned);

    LogFlowFunc(("cbSample=%RU32, iSampleHz=%d, cChannels=%d, cBits=%d, fUnsigned=%RTbool\n",
                 VRDE_AUDIO_FMT_BYTES_PER_SAMPLE(audioFmt), iSampleHz, cChannels, cBits, fUnsigned));

    return VINF_SUCCESS;
}


int AudioVRDE::onVRDEInputData(void *pvContext, const void *pvData, uint32_t cbData)
{
    PVRDESTREAM pStreamVRDE = (PVRDESTREAM)pvContext;
    AssertPtrReturn(pStreamVRDE, VERR_INVALID_POINTER);

    void  *pvBuf;
    size_t cbBuf;

    RTCircBufAcquireWriteBlock(pStreamVRDE->In.pCircBuf, cbData, &pvBuf, &cbBuf);

    if (cbBuf)
        memcpy(pvBuf, pvData, cbBuf);

    RTCircBufReleaseWriteBlock(pStreamVRDE->In.pCircBuf, cbBuf);

    if (cbBuf < cbData)
        LogRel(("VRDE: Capturing audio data lost %zu bytes\n", cbData - cbBuf)); /** @todo Use an error counter. */

    return VINF_SUCCESS; /** @todo r=andy How to tell the caller if we were not able to handle *all* input data? */
}


int AudioVRDE::onVRDEInputEnd(void *pvContext)
{
    RT_NOREF(pvContext);

    return VINF_SUCCESS;
}


int AudioVRDE::onVRDEInputIntercept(bool fEnabled)
{
    RT_NOREF(fEnabled);
    return VINF_SUCCESS; /* Never veto. */
}


/**
 * @interface_method_impl{PDMDRVREG,pfnPowerOff}
 */
/*static*/ DECLCALLBACK(void) AudioVRDE::drvPowerOff(PPDMDRVINS pDrvIns)
{
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);
    LogFlowFuncEnter();

    if (pThis->pConsoleVRDPServer)
        pThis->pConsoleVRDPServer->SendAudioInputEnd(NULL);
}


/**
 * @interface_method_impl{PDMDRVREG,pfnDestruct}
 */
/*static*/ DECLCALLBACK(void) AudioVRDE::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);
    LogFlowFuncEnter();

    /** @todo For runtime detach maybe:
    if (pThis->pConsoleVRDPServer)
        pThis->pConsoleVRDPServer->SendAudioInputEnd(NULL); */

    /*
     * If the AudioVRDE object is still alive, we must clear it's reference to
     * us since we'll be invalid when we return from this method.
     */
    if (pThis->pAudioVRDE)
    {
        pThis->pAudioVRDE->mpDrv = NULL;
        pThis->pAudioVRDE = NULL;
    }
}


/**
 * Construct a VRDE audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
/* static */
DECLCALLBACK(int) AudioVRDE::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);
    RT_NOREF(fFlags);

    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    LogRel(("Audio: Initializing VRDE driver\n"));
    LogFlowFunc(("fFlags=0x%x\n", fFlags));

    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvAudioVrdeQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnGetConfig         = drvAudioVrdeHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices        = NULL;
    pThis->IHostAudio.pfnGetStatus         = drvAudioVrdeHA_GetStatus;
    pThis->IHostAudio.pfnStreamCreate      = drvAudioVrdeHA_StreamCreate;
    pThis->IHostAudio.pfnStreamDestroy     = drvAudioVrdeHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamControl     = drvAudioVrdeHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable = drvAudioVrdeHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable = drvAudioVrdeHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending  = NULL;
    pThis->IHostAudio.pfnStreamGetStatus   = drvAudioVrdeHA_StreamGetStatus;
    pThis->IHostAudio.pfnStreamPlay        = drvAudioVrdeHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture     = drvAudioVrdeHA_StreamCapture;

    /*
     * Get the ConsoleVRDPServer object pointer.
     */
    void *pvUser;
    int rc = CFGMR3QueryPtr(pCfg, "ObjectVRDPServer", &pvUser); /** @todo r=andy Get rid of this hack and use IHostAudio::SetCallback. */
    AssertMsgRCReturn(rc, ("Confguration error: No/bad \"ObjectVRDPServer\" value, rc=%Rrc\n", rc), rc);

    /* CFGM tree saves the pointer to ConsoleVRDPServer in the Object node of AudioVRDE. */
    pThis->pConsoleVRDPServer = (ConsoleVRDPServer *)pvUser;
    pThis->cClients = 0;

    /*
     * Get the AudioVRDE object pointer.
     */
    pvUser = NULL;
    rc = CFGMR3QueryPtr(pCfg, "Object", &pvUser); /** @todo r=andy Get rid of this hack and use IHostAudio::SetCallback. */
    AssertMsgRCReturn(rc, ("Confguration error: No/bad \"Object\" value, rc=%Rrc\n", rc), rc);

    pThis->pAudioVRDE = (AudioVRDE *)pvUser;
    pThis->pAudioVRDE->mpDrv = pThis;

    /*
     * Get the interface for the above driver (DrvAudio) to make mixer/conversion calls.
     * Described in CFGM tree.
     */
    pThis->pDrvAudio = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOCONNECTOR);
    AssertMsgReturn(pThis->pDrvAudio, ("Configuration error: No upper interface specified!\n"), VERR_PDM_MISSING_INTERFACE_ABOVE);

    return VINF_SUCCESS;
}


/**
 * VRDE audio driver registration record.
 */
const PDMDRVREG AudioVRDE::DrvReg =
{
    PDM_DRVREG_VERSION,
    /* szName */
    "AudioVRDE",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio driver for VRDE backend",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVAUDIOVRDE),
    /* pfnConstruct */
    AudioVRDE::drvConstruct,
    /* pfnDestruct */
    AudioVRDE::drvDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    AudioVRDE::drvPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

