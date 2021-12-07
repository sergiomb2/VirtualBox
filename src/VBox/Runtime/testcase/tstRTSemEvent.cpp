/* $Id$ */
/** @file
 * IPRT Testcase - Multiple Release Event Semaphores.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include <iprt/semaphore.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/thread.h>
#include <iprt/time.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The test handle. */
static RTTEST  g_hTest;
/** Use to stop test loops. */
static volatile bool g_fStop = false;



/*********************************************************************************************************************************
*   Benchmark #1: Two thread pinging each other on two event sempahores.                                                         *
*********************************************************************************************************************************/
/** Pair of event semphores for the first benchmark test. */
static RTSEMEVENT   g_ahEvtBench1[2];
static uint64_t     g_cBench1Iterations;
static uint64_t     g_uTimeoutBench1;
static uint64_t     g_fWaitBench1;


static DECLCALLBACK(int) bench1Thread(RTTHREAD hThreadSelf, void *pvUser)
{
    uintptr_t const idxThread = (uintptr_t)pvUser;
    RT_NOREF(hThreadSelf);

    uint64_t cIterations = 0;
    for (;; cIterations++)
    {
        int rc = RTSemEventWaitEx(g_ahEvtBench1[idxThread], g_fWaitBench1, g_uTimeoutBench1);
        if (RT_SUCCESS(rc))
            RTTEST_CHECK_RC(g_hTest, RTSemEventSignal(g_ahEvtBench1[(idxThread + 1) & 1]), VINF_SUCCESS);
        else if (   rc == VERR_TIMEOUT
                 && g_uTimeoutBench1 == 0
                 && (g_fWaitBench1 & RTSEMWAIT_FLAGS_RELATIVE) )
        { /* likely */ }
        else
            RTTestFailed(g_hTest, "rc=%Rrc g_fWaitBench1=%#x g_uTimeoutBench1=%#RX64 (now=%#RX64)",
                         rc, g_fWaitBench1, g_uTimeoutBench1, RTTimeSystemNanoTS());
        if (g_fStop)
        {
            RTTEST_CHECK_RC(g_hTest, RTSemEventSignal(g_ahEvtBench1[(idxThread + 1) & 1]), VINF_SUCCESS);
            break;
        }
    }

    if (idxThread == 0)
        g_cBench1Iterations = cIterations;
    return VINF_SUCCESS;
}


static void bench1(const char *pszTest, uint32_t fFlags, uint64_t uTimeout)
{
    RTTestISub(pszTest);

    /*
     * Create the two threads and make the wait on one another's sempahore.
     */
    g_fStop          = false;
    g_uTimeoutBench1 = uTimeout;
    g_fWaitBench1    = fFlags;

    RTTESTI_CHECK_RC_RETV(RTSemEventCreate(&g_ahEvtBench1[0]), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventCreate(&g_ahEvtBench1[1]), VINF_SUCCESS);

    RTTHREAD hThread1;
    RTTESTI_CHECK_RC_RETV(RTThreadCreate(&hThread1, bench1Thread, (void *)0, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "bench1t1"), VINF_SUCCESS);
    RTTHREAD hThread2;
    RTTESTI_CHECK_RC_RETV(RTThreadCreate(&hThread2, bench1Thread, (void *)1, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "bench1t2"), VINF_SUCCESS);
    RTThreadSleep(256);

    /*
     * Kick off the first thread and wait for 5 seconds before stopping them
     * and seeing how many iterations they managed to perform.
     */
    uint64_t const nsStart = RTTimeNanoTS();
    RTTESTI_CHECK_RC(RTSemEventSignal(g_ahEvtBench1[0]), VINF_SUCCESS);
    RTThreadSleep(RT_MS_5SEC);

    ASMAtomicWriteBool(&g_fStop, true);
    uint64_t const cNsElapsed = RTTimeNanoTS() - nsStart;

    RTTESTI_CHECK_RC(RTSemEventSignal(g_ahEvtBench1[0]), VINF_SUCCESS); /* paranoia */
    RTTESTI_CHECK_RC(RTThreadWait(hThread1, RT_MS_5SEC, NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTSemEventSignal(g_ahEvtBench1[1]), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTThreadWait(hThread2, RT_MS_5SEC, NULL), VINF_SUCCESS);

    RTTESTI_CHECK_RC(RTSemEventDestroy(g_ahEvtBench1[0]), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTSemEventDestroy(g_ahEvtBench1[1]), VINF_SUCCESS);

    /*
     * Report the result.
     */
    RTTestValue(g_hTest, "Throughput", g_cBench1Iterations * RT_NS_1SEC / cNsElapsed, RTTESTUNIT_OCCURRENCES_PER_SEC);
    RTTestValue(g_hTest, "Roundtrip", cNsElapsed / g_cBench1Iterations, RTTESTUNIT_NS_PER_OCCURRENCE);

}


/*********************************************************************************************************************************
*   Test #1: Simple setup checking wakup order of two waiting thread.                                                            *
*********************************************************************************************************************************/

static DECLCALLBACK(int) test1Thread(RTTHREAD hThreadSelf, void *pvUser)
{
    RTSEMEVENT hSem = *(PRTSEMEVENT)pvUser;
    RTTEST_CHECK_RC(g_hTest, RTThreadUserSignal(hThreadSelf), VINF_SUCCESS);
    RTTEST_CHECK_RC(g_hTest, RTSemEventWait(hSem, RT_INDEFINITE_WAIT), VINF_SUCCESS);
    return VINF_SUCCESS;
}


static void test1(void)
{
    RTTestISub("Three threads");

    /*
     * Create the threads and let them block on the event semaphore one
     * after the other.
     */
    RTSEMEVENT hSem;
    RTTESTI_CHECK_RC_RETV(RTSemEventCreate(&hSem), VINF_SUCCESS);

    RTTHREAD hThread1;
    RTTESTI_CHECK_RC_RETV(RTThreadCreate(&hThread1, test1Thread, &hSem, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "test1t1"), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTThreadUserWait(hThread1, RT_MS_30SEC), VINF_SUCCESS);
    RTThreadSleep(256);

    RTTHREAD hThread2;
    RTTESTI_CHECK_RC_RETV(RTThreadCreate(&hThread2, test1Thread, &hSem, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "test1t2"), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTThreadUserWait(hThread2, RT_MS_30SEC), VINF_SUCCESS);
    RTThreadSleep(256);

    /* Signal once, hopefully waking up thread1: */
    RTTESTI_CHECK_RC(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTThreadWait(hThread1, 5000, NULL), VINF_SUCCESS);

    /* Signal once more, hopefully waking up thread2: */
    RTTESTI_CHECK_RC(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTThreadWait(hThread2, 5000, NULL), VINF_SUCCESS);

    RTTESTI_CHECK_RC(RTSemEventDestroy(hSem), VINF_SUCCESS);
}


/*********************************************************************************************************************************
*   Basic tests                                                                                                                  *
*********************************************************************************************************************************/


static void testBasicsWaitTimeout(RTSEMEVENT hSem, unsigned i)
{
    RTTESTI_CHECK_RC_RETV(RTSemEventWait(hSem, 0), VERR_TIMEOUT);
#if 0
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitNoResume(hSem, 0), VERR_TIMEOUT);
#else
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_RELATIVE,
                                           0), VERR_TIMEOUT);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           RTTimeSystemNanoTS() + 1000*i), VERR_TIMEOUT);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           RTTimeNanoTS() + 1000*i), VERR_TIMEOUT);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_MILLISECS | RTSEMWAIT_FLAGS_RELATIVE,
                                           0), VERR_TIMEOUT);
