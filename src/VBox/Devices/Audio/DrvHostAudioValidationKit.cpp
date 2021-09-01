/* $Id$ */
/** @file
 * Host audio driver - ValidationKit - For dumping and injecting audio data from/to the device emulation.
 */

/*
 * Copyright (C) 2016-2021 Oracle Corporation
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
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include "VBoxDD.h"
#include "AudioHlp.h"
#include "AudioTest.h"
#include "AudioTestService.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Structure for keeping a Validation Kit input/output stream.
 */
typedef struct VALKITAUDIOSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM   Core;
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** How much bytes are available to read (only for capturing streams). */
    uint32_t                cbAvail;
} VALKITAUDIOSTREAM;
/** Pointer to a Validation Kit stream. */
typedef VALKITAUDIOSTREAM *PVALKITAUDIOSTREAM;

/**
 * Test tone-specific instance data.
 */
typedef struct VALKITTESTTONEDATA
{
    union
    {
        struct
        {
            /** How many bytes to write. */
            uint64_t           cbToWrite;
            /** How many bytes already written. */
            uint64_t           cbWritten;
        } Rec;
        struct
        {
            /** How many bytes to read. */
            uint64_t           cbToRead;
            /** How many bytes already read. */
            uint64_t           cbRead;
        } Play;
    } u;
    /** The test tone instance to use. */
    AUDIOTESTTONE              Tone;
    /** The test tone parameters to use. */
    AUDIOTESTTONEPARMS         Parms;
} VALKITTESTTONEDATA;

/**
 * Structure keeping a single Validation Kit test.
 */
typedef struct VALKITTESTDATA
{
    /** The list node. */
    RTLISTNODE             Node;
    /** Index in test sequence (0-based). */
    uint32_t               idxTest;
    /** Current test set entry to process. */
    PAUDIOTESTENTRY        pEntry;
    /** Current test object to process. */
    AUDIOTESTOBJ           Obj;
    /** Stream configuration to use for this test. */
    PDMAUDIOSTREAMCFG      StreamCfg;
    union
    {
        /** Test tone-specific data. */
        VALKITTESTTONEDATA TestTone;
    } t;
    /** Time stamp (real, in ms) when test started. */
    uint64_t               msStartedTS;
} VALKITTESTDATA;
/** Pointer to Validation Kit test data. */
typedef VALKITTESTDATA *PVALKITTESTDATA;

/**
 * Validation Kit audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTVALKITAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO       IHostAudio;
    /** Total number of bytes played since driver construction. */
    uint64_t            cbPlayedTotal;
    /** Total number of bytes recorded since driver construction. */
    uint64_t            cbRecordedTotal;
    /** Temporary path to use. */
    char                szPathTemp[RTPATH_MAX];
    /** Output path to use. */
    char                szPathOut[RTPATH_MAX];
    /** Current test set being handled.
     *  At the moment only one test set can be around at a time. */
    AUDIOTESTSET        Set;
    /** Number of total tests created. */
    uint32_t            cTestsTotal;
    /** Number of tests in \a lstTestsRec. */
    uint32_t            cTestsRec;
    /** List keeping the recording tests (FIFO). */
    RTLISTANCHOR        lstTestsRec;
    /** Pointer to current recording test being processed.
     *  NULL if no current test active. */
    PVALKITTESTDATA     pTestCurRec;
    /** Number of tests in \a lstTestsPlay. */
    uint32_t            cTestsPlay;
    /** List keeping the recording tests (FIFO). */
    RTLISTANCHOR        lstTestsPlay;
    /** Pointer to current playback test being processed.
     *  NULL if no current test active. */
    PVALKITTESTDATA     pTestCurPlay;
    /** Critical section for serializing access across threads. */
    RTCRITSECT          CritSect;
    /** Whether the test set needs to end.
     *  Needed for packing up (to archive) and termination, as capturing and playback
     *  can run in asynchronous threads. */
    bool                fTestSetEnd;
    /** Event semaphore for waiting on the current test set to end. */
    RTSEMEVENT          EventSemEnded;
    /** The Audio Test Service (ATS) instance. */
    ATSSERVER           Srv;
    /** Absolute path to the packed up test set archive.
     *  Keep it simple for now and only support one (open) archive at a time. */
    char                szTestSetArchive[RTPATH_MAX];
    /** File handle to the (opened) test set archive for reading. */
    RTFILE              hTestSetArchive;

} DRVHOSTVALKITAUDIO;
/** Pointer to a Validation Kit host audio driver instance. */
typedef DRVHOSTVALKITAUDIO *PDRVHOSTVALKITAUDIO;


/*********************************************************************************************************************************
*   Internal test handling code                                                                                                  *
*********************************************************************************************************************************/

/**
 * Unregisters a ValKit test, common code.
 *
 * @param   pTst                Test to unregister.
 *                              The pointer will be invalid afterwards.
 */
