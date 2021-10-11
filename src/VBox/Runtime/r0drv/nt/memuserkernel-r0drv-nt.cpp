/* $Id$ */
/** @file
 * IPRT - User & Kernel Memory, Ring-0 Driver, NT.
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
#include "the-nt-kernel.h"

#include <iprt/mem.h>
#include <iprt/errcore.h>

#include "internal-r0drv-nt.h"


RTR0DECL(int) RTR0MemUserCopyFrom(void *pvDst, RTR3PTR R3PtrSrc, size_t cb)
{
    __try
    {
        ProbeForRead((PVOID)R3PtrSrc, cb, 1);
        memcpy(pvDst, (void const *)R3PtrSrc, cb);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return VERR_ACCESS_DENIED;
    }
    return VINF_SUCCESS;
}


RTR0DECL(int) RTR0MemUserCopyTo(RTR3PTR R3PtrDst, void const *pvSrc, size_t cb)
{
    __try
    {
        ProbeForWrite((PVOID)R3PtrDst, cb, 1);
        memcpy((void *)R3PtrDst, pvSrc, cb);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return VERR_ACCESS_DENIED;
    }
    return VINF_SUCCESS;
}


RTR0DECL(bool) RTR0MemUserIsValidAddr(RTR3PTR R3Ptr)
{
#ifdef IPRT_TARGET_NT4
    uintptr_t const uLast = g_puRtMmHighestUserAddress ? *g_puRtMmHighestUserAddress : ~(uintptr_t)0 / 2;
#else
    uintptr_t const uLast = (uintptr_t)MM_HIGHEST_USER_ADDRESS;
#endif
    return R3Ptr <= uLast;
}


RTR0DECL(bool) RTR0MemKernelIsValidAddr(void *pv)
{
#ifdef IPRT_TARGET_NT4
    uintptr_t const uFirst = g_puRtMmSystemRangeStart ? *g_puRtMmSystemRangeStart : ~(uintptr_t)0 / 2 + 1;
#else
    uintptr_t const uFirst = (uintptr_t)MM_SYSTEM_RANGE_START;
#endif
    return (uintptr_t)pv >= uFirst;
}


RTR0DECL(bool) RTR0MemAreKrnlAndUsrDifferent(void)
{
    return true;
}


RTR0DECL(int) RTR0MemKernelCopyFrom(void *pvDst, void const *pvSrc, size_t cb)
{
    if (!RTR0MemKernelIsValidAddr((void *)pvSrc))
        return VERR_ACCESS_DENIED;

    uint8_t       *pbDst = (uint8_t *)pvDst;
    uint8_t const *pbSrc = (uint8_t const *)pvSrc;

#if 0
    /*
     * The try+except stuff does not work for kernel addresses.
     */
    __try
    {
        while (cb-- > 0)
            *pbDst++ = *pbSrc++;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return VERR_ACCESS_DENIED;
    }
#else
    /*
     * This is the best I can come up with for now: Work page-by-page using MmIsAddressValid.
     */
    while (cb > 0)
    {
        if (!MmIsAddressValid((PVOID)pbSrc))
            return VERR_ACCESS_DENIED;

        size_t cbToCopy = (uintptr_t)pbSrc & PAGE_OFFSET_MASK;
        if (cbToCopy > cb)
            cbToCopy = cb;
        cb -= cbToCopy;

        __try /* doesn't work, but can't hurt, right? */
        {
            while (cbToCopy-- > 0)
                *pbDst++ = *pbSrc++;
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            return VERR_ACCESS_DENIED;
        }
    }
#endif
    return VINF_SUCCESS;
}


RTR0DECL(int) RTR0MemKernelCopyTo(void *pvDst, void const *pvSrc, size_t cb)
{
    if (!RTR0MemKernelIsValidAddr(pvDst))
        return VERR_ACCESS_DENIED;
#if 0
    uint8_t       *pbDst = (uint8_t *)pvDst;
    uint8_t const *pbSrc = (uint8_t const *)pvSrc;
# if 0
    /*
     * The try+except stuff does not work for kernel addresses.
     */
    __try
    {
        while (cb-- > 0)
            *pbDst++ = *pbSrc++;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return VERR_ACCESS_DENIED;
    }

# else
    /*
     * This is the best I can come up with for now: Work page-by-page using MmIsAddressValid.
     * Note! MmIsAddressValid does not indicate that it's writable, so we're a bit buggered if it isn't...
     */
    while (cb > 0)
    {
        if (!MmIsAddressValid((PVOID)pbSrc))
            return VERR_ACCESS_DENIED;

        size_t cbToCopy = (uintptr_t)pbSrc & PAGE_OFFSET_MASK;
        if (cbToCopy > cb)
            cbToCopy = cb;
        cb -= cbToCopy;

        __try /* doesn't work, but can't hurt, right? */
        {
            while (cbToCopy-- > 0)
                *pbDst++ = *pbSrc++;
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            return VERR_ACCESS_DENIED;
        }
    }
# endif
    return VINF_SUCCESS;
#else
    RT_NOREF(pvDst, pvSrc, cb);
    return VERR_NOT_SUPPORTED;
#endif
}

