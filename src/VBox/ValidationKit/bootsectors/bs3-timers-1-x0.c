/* $Id$ */
/** @file
 * BS3Kit - bs3-timers-1, C test driver code (16-bit).
 */

/*
 * Copyright (C) 2007-2022 Oracle Corporation
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
#define BS3_USE_X0_TEXT_SEG
#include <bs3kit.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>


static uint8_t bs3Timers1_Pit(uint8_t bMode, uint16_t uHz, uint32_t cNsMaxDiviation)
{
    uint32_t const  cTargetTicks = uHz * 3;
    uint16_t        uActualHz;
    uint64_t        cNsElapsed;
    int64_t         cNsDelta;
    uint64_t        cNsDeltaAbs;

    Bs3PitSetupAndEnablePeriodTimer(uHz);
    cNsElapsed = Bs3TestNow();
    ASMIntEnable();
    uActualHz  = g_cBs3PitIntervalHz;

    while (g_cBs3PitTicks < cTargetTicks)
        ASMHalt();

    Bs3PitDisable();
    ASMIntDisable();
    cNsElapsed = Bs3TestNow() - cNsElapsed;

    /* Calculate the absolute delta and fail the test if the diviation is too high... */
    cNsDelta    = cNsElapsed - RT_NS_1SEC * 3;
    cNsDeltaAbs = RT_ABS(cNsDelta);
    /*Bs3TestPrintf("cNsElapsed=%RU64 g_cBs3PitTicks=%RU32 uHz=%u -> %RU64ns\n",
                  cNsElapsed, g_cBs3PitTicks, uActualHz, cNsDeltaAbs);*/
    if (cNsDeltaAbs > cNsMaxDiviation)
        Bs3TestFailedF("delta %c%RU64 ns (%RI32 ms), max %RU32 ns", cNsDelta < 0 ? '-' : '+', cNsDeltaAbs,
                       (int32_t)((uint64_t)cNsDelta / RT_NS_1MS), cNsMaxDiviation);

    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_FAR_NM(bs3Timers1_Pit_100Hz)(uint8_t bMode)
{
    return bs3Timers1_Pit(bMode, 100, RT_NS_10MS);
}


BS3_DECL_FAR(uint8_t) BS3_CMN_FAR_NM(bs3Timers1_Pit_1000Hz)(uint8_t bMode)
{
    return bs3Timers1_Pit(bMode, 1000, RT_NS_10MS);
}


BS3_DECL_FAR(uint8_t) BS3_CMN_FAR_NM(bs3Timers1_Pit_2000Hz)(uint8_t bMode)
{
    return bs3Timers1_Pit(bMode, 2000, RT_NS_10MS*2);
}


BS3_DECL_FAR(uint8_t) BS3_CMN_FAR_NM(bs3Timers1_Pit_4000Hz)(uint8_t bMode)
{
    return bs3Timers1_Pit(bMode, 4000, RT_NS_10MS*4);
}