static void drvHostValKiUnregisterTest(PVALKITTESTDATA pTst)
{
    AssertPtrReturnVoid(pTst);

    RTListNodeRemove(&pTst->Node);

    AudioTestObjClose(pTst->Obj);
    pTst->Obj = NULL;

    if (pTst->pEntry) /* Set set entry assign? Mark as done. */
    {
        AssertPtrReturnVoid(pTst->pEntry);
        pTst->pEntry = NULL;
    }

    RTMemFree(pTst);
    pTst = NULL;
}

/**
 * Unregisters a ValKit recording test.
 *
 * @param   pThis               ValKit audio driver instance.
 * @param   pTst                Test to unregister.
 *                              The pointer will be invalid afterwards.
 */
static void drvHostValKiUnregisterRecTest(PDRVHOSTVALKITAUDIO pThis, PVALKITTESTDATA pTst)
{
    drvHostValKiUnregisterTest(pTst);

    Assert(pThis->cTestsRec);
    pThis->cTestsRec--;
}

/**
 * Unregisters a ValKit playback test.
 *
 * @param   pThis               ValKit audio driver instance.
 * @param   pTst                Test to unregister.
 *                              The pointer will be invalid afterwards.
 */
static void drvHostValKiUnregisterPlayTest(PDRVHOSTVALKITAUDIO pThis, PVALKITTESTDATA pTst)
{
    drvHostValKiUnregisterTest(pTst);

    Assert(pThis->cTestsPlay);
    pThis->cTestsPlay--;
}

/**
 * Performs some internal cleanup / housekeeping of all registered tests.
 *
 * @param   pThis               ValKit audio driver instance.
 */
static void drvHostValKitCleanup(PDRVHOSTVALKITAUDIO pThis)
{
    LogRel(("ValKit: Cleaning up ...\n"));

    if (   pThis->cTestsTotal
        && (   !pThis->cbPlayedTotal
            && !pThis->cbRecordedTotal)
       )
    {
        LogRel(("ValKit: Warning: Did not get any audio data to play or record altough tests were configured\n\n"));
        LogRel(("ValKit: Hints:\n"
                "ValKit:     - Audio device emulation configured and enabled for the VM?\n"
                "ValKit:     - Audio input and/or output enabled for the VM?\n"
                "ValKit:     - Is the guest able to play / record sound at all?\n"
                "ValKit:     - Is the guest's audio mixer or input / output sinks muted?\n"
                "ValKit:     - Audio stack misconfiguration / bug?\n\n"));
    }

    if (pThis->cTestsRec)
        LogRel(("ValKit: Warning: %RU32 guest recording tests still outstanding:\n", pThis->cTestsRec));

    PVALKITTESTDATA pTst, pTstNext;
    RTListForEachSafe(&pThis->lstTestsRec, pTst, pTstNext, VALKITTESTDATA, Node)
    {
        size_t const cbOutstanding = pTst->t.TestTone.u.Rec.cbToWrite - pTst->t.TestTone.u.Rec.cbWritten;
        if (cbOutstanding)
            LogRel(("ValKit: \tRecording test #%RU32 has %RU64 bytes (%RU32ms) outstanding (%RU8%% left)\n",
                    pTst->idxTest, cbOutstanding, PDMAudioPropsBytesToMilli(&pTst->t.TestTone.Parms.Props, (uint32_t)cbOutstanding),
                    100 - (pTst->t.TestTone.u.Rec.cbWritten * 100) / RT_MAX(pTst->t.TestTone.u.Rec.cbToWrite, 1)));
        drvHostValKiUnregisterRecTest(pThis, pTst);
    }

    if (pThis->cTestsPlay)
        LogRel(("ValKit: Warning: %RU32 guest playback tests still outstanding:\n", pThis->cTestsPlay));

    RTListForEachSafe(&pThis->lstTestsPlay, pTst, pTstNext, VALKITTESTDATA, Node)
    {
        size_t const cbOutstanding = pTst->t.TestTone.u.Play.cbToRead - pTst->t.TestTone.u.Play.cbRead;
        if (cbOutstanding)
            LogRel(("ValKit: \tPlayback test #%RU32 has %RU64 bytes (%RU32ms) outstanding (%RU8%% left)\n",
                    pTst->idxTest, cbOutstanding, PDMAudioPropsBytesToMilli(&pTst->t.TestTone.Parms.Props, (uint32_t)cbOutstanding),
                    100 - (pTst->t.TestTone.u.Play.cbRead * 100) / RT_MAX(pTst->t.TestTone.u.Play.cbToRead, 1)));
        drvHostValKiUnregisterPlayTest(pThis, pTst);
    }

    Assert(pThis->cTestsRec == 0);
    Assert(pThis->cTestsPlay == 0);
}


/*********************************************************************************************************************************
*   ATS callback implementations                                                                                                 *
*********************************************************************************************************************************/

