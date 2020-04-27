/* $Id$ */
/** @file
 * IPRT - No-CRT - Basic allocators, Windows.
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
#include "internal/iprt.h"
#include <iprt/mem.h>

#include <iprt/nt/nt-and-windows.h>


#undef RTMemTmpFree
RTDECL(void) RTMemTmpFree(void *pv)
{
    HeapFree(GetProcessHeap(), 0, pv);
}


#undef RTMemFree
RTDECL(void) RTMemFree(void *pv)
{
    HeapFree(GetProcessHeap(), 0, pv);
}


#undef RTMemTmpAllocTag
RTDECL(void *) RTMemTmpAllocTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), 0, cb);
}


#undef RTMemTmpAllocZTag
RTDECL(void *) RTMemTmpAllocZTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
}


#undef RTMemAllocTag
RTDECL(void *) RTMemAllocTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), 0, cb);
}


#undef RTMemAllocZTag
RTDECL(void *) RTMemAllocZTag(size_t cb, const char *pszTag)
{
    RT_NOREF(pszTag);
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
}


#undef RTMemReallocTag
RTDECL(void *) RTMemReallocTag(void *pvOld, size_t cbNew, const char *pszTag)
{
    RT_NOREF(pszTag);
    if (pvOld)
        return HeapReAlloc(GetProcessHeap(), 0, pvOld, cbNew);
    return HeapAlloc(GetProcessHeap(), 0, cbNew);
}


#undef RTMemReallocZTag
RTDECL(void *) RTMemReallocZTag(void *pvOld, size_t cbOld, size_t cbNew, const char *pszTag)
{
    RT_NOREF(pszTag, cbOld);
    if (pvOld)
        return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pvOld, cbNew);
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbNew);
}

