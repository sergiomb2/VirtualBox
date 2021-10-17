/* $Id$ */
/** @file
 * Testcase for the VMMR0JMPBUF operations.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <iprt/errcore.h>
#include <VBox/param.h>
#include <iprt/alloca.h>
#include <iprt/initterm.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/test.h>

#define IN_VMM_R0
#define IN_RING0 /* pretent we're in Ring-0 to get the prototypes. */
#include <VBox/vmm/vmm.h>
#include "VMMInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#if !defined(VMM_R0_SWITCH_STACK) && !defined(VMM_R0_NO_SWITCH_STACK)
# error "VMM_R0_SWITCH_STACK or VMM_R0_NO_SWITCH_STACK has to be defined."
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The jump buffer. */
static VMMR0JMPBUF          g_Jmp;
/** The number of jumps we've done. */
static unsigned volatile    g_cJmps;
/** Number of bytes allocated last time we called foo(). */
static size_t volatile      g_cbFoo;
/** Number of bytes used last time we called foo(). */
static intptr_t volatile    g_cbFooUsed;
/** Set if we're in a long jump. */
static bool                 g_fInLongJmp;


int foo(int i, int iZero, int iMinusOne)
{
    NOREF(iZero);

    /* allocate a buffer which we fill up to the end. */
    size_t cb = (i % 1555) + 32;
    g_cbFoo = cb;
    char  *pv = (char *)alloca(cb);
    RTStrPrintf(pv, cb, "i=%d%*s\n", i, cb, "");
#ifdef VMM_R0_SWITCH_STACK
    g_cbFooUsed = VMM_STACK_SIZE - ((uintptr_t)pv - (uintptr_t)g_Jmp.pvSavedStack);
    RTTESTI_CHECK_MSG_RET(g_cbFooUsed < (intptr_t)VMM_STACK_SIZE - 128, ("%#x - (%p - %p) -> %#x; cb=%#x i=%d\n", VMM_STACK_SIZE, pv, g_Jmp.pvSavedStack, g_cbFooUsed, cb, i), -15);
#elif defined(RT_ARCH_AMD64)
    g_cbFooUsed = (uintptr_t)g_Jmp.rsp - (uintptr_t)pv;
    RTTESTI_CHECK_MSG_RET(g_cbFooUsed < VMM_STACK_SIZE - 128, ("%p - %p -> %#x; cb=%#x i=%d\n", g_Jmp.rsp, pv, g_cbFooUsed, cb, i), -15);
#elif defined(RT_ARCH_X86)
    g_cbFooUsed = (uintptr_t)g_Jmp.esp - (uintptr_t)pv;
    RTTESTI_CHECK_MSG_RET(g_cbFooUsed < (intptr_t)VMM_STACK_SIZE - 128, ("%p - %p -> %#x; cb=%#x i=%d\n", g_Jmp.esp, pv, g_cbFooUsed, cb, i), -15);
#endif

    /* Twice in a row, every 7th time. */
    if ((i % 7) <= 1)
    {
        g_cJmps++;
        g_fInLongJmp = true;
        int rc = vmmR0CallRing3LongJmp(&g_Jmp, 42);
        g_fInLongJmp = false;
        if (!rc)
            return i + 10000;
        return -1;
    }
    NOREF(iMinusOne);
    return i;
}


DECLCALLBACK(int) tst2(intptr_t i, intptr_t i2)
{
    RTTESTI_CHECK_MSG_RET(i >= 0 && i <= 8192, ("i=%d is out of range [0..8192]\n", i),      1);
    RTTESTI_CHECK_MSG_RET(i2 == 0,             ("i2=%d is out of range [0]\n", i2),          1);
    int iExpect = (i % 7) <= 1 ? i + 10000 : i;
    int rc = foo(i, 0, -1);
    RTTESTI_CHECK_MSG_RET(rc == iExpect,       ("i=%d rc=%d expected=%d\n", i, rc, iExpect), 1);
    return 0;
}


DECLCALLBACK(DECL_NO_INLINE(RT_NOTHING, int)) stackRandom(PVMMR0JMPBUF pJmpBuf, PFNVMMR0SETJMP pfn, PVM pVM, PVMCPU pVCpu)
{
#ifdef RT_ARCH_AMD64
    uint32_t            cbRand  = RTRandU32Ex(1, 96);
#else
    uint32_t            cbRand  = 1;
#endif
    uint8_t volatile   *pabFuzz = (uint8_t volatile *)alloca(cbRand);
    memset((void *)pabFuzz, 0xfa, cbRand);
    int rc = vmmR0CallRing3SetJmp(pJmpBuf, pfn, pVM, pVCpu);
    memset((void *)pabFuzz, 0xaf, cbRand);
    return rc;
}