/** @copydoc ATSCALLBACKS::pfnTestSetBegin */
static DECLCALLBACK(int) drvHostValKitTestSetBegin(void const *pvUser, const char *pszTag)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    LogRel(("ValKit: Beginning test set '%s'\n", pszTag));

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        rc = AudioTestSetCreate(&pThis->Set, pThis->szPathTemp, pszTag);

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_FAILURE(rc))
        LogRel(("ValKit: Beginning test set failed with %Rrc\n", rc));

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnTestSetEnd */
static DECLCALLBACK(int) drvHostValKitTestSetEnd(void const *pvUser, const char *pszTag)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    LogRel(("ValKit: Ending test set '%s'\n", pszTag));

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        const PAUDIOTESTSET pSet  = &pThis->Set;

        if (AudioTestSetIsRunning(pSet))
        {
            ASMAtomicWriteBool(&pThis->fTestSetEnd, true);

            rc = RTCritSectLeave(&pThis->CritSect);
            if (RT_SUCCESS(rc))
            {
                LogRel(("ValKit: Waiting for runnig test set '%s' to end ...\n", pszTag));
                rc = RTSemEventWait(pThis->EventSemEnded, RT_MS_30SEC);
                if (RT_FAILURE(rc))
                    LogRel(("ValKit: Waiting for runnig test set '%s' failed with %Rrc\n", pszTag, rc));

                int rc2 = RTCritSectEnter(&pThis->CritSect);
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }
        }

        LogRel(("ValKit: Test set has %RU32 tests total, %RU32 (still) running, %RU32 failures total\n",
                AudioTestSetGetTestsTotal(pSet), AudioTestSetGetTestsRunning(pSet), AudioTestSetGetTotalFailures(pSet)));

        if (RT_SUCCESS(rc))
        {
            LogRel(("ValKit: Closing test set '%s' ...\n", pszTag));

            /* Close the test set first. */
            rc = AudioTestSetClose(pSet);
            if (RT_SUCCESS(rc))
            {
                /* Before destroying the test environment, pack up the test set so
                 * that it's ready for transmission. */
                rc = AudioTestSetPack(pSet, pThis->szPathOut, pThis->szTestSetArchive, sizeof(pThis->szTestSetArchive));
                if (RT_SUCCESS(rc))
                {
                    LogRel(("ValKit: Packed up to '%s'\n", pThis->szTestSetArchive));
                }
                else
                    LogRel(("ValKit: Packing up test set failed with %Rrc\n", rc));

                /* Do some internal housekeeping. */
                drvHostValKitCleanup(pThis);

#ifndef DEBUG_andy
                int rc2 = AudioTestSetWipe(pSet);
                if (RT_SUCCESS(rc))
                    rc = rc2;
#endif
            }
            else
                LogRel(("ValKit: Closing test set failed with %Rrc\n", rc));

            int rc2 = AudioTestSetDestroy(pSet);
            if (RT_FAILURE(rc2))
            {
                LogRel(("ValKit: Destroying test set failed with %Rrc\n", rc));
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }
        }

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_FAILURE(rc))
        LogRel(("ValKit: Ending test set failed with %Rrc\n", rc));

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnTonePlay
 *
 * Creates and registers a new test tone guest recording test.
 * This backend will play (inject) input data to the guest.
 */
static DECLCALLBACK(int) drvHostValKitRegisterGuestRecTest(void const *pvUser, PAUDIOTESTTONEPARMS pToneParms)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    PVALKITTESTDATA pTestData = (PVALKITTESTDATA)RTMemAllocZ(sizeof(VALKITTESTDATA));
    AssertPtrReturn(pTestData, VERR_NO_MEMORY);

    memcpy(&pTestData->t.TestTone.Parms, pToneParms, sizeof(AUDIOTESTTONEPARMS));

    AssertReturn(pTestData->t.TestTone.Parms.msDuration, VERR_INVALID_PARAMETER);
    AssertReturn(PDMAudioPropsAreValid(&pTestData->t.TestTone.Parms.Props), VERR_INVALID_PARAMETER);

    AudioTestToneInit(&pTestData->t.TestTone.Tone, &pTestData->t.TestTone.Parms.Props, pTestData->t.TestTone.Parms.dbFreqHz);

    pTestData->t.TestTone.u.Rec.cbToWrite = PDMAudioPropsMilliToBytes(&pTestData->t.TestTone.Parms.Props,
                                                                      pTestData->t.TestTone.Parms.msDuration);
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        LogRel(("ValKit: Registering guest recording test #%RU32 (%RU32ms, %RU64 bytes)\n",
                pThis->cTestsTotal, pTestData->t.TestTone.Parms.msDuration, pTestData->t.TestTone.u.Rec.cbToWrite));

        RTListAppend(&pThis->lstTestsRec, &pTestData->Node);

        pTestData->idxTest = pThis->cTestsTotal++;

        pThis->cTestsRec++;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    return VINF_SUCCESS;
}

/** @copydoc ATSCALLBACKS::pfnToneRecord
 *
 * Creates and registers a new test tone guest playback test.
 * This backend will record the guest output data.
 */