#endif
}


static void testBasics(void)
{
    RTTestISub("Basics");

    RTSEMEVENT hSem;
    RTTESTI_CHECK_RC_RETV(RTSemEventCreate(&hSem), VINF_SUCCESS);

    /* The semaphore is created in a non-signalled state. */
    testBasicsWaitTimeout(hSem, 0);
    testBasicsWaitTimeout(hSem, 1);
    if (RTTestIErrorCount())
        return;

    /* When signalling the semaphore, only the next waiter call shall
       success, all subsequent ones should timeout as above.  */
    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWait(hSem, 0), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 0);
    if (RTTestIErrorCount())
        return;

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWait(hSem, 2), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 2);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWait(hSem, RT_INDEFINITE_WAIT), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    if (RTTestIErrorCount())
        return;

    /* Now do all the event wait ex variations: */
    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_RELATIVE,
                                                0),
                          VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME   | RTSEMWAIT_FLAGS_INDEFINITE, 0), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_NORESUME | RTSEMWAIT_FLAGS_INDEFINITE, 0), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           RTTimeSystemNanoTS() + RT_NS_1US), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           RTTimeNanoTS() + RT_NS_1US), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 0);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           RTTimeNanoTS() + RT_NS_1HOUR), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 0);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           0), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           _1G), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           UINT64_MAX), VINF_SUCCESS);

    testBasicsWaitTimeout(hSem, 10);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_MILLISECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           RTTimeSystemMilliTS() + RT_MS_1SEC), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_MILLISECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           RTTimeMilliTS() + RT_MS_1SEC), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_MILLISECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           0), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 0);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_MILLISECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           _1M), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventWaitEx(hSem, RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_MILLISECS | RTSEMWAIT_FLAGS_ABSOLUTE,
                                           UINT64_MAX), VINF_SUCCESS);
    testBasicsWaitTimeout(hSem, 1);

    /* Destroy it. */
    RTTESTI_CHECK_RC_RETV(RTSemEventDestroy(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventDestroy(NIL_RTSEMEVENT), VINF_SUCCESS);

    /* Whether it is signalled or not used shouldn't matter.  */
    RTTESTI_CHECK_RC_RETV(RTSemEventCreate(&hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventSignal(hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventDestroy(hSem), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTSemEventCreate(&hSem), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTSemEventDestroy(hSem), VINF_SUCCESS);

    RTTestISubDone();
}


int main(int argc, char **argv)
{
    RT_NOREF_PV(argc); RT_NOREF_PV(argv);

    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTSemEvent", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    testBasics();
    if (!RTTestErrorCount(g_hTest))
    {
        test1();
    }
    if (!RTTestErrorCount(g_hTest))
    {
        bench1("Benchmark: Ping Pong, spin",       RTSEMWAIT_FLAGS_NORESUME | RTSEMWAIT_FLAGS_MILLISECS | RTSEMWAIT_FLAGS_RELATIVE,
               0);
        bench1("Benchmark: Ping Pong, indefinite", RTSEMWAIT_FLAGS_NORESUME | RTSEMWAIT_FLAGS_INDEFINITE,
               0);
        bench1("Benchmark: Ping Pong, absolute",   RTSEMWAIT_FLAGS_NORESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE,
               RTTimeSystemNanoTS() + RT_NS_1HOUR);
        bench1("Benchmark: Ping Pong, relative",   RTSEMWAIT_FLAGS_NORESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_RELATIVE,
               RT_NS_1HOUR);
        bench1("Benchmark: Ping Pong, relative, resume", RTSEMWAIT_FLAGS_RESUME | RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_RELATIVE,
               RT_NS_1HOUR);
    }

    return RTTestSummaryAndDestroy(g_hTest);
}