void tst(int iFrom, int iTo, int iInc)
{
#ifdef VMM_R0_SWITCH_STACK
    int const cIterations = iFrom > iTo ? iFrom - iTo : iTo - iFrom;
    void   *pvPrev = alloca(1);
#endif

    RTR0PTR R0PtrSaved = g_Jmp.pvSavedStack;
    RT_ZERO(g_Jmp);
    g_Jmp.pvSavedStack = R0PtrSaved;
    memset((void *)g_Jmp.pvSavedStack, '\0', VMM_STACK_SIZE);
    g_cbFoo = 0;
    g_cJmps = 0;
    g_cbFooUsed = 0;
    g_fInLongJmp = false;

    int iOrg = iFrom;
    for (int i = iFrom, iItr = 0; i != iTo; i += iInc, iItr++)
    {
        if (!g_fInLongJmp)
            iOrg = i;
        int rc = stackRandom(&g_Jmp, (PFNVMMR0SETJMP)(uintptr_t)tst2, (PVM)(uintptr_t)iOrg, 0);
        RTTESTI_CHECK_MSG_RETV(rc == (g_fInLongJmp ? 42 : 0),
                               ("i=%d iOrg=%d rc=%d setjmp; cbFoo=%#x cbFooUsed=%#x fInLongJmp=%d\n",
                                i, iOrg, rc, g_cbFoo, g_cbFooUsed, g_fInLongJmp));

#ifdef VMM_R0_SWITCH_STACK
        /* Make the stack pointer slide for the second half of the calls. */
        if (iItr >= cIterations / 2)
        {
            /* Note! gcc does funny rounding up of alloca(). */
# if !defined(VBOX_WITH_GCC_SANITIZER) && !defined(__MSVC_RUNTIME_CHECKS)
            void  *pv2 = alloca((i % 63) | 1);
            size_t cb2 = (uintptr_t)pvPrev - (uintptr_t)pv2;
# else
            size_t cb2 = ((i % 3) + 1) * 16; /* We get what we ask for here, and it's not at RSP/ESP due to guards. */
            void  *pv2 = alloca(cb2);
# endif
            RTTESTI_CHECK_MSG(cb2 >= 16 && cb2 <= 128, ("cb2=%zu pv2=%p pvPrev=%p iAlloca=%d\n", cb2, pv2, pvPrev, iItr));
            memset(pv2, 0xff, cb2);
            memset(pvPrev, 0xee, 1);
            pvPrev = pv2;
        }
#endif
    }
    RTTESTI_CHECK_MSG_RETV(g_cJmps, ("No jumps!"));
    if (g_Jmp.cbUsedAvg || g_Jmp.cUsedTotal)
        RTTestIPrintf(RTTESTLVL_ALWAYS, "cbUsedAvg=%#x cbUsedMax=%#x cUsedTotal=%#llx\n",
                      g_Jmp.cbUsedAvg, g_Jmp.cbUsedMax, g_Jmp.cUsedTotal);
}


#if defined(VMM_R0_SWITCH_STACK) && defined(RT_ARCH_AMD64)
/*
 * Stack switch back tests.
 */
RT_C_DECLS_BEGIN
DECLCALLBACK(int) tstWrapped1(        PVMMR0JMPBUF pJmp, uintptr_t u1, uintptr_t u2,  uintptr_t u3, uintptr_t u4, uintptr_t u5,
                                      uintptr_t u6, uintptr_t u7, uintptr_t u8, uintptr_t u9);
DECLCALLBACK(int) StkBack_tstWrapped1(PVMMR0JMPBUF pJmp, uintptr_t u1, uintptr_t u2,  uintptr_t u3, uintptr_t u4, uintptr_t u5,
                                      uintptr_t u6, uintptr_t u7, uintptr_t u8, uintptr_t u9);
DECLCALLBACK(int) tstWrappedThin(PVMMR0JMPBUF pJmp);
DECLCALLBACK(int) StkBack_tstWrappedThin(PVMMR0JMPBUF pJmp);
RT_C_DECLS_END


DECLCALLBACK(int) StkBack_tstWrapped1(PVMMR0JMPBUF pJmp, uintptr_t u1, uintptr_t u2,  uintptr_t u3, uintptr_t u4, uintptr_t u5,
                                      uintptr_t u6, uintptr_t u7, uintptr_t u8, uintptr_t u9)
{
    RTTESTI_CHECK_RET(pJmp == &g_Jmp, -1);
    RTTESTI_CHECK_RET(u1 == ~(uintptr_t)1U, -2);
    RTTESTI_CHECK_RET(u2 == ~(uintptr_t)2U, -3);
    RTTESTI_CHECK_RET(u3 == ~(uintptr_t)3U, -4);
    RTTESTI_CHECK_RET(u4 == ~(uintptr_t)4U, -5);
    RTTESTI_CHECK_RET(u5 == ~(uintptr_t)5U, -6);
    RTTESTI_CHECK_RET(u6 == ~(uintptr_t)6U, -7);
    RTTESTI_CHECK_RET(u7 == ~(uintptr_t)7U, -8);
    RTTESTI_CHECK_RET(u8 == ~(uintptr_t)8U, -9);
    RTTESTI_CHECK_RET(u9 == ~(uintptr_t)9U, -10);

    void *pv = alloca(32);
    memset(pv, 'a', 32);
    RTTESTI_CHECK_RET((uintptr_t)pv - (uintptr_t)g_Jmp.pvSavedStack > VMM_STACK_SIZE, -11);

    return 42;
}