static DECLCALLBACK(int) drvHostValKitRegisterGuestPlayTest(void const *pvUser, PAUDIOTESTTONEPARMS pToneParms)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    PVALKITTESTDATA pTestData = (PVALKITTESTDATA)RTMemAllocZ(sizeof(VALKITTESTDATA));
    AssertPtrReturn(pTestData, VERR_NO_MEMORY);

    memcpy(&pTestData->t.TestTone.Parms, pToneParms, sizeof(AUDIOTESTTONEPARMS));

    AssertReturn(pTestData->t.TestTone.Parms.msDuration, VERR_INVALID_PARAMETER);
    AssertReturn(PDMAudioPropsAreValid(&pTestData->t.TestTone.Parms.Props), VERR_INVALID_PARAMETER);

    pTestData->t.TestTone.u.Play.cbToRead = PDMAudioPropsMilliToBytes(&pTestData->t.TestTone.Parms.Props,
                                                                      pTestData->t.TestTone.Parms.msDuration);
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        LogRel(("ValKit: Registering guest playback test #%RU32 (%RU32ms, %RU64 bytes)\n",
                pThis->cTestsTotal, pTestData->t.TestTone.Parms.msDuration, pTestData->t.TestTone.u.Play.cbToRead));

        RTListAppend(&pThis->lstTestsPlay, &pTestData->Node);

        pTestData->idxTest = pThis->cTestsTotal++;

        pThis->cTestsPlay++;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    return VINF_SUCCESS;
}

/** @copydoc ATSCALLBACKS::pfnTestSetSendBegin */
static DECLCALLBACK(int) drvHostValKitTestSetSendBeginCallback(void const *pvUser, const char *pszTag)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (RTFileExists(pThis->szTestSetArchive)) /* Has the archive successfully been created yet? */
        {
            rc = RTFileOpen(&pThis->hTestSetArchive, pThis->szTestSetArchive, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
            if (RT_SUCCESS(rc))
            {
                uint64_t uSize;
                rc = RTFileQuerySize(pThis->hTestSetArchive, &uSize);
                if (RT_SUCCESS(rc))
                    LogRel(("ValKit: Sending test set '%s' (%zu bytes)\n", pThis->szTestSetArchive, uSize));
            }
        }
        else
            rc = VERR_FILE_NOT_FOUND;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_FAILURE(rc))
        LogRel(("ValKit: Beginning to send test set '%s' failed with %Rrc\n", pszTag, rc));

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnTestSetSendRead */
static DECLCALLBACK(int) drvHostValKitTestSetSendReadCallback(void const *pvUser,
                                                              const char *pszTag, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (RTFileIsValid(pThis->hTestSetArchive))
        {
            rc = RTFileRead(pThis->hTestSetArchive, pvBuf, cbBuf, pcbRead);
        }
        else
            rc = VERR_WRONG_ORDER;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_FAILURE(rc))
        LogRel(("ValKit: Reading from test set '%s' failed with %Rrc\n", pszTag, rc));

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnTestSetSendEnd */
static DECLCALLBACK(int) drvHostValKitTestSetSendEndCallback(void const *pvUser, const char *pszTag)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (RTFileIsValid(pThis->hTestSetArchive))
        {
            rc = RTFileClose(pThis->hTestSetArchive);
            if (RT_SUCCESS(rc))
                pThis->hTestSetArchive = NIL_RTFILE;
        }

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_FAILURE(rc))
        LogRel(("ValKit: Ending to send test set '%s' failed with %Rrc\n", pszTag, rc));

    return rc;
}


/*********************************************************************************************************************************
*   PDMIHOSTAUDIO interface implementation                                                                                       *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "Validation Kit");
    pBackendCfg->cbStream       = sizeof(VALKITAUDIOSTREAM);
    pBackendCfg->fFlags         = 0;
    pBackendCfg->cMaxStreamsOut = 1; /* Output (Playback). */
    pBackendCfg->cMaxStreamsIn  = 1; /* Input (Recording). */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostValKitAudioHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                           PCPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVHOSTVALKITAUDIO pThis      = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
    PVALKITAUDIOSTREAM  pStreamDbg = (PVALKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);
    RT_NOREF(pThis);

    int rc = VINF_SUCCESS;
    PDMAudioStrmCfgCopy(&pStreamDbg->Cfg, pCfgAcq);
    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                            bool fImmediate)
{
    RT_NOREF(pInterface, fImmediate);
    PVALKITAUDIOSTREAM  pStreamDbg = (PVALKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamEnable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDisable}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDisable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPause}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamPause(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamResume}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamResume(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDrain}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDrain(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pStream);

    PDRVHOSTVALKITAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        PVALKITTESTDATA pTst = pThis->pTestCurPlay;

        if (pTst)
        {
            LogRel(("ValKit: Test #%RU32: Recording audio data ended (took %RU32ms)\n",
                pTst->idxTest, RTTimeMilliTS() - pTst->msStartedTS));

            if (pTst->t.TestTone.u.Play.cbRead > pTst->t.TestTone.u.Play.cbToRead)
                LogRel(("ValKit: Warning: Test #%RU32 read %RU32 bytes more than announced\n",
                        pTst->idxTest, pTst->t.TestTone.u.Play.cbRead - pTst->t.TestTone.u.Play.cbToRead));

            AudioTestSetTestDone(pTst->pEntry);

            pThis->pTestCurPlay = NULL;
            pTst                = NULL;

            if (ASMAtomicReadBool(&pThis->fTestSetEnd))
                rc = RTSemEventSignal(pThis->EventSemEnded);
        }

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostValKitAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDRVHOSTVALKITAUDIO pThis       = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
    PVALKITAUDIOSTREAM  pStrmValKit = (PVALKITAUDIOSTREAM)pStream;
    PVALKITTESTDATA     pTst        = NULL;

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->pTestCurRec == NULL)
        {
            pThis->pTestCurRec = RTListGetFirst(&pThis->lstTestsRec, VALKITTESTDATA, Node);
            if (pThis->pTestCurRec)
                LogRel(("ValKit: Next guest recording test in queue is test #%RU32\n", pThis->pTestCurRec->idxTest));
        }

        pTst = pThis->pTestCurRec;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    if (   pTst
        && pTst->pEntry == NULL) /* Test not started yet? */
    {
        AUDIOTESTPARMS Parms;
        RT_ZERO(Parms);
        Parms.enmDir   = PDMAUDIODIR_OUT;
        Parms.enmType  = AUDIOTESTTYPE_TESTTONE_PLAY;
        Parms.TestTone = pTst->t.TestTone.Parms;

        rc = AudioTestSetTestBegin(&pThis->Set, "Injecting audio input data to guest",
                                    &Parms, &pTst->pEntry);
        if (RT_SUCCESS(rc))
            rc = AudioTestSetObjCreateAndRegister(&pThis->Set, "host-tone-play.pcm", &pTst->Obj);

        if (RT_SUCCESS(rc))
        {
            pTst->msStartedTS = RTTimeMilliTS();
            LogRel(("ValKit: Injecting audio input data (%RU16Hz, %RU32ms, %RU32 bytes) started\n",
                    (uint16_t)pTst->t.TestTone.Tone.rdFreqHz,
                    pTst->t.TestTone.Parms.msDuration, pTst->t.TestTone.u.Rec.cbToWrite));
        }

        pStrmValKit->cbAvail += pTst->t.TestTone.u.Rec.cbToWrite;
        LogRel(("ValKit: Now total of %RU32 bytes available for capturing\n", pStrmValKit->cbAvail));
    }

    LogRel(("ValKit: Test #%RU32: Reporting %RU32 bytes as available\n",
            pTst ? pTst->idxTest : 9999, pStrmValKit->cbAvail));
    return pStrmValKit->cbAvail;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostValKitAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetState}
 */
static DECLCALLBACK(PDMHOSTAUDIOSTREAMSTATE) drvHostValKitAudioHA_StreamGetState(PPDMIHOSTAUDIO pInterface,
                                                                                 PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pStream, PDMHOSTAUDIOSTREAMSTATE_INVALID);

    PDRVHOSTVALKITAUDIO     pThis    = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
    PDMHOSTAUDIOSTREAMSTATE enmState = PDMHOSTAUDIOSTREAMSTATE_NOT_WORKING;

    if (pStream->pStream->Cfg.enmDir == PDMAUDIODIR_IN)
    {
        int rc2 = RTCritSectEnter(&pThis->CritSect);
        if (RT_SUCCESS(rc2))
        {
            enmState = pThis->cTestsRec == 0
                     ? PDMHOSTAUDIOSTREAMSTATE_INACTIVE : PDMHOSTAUDIOSTREAMSTATE_OKAY;

            rc2 = RTCritSectLeave(&pThis->CritSect);
            AssertRC(rc2);
        }
    }
    else
        enmState = PDMHOSTAUDIOSTREAMSTATE_OKAY;

    return enmState;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    if (cbBuf == 0)
    {
        /* Fend off draining calls. */
        *pcbWritten = 0;
        return VINF_SUCCESS;
    }

    PDRVHOSTVALKITAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
    PVALKITTESTDATA     pTst  = NULL;

    pThis->cbPlayedTotal += cbBuf; /* Do a bit of accounting. */

    bool const fIsSilence = PDMAudioPropsIsBufferSilence(&pStream->pStream->Cfg.Props, pvBuf, cbBuf);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->pTestCurPlay == NULL)
        {
            pThis->pTestCurPlay = RTListGetFirst(&pThis->lstTestsPlay, VALKITTESTDATA, Node);
            if (pThis->pTestCurPlay)
                LogRel(("ValKit: Next guest playback test in queue is test #%RU32\n", pThis->pTestCurPlay->idxTest));
        }

        pTst = pThis->pTestCurPlay;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    if (pTst == NULL) /* Empty list? */
    {
#ifdef DEBUG_andy
        if (!fIsSilence)
#endif
            LogRel(("ValKit: Warning: Guest is playing back audio (%s, %RU32 bytes, %RU64ms) when no playback test is active\n",
                    fIsSilence ? "silence" : "audible", cbBuf, PDMAudioPropsBytesToMilli(&pStream->pStream->Cfg.Props, cbBuf)));

        *pcbWritten = cbBuf;
        return VINF_SUCCESS;
    }