DECLCALLBACK(int) tstSwitchBackInner(intptr_t i1, intptr_t i2)
{
    RTTESTI_CHECK_RET(i1 == -42, -20);
    RTTESTI_CHECK_RET(i2 == (intptr_t)&g_Jmp, -21);

    void *pv = alloca(32);
    memset(pv, 'b', 32);
    RTTESTI_CHECK_RET((uintptr_t)pv - (uintptr_t)g_Jmp.pvSavedStack < VMM_STACK_SIZE, -22);

    int rc = tstWrapped1(&g_Jmp,
                         ~(uintptr_t)1U,
                         ~(uintptr_t)2U,
                         ~(uintptr_t)3U,
                         ~(uintptr_t)4U,
                         ~(uintptr_t)5U,
                         ~(uintptr_t)6U,
                         ~(uintptr_t)7U,
                         ~(uintptr_t)8U,
                         ~(uintptr_t)9U);
    RTTESTI_CHECK_RET(rc == 42, -23);
    return rc;
}


DECLCALLBACK(int) StkBack_tstWrappedThin(PVMMR0JMPBUF pJmp)
{
    RTTESTI_CHECK_RET(pJmp == &g_Jmp, -31);

    void *pv = alloca(32);
    memset(pv, 'c', 32);
    RTTESTI_CHECK_RET((uintptr_t)pv - (uintptr_t)g_Jmp.pvSavedStack > VMM_STACK_SIZE, -32);

    return 42;
}

DECLCALLBACK(int) tstSwitchBackInnerThin(intptr_t i1, intptr_t i2)
{
    RT_NOREF(i1);
    return tstWrappedThin((PVMMR0JMPBUF)i2);
}


void tstSwitchBack(void)
{
    RTR0PTR R0PtrSaved = g_Jmp.pvSavedStack;
    RT_ZERO(g_Jmp);
    g_Jmp.pvSavedStack = R0PtrSaved;
    memset((void *)g_Jmp.pvSavedStack, '\0', VMM_STACK_SIZE);
    g_cbFoo = 0;
    g_cJmps = 0;
    g_cbFooUsed = 0;
    g_fInLongJmp = false;

    //for (int i = iFrom, iItr = 0; i != iTo; i += iInc, iItr++)
    {
        int rc = stackRandom(&g_Jmp, (PFNVMMR0SETJMP)(uintptr_t)tstSwitchBackInner, (PVM)(intptr_t)-42, (PVMCPU)&g_Jmp);
        RTTESTI_CHECK_MSG_RETV(rc == 42,
                               ("i=%d iOrg=%d rc=%d setjmp; cbFoo=%#x cbFooUsed=%#x fInLongJmp=%d\n",
                                0, 0 /*i, iOrg*/, rc, g_cbFoo, g_cbFooUsed, g_fInLongJmp));

        rc = stackRandom(&g_Jmp, (PFNVMMR0SETJMP)(uintptr_t)tstSwitchBackInnerThin, NULL, (PVMCPU)&g_Jmp);
        RTTESTI_CHECK_MSG_RETV(rc == 42,
                               ("i=%d iOrg=%d rc=%d setjmp; cbFoo=%#x cbFooUsed=%#x fInLongJmp=%d\n",
                                0, 0 /*i, iOrg*/, rc, g_cbFoo, g_cbFooUsed, g_fInLongJmp));

    }
    //RTTESTI_CHECK_MSG_RETV(g_cJmps, ("No jumps!"));
}

#endif


int main()
{
    /*
     * Init.
     */
    RTTEST hTest;
#ifdef VMM_R0_NO_SWITCH_STACK
    RTEXITCODE rcExit = RTTestInitAndCreate("tstVMMR0CallHost-1", &hTest);
#else
    RTEXITCODE rcExit = RTTestInitAndCreate("tstVMMR0CallHost-2", &hTest);
#endif
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    g_Jmp.pvSavedStack = (RTR0PTR)RTTestGuardedAllocTail(hTest, VMM_STACK_SIZE);

    /*
     * Run two test with about 1000 long jumps each.
     */
    RTTestSub(hTest, "Increasing stack usage");
    tst(0, 7000, 1);
    RTTestSub(hTest, "Decreasing stack usage");
    tst(7599, 0, -1);
#if defined(VMM_R0_SWITCH_STACK) && defined(RT_ARCH_AMD64)
    RTTestSub(hTest, "Switch back");
    tstSwitchBack();
#endif

    return RTTestSummaryAndDestroy(hTest);
}