#ifndef DEBUG_andy
    if (fIsSilence)
        LogRel2(("ValKit: Guest is playing back %RU32 bytes (%RU64ms) silence\n",
                 cbBuf, PDMAudioPropsBytesToMilli(&pStream->pStream->Cfg.Props, cbBuf)));
#endif

    const bool fHandleSilence = false; /** @todo Skip blocks of entire silence for now. */

    if (pTst->pEntry == NULL) /* Test not started yet? */
    {
        AUDIOTESTPARMS Parms;
        RT_ZERO(Parms);
        Parms.enmDir   = PDMAUDIODIR_IN;
        Parms.enmType  = AUDIOTESTTYPE_TESTTONE_RECORD;
        Parms.TestTone = pTst->t.TestTone.Parms;

        rc = AudioTestSetTestBegin(&pThis->Set, "Recording audio data from guest",
                                    &Parms, &pTst->pEntry);
        if (RT_SUCCESS(rc))
            rc = AudioTestSetObjCreateAndRegister(&pThis->Set, "host-tone-rec.pcm", &pTst->Obj);

        if (RT_SUCCESS(rc))
        {
            pTst->msStartedTS = RTTimeMilliTS();
            LogRel(("ValKit: Test #%RU32: Recording audio data (%RU16Hz, %RU32ms) started\n",
                    pTst->idxTest, (uint16_t)Parms.TestTone.dbFreqHz, Parms.TestTone.msDuration));
        }
    }

    uint32_t cbWritten = 0;

    if (RT_SUCCESS(rc))
    {
        if (   !fIsSilence
            || (fIsSilence && fHandleSilence))
        {
            rc = AudioTestObjWrite(pTst->Obj, pvBuf, cbBuf);
            pTst->t.TestTone.u.Play.cbRead += cbBuf;

            const bool fComplete = pTst->t.TestTone.u.Play.cbRead >= pTst->t.TestTone.u.Play.cbToRead;
            if (fComplete)
            {
                LogRel(("ValKit: Test #%RU32: Recording audio data ended (took %RU32ms)\n",
                        pTst->idxTest, RTTimeMilliTS() - pTst->msStartedTS));

                if (pTst->t.TestTone.u.Play.cbRead > pTst->t.TestTone.u.Play.cbToRead)
                    LogRel(("ValKit: Warning: Test #%RU32 read %RU32 bytes more than announced\n",
                            pTst->idxTest, pTst->t.TestTone.u.Play.cbRead - pTst->t.TestTone.u.Play.cbToRead));

                AudioTestSetTestDone(pTst->pEntry);

                rc = RTCritSectEnter(&pThis->CritSect);
                if (RT_SUCCESS(rc))
                {
                    drvHostValKiUnregisterPlayTest(pThis, pTst);

                    pThis->pTestCurPlay = NULL;
                    pTst                = NULL;

                    int rc2 = RTCritSectLeave(&pThis->CritSect);
                    if (RT_SUCCESS(rc))
                        rc = rc2;
                }
            }
        }

        /* Always report everything as being played. */
        cbWritten = cbBuf;
    }

    if (RT_FAILURE(rc))
    {
        if (   pTst
            && pTst->pEntry)
            AudioTestSetTestFailed(pTst->pEntry, rc, "Recording audio data failed");
        LogRel(("ValKit: Recording audio data failed with %Rrc\n", rc));
    }

    *pcbWritten = cbWritten;

    return VINF_SUCCESS; /** @todo Return rc here? */
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                            void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pStream);

    if (cbBuf == 0)
    {
        /* Fend off draining calls. */
        *pcbRead = 0;
        return VINF_SUCCESS;
    }

    PDRVHOSTVALKITAUDIO pThis       = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
    PVALKITAUDIOSTREAM  pStrmValKit = (PVALKITAUDIOSTREAM)pStream;
    PVALKITTESTDATA     pTst        = NULL;

    pThis->cbRecordedTotal += cbBuf; /* Do a bit of accounting. */

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->pTestCurRec == NULL)
        {
            pThis->pTestCurRec = RTListGetFirst(&pThis->lstTestsRec, VALKITTESTDATA, Node);
            if (pThis->pTestCurRec)
                LogRel(("ValKit: Next guest recording test in queue is test #%RU32\n", pThis->pTestCurRec->idxTest));
        }

        pTst = pThis->pTestCurRec;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    if (pTst == NULL) /* Empty list? */
    {
        LogRel(("ValKit: Warning: Guest is trying to record %RU32 bytes (%RU32ms) of audio data when no recording test is active (%RU32 bytes available)\n",
                cbBuf, PDMAudioPropsBytesToMilli(&pStream->pStream->Cfg.Props, cbBuf), pStrmValKit->cbAvail));

        /** @todo Not sure yet why this happens after all data has been captured sometimes,
         *        but the guest side just will record silence and the audio test verification
         *        will have to deal with (and/or report) it then. */
        PDMAudioPropsClearBuffer(&pStream->pStream->Cfg.Props, pvBuf, cbBuf,
                                 PDMAudioPropsBytesToFrames(&pStream->pStream->Cfg.Props, cbBuf));

        *pcbRead = cbBuf; /* Just report back stuff as being "recorded" (silence). */
        return VINF_SUCCESS;
    }

    uint32_t cbRead = 0;

    if (RT_SUCCESS(rc))
    {
        uint32_t cbToWrite = RT_MIN(cbBuf,
                                    pTst->t.TestTone.u.Rec.cbToWrite - pTst->t.TestTone.u.Rec.cbWritten);
        if (cbToWrite)
            rc = AudioTestToneGenerate(&pTst->t.TestTone.Tone, pvBuf, cbToWrite, &cbRead);
        if (   RT_SUCCESS(rc)
            && cbRead)
        {
            Assert(cbRead == cbToWrite);

            if (cbRead > pStrmValKit->cbAvail)
                LogRel(("ValKit: Warning: Test #%RU32: Reading more from capturing stream than availabe for (%RU32 vs. %RU32)\n",
                        pTst->idxTest, cbRead, pStrmValKit->cbAvail));

            pStrmValKit->cbAvail -= RT_MIN(pStrmValKit->cbAvail, cbRead);

            rc = AudioTestObjWrite(pTst->Obj, pvBuf, cbRead);
            if (RT_SUCCESS(rc))
            {
                pTst->t.TestTone.u.Rec.cbWritten += cbRead;
                Assert(pTst->t.TestTone.u.Rec.cbWritten <= pTst->t.TestTone.u.Rec.cbToWrite);

                LogRel(("ValKit: Test #%RU32: Read %RU32 bytes of (capturing) audio data (%RU32 bytes left)\n",
                        pTst->idxTest, cbRead, pStrmValKit->cbAvail));

                const bool fComplete = pTst->t.TestTone.u.Rec.cbWritten >= pTst->t.TestTone.u.Rec.cbToWrite;
                if (fComplete)
                {
                    LogRel(("ValKit: Test #%RU32: Recording done (took %RU32ms)\n",
                            pTst->idxTest, RTTimeMilliTS() - pTst->msStartedTS));

                    AudioTestSetTestDone(pTst->pEntry);

                    rc = RTCritSectEnter(&pThis->CritSect);
                    if (RT_SUCCESS(rc))
                    {
                        drvHostValKiUnregisterRecTest(pThis, pTst);

                        pThis->pTestCurRec = NULL;
                        pTst               = NULL;

                        int rc2 = RTCritSectLeave(&pThis->CritSect);
                        AssertRC(rc2);
                    }
                }
            }
        }
    }

    if (ASMAtomicReadBool(&pThis->fTestSetEnd))
    {
        int rc2 = RTSemEventSignal(pThis->EventSemEnded);
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
    {
        if (pTst->pEntry)
            AudioTestSetTestFailed(pTst->pEntry, rc, "Injecting audio input data failed");
        LogRel(("ValKit: Test #%RU32: Failed with %Rrc\n", pTst->idxTest, rc));
    }

    *pcbRead = cbRead;

    return VINF_SUCCESS; /** @todo Return rc here? */
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostValKitAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS         pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTVALKITAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTVALKITAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


/**
 * Constructs a VaKit audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostValKitAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(pCfg, fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVHOSTVALKITAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTVALKITAUDIO);
    LogRel(("Audio: Initializing VALKIT driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostValKitAudioQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnGetConfig                  = drvHostValKitAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices                 = NULL;
    pThis->IHostAudio.pfnGetStatus                  = drvHostValKitAudioHA_GetStatus;
    pThis->IHostAudio.pfnDoOnWorkerThread           = NULL;
    pThis->IHostAudio.pfnStreamConfigHint           = NULL;
    pThis->IHostAudio.pfnStreamCreate               = drvHostValKitAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamInitAsync            = NULL;
    pThis->IHostAudio.pfnStreamDestroy              = drvHostValKitAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamNotifyDeviceChanged  = NULL;
    pThis->IHostAudio.pfnStreamEnable               = drvHostValKitAudioHA_StreamEnable;
    pThis->IHostAudio.pfnStreamDisable              = drvHostValKitAudioHA_StreamDisable;
    pThis->IHostAudio.pfnStreamPause                = drvHostValKitAudioHA_StreamPause;
    pThis->IHostAudio.pfnStreamResume               = drvHostValKitAudioHA_StreamResume;
    pThis->IHostAudio.pfnStreamDrain                = drvHostValKitAudioHA_StreamDrain;
    pThis->IHostAudio.pfnStreamGetReadable          = drvHostValKitAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable          = drvHostValKitAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending           = NULL;
    pThis->IHostAudio.pfnStreamGetState             = drvHostValKitAudioHA_StreamGetState;
    pThis->IHostAudio.pfnStreamPlay                 = drvHostValKitAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture              = drvHostValKitAudioHA_StreamCapture;

    int rc = RTCritSectInit(&pThis->CritSect);
    AssertRCReturn(rc, rc);
    rc = RTSemEventCreate(&pThis->EventSemEnded);
    AssertRCReturn(rc, rc);

    pThis->cbPlayedTotal   = 0;
    pThis->cbRecordedTotal = 0;

    pThis->fTestSetEnd = false;

    RTListInit(&pThis->lstTestsRec);
    pThis->cTestsRec  = 0;
    RTListInit(&pThis->lstTestsPlay);
    pThis->cTestsPlay = 0;

    ATSCALLBACKS Callbacks;
    RT_ZERO(Callbacks);
    Callbacks.pfnTestSetBegin     = drvHostValKitTestSetBegin;
    Callbacks.pfnTestSetEnd       = drvHostValKitTestSetEnd;
    Callbacks.pfnTonePlay         = drvHostValKitRegisterGuestRecTest;
    Callbacks.pfnToneRecord       = drvHostValKitRegisterGuestPlayTest;
    Callbacks.pfnTestSetSendBegin = drvHostValKitTestSetSendBeginCallback;
    Callbacks.pfnTestSetSendRead  = drvHostValKitTestSetSendReadCallback;
    Callbacks.pfnTestSetSendEnd   = drvHostValKitTestSetSendEndCallback;
    Callbacks.pvUser              = pThis;

    /** @todo Make this configurable via CFGM. */
    const char *pszBindAddr = "127.0.0.1"; /* Only reachable for localhost for now. */
    uint32_t    uBindPort   = ATS_TCP_DEF_BIND_PORT_VALKIT;

    LogRel2(("ValKit: Debug logging enabled\n"));

    LogRel(("ValKit: Starting Audio Test Service (ATS) at %s:%RU32...\n",
            pszBindAddr, uBindPort));

    /* Dont' use rc here, as this will be reported back to PDM and will prevent VBox
     * from starting -- not critical but warn the user though. */
    int rc2 = AudioTestSvcInit(&pThis->Srv, &Callbacks);
    if (RT_SUCCESS(rc2))
    {
        RTGETOPTUNION Val;
        RT_ZERO(Val);

        Val.u32 = ATSCONNMODE_SERVER; /** @todo No client connection mode needed here (yet). Make this configurable via CFGM. */
        rc2 = AudioTestSvcHandleOption(&pThis->Srv, ATSTCPOPT_CONN_MODE, &Val);
        AssertRC(rc2);

        Val.psz = pszBindAddr;
        rc2 = AudioTestSvcHandleOption(&pThis->Srv, ATSTCPOPT_BIND_ADDRESS, &Val);
        AssertRC(rc2);

        Val.u16 = uBindPort;
        rc2 = AudioTestSvcHandleOption(&pThis->Srv, ATSTCPOPT_BIND_PORT, &Val);
        AssertRC(rc2);

        rc2 = AudioTestSvcStart(&pThis->Srv);
    }

    if (RT_SUCCESS(rc2))
    {
        LogRel(("ValKit: Audio Test Service (ATS) running\n"));

        /** @todo Let the following be customizable by CFGM later. */
        rc2 = AudioTestPathCreateTemp(pThis->szPathTemp, sizeof(pThis->szPathTemp), "ValKitAudio");
        if (RT_SUCCESS(rc2))
        {
            LogRel(("ValKit: Using temp dir '%s'\n", pThis->szPathTemp));
            rc2 = AudioTestPathGetTemp(pThis->szPathOut, sizeof(pThis->szPathOut));
            if (RT_SUCCESS(rc2))
                LogRel(("ValKit: Using output dir '%s'\n", pThis->szPathOut));
        }
    }

    if (RT_FAILURE(rc2))
        LogRel(("ValKit: Error starting Audio Test Service (ATS), rc=%Rrc -- tests *will* fail!\n", rc2));

    if (RT_FAILURE(rc)) /* This one *is* critical though. */
        LogRel(("ValKit: Initialization failed, rc=%Rrc\n", rc));

    return rc;
}

static DECLCALLBACK(void) drvHostValKitAudioDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVHOSTVALKITAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTVALKITAUDIO);

    LogRel(("ValKit: Shutting down Audio Test Service (ATS) ...\n"));

    int rc = AudioTestSvcStop(&pThis->Srv);
    if (RT_SUCCESS(rc))
        rc = AudioTestSvcDestroy(&pThis->Srv);

    if (RT_SUCCESS(rc))
    {
        LogRel(("ValKit: Shutdown of Audio Test Service (ATS) complete\n"));
        drvHostValKitCleanup(pThis);
    }
    else
        LogRel(("ValKit: Shutdown of Audio Test Service (ATS) failed, rc=%Rrc\n", rc));

    /* Try cleaning up a bit. */
    RTDirRemove(pThis->szPathTemp);
    RTDirRemove(pThis->szPathOut);

    RTSemEventDestroy(pThis->EventSemEnded);

    if (RTCritSectIsInitialized(&pThis->CritSect))
    {
        int rc2 = RTCritSectDelete(&pThis->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_FAILURE(rc))
        LogRel(("ValKit: Destruction failed, rc=%Rrc\n", rc));
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostValidationKitAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "ValidationKitAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "ValidationKitAudio audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTVALKITAUDIO),
    /* pfnConstruct */
    drvHostValKitAudioConstruct,
    /* pfnDestruct */
    drvHostValKitAudioDestruct,
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
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

