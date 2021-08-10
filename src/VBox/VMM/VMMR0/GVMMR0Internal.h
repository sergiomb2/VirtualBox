/* $Id$ */
/** @file
 * GVMM - The Global VM Manager, Internal header.
 */

/*
 * Copyright (C) 2007-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_VMMR0_GVMMR0Internal_h
#define VMM_INCLUDED_SRC_VMMR0_GVMMR0Internal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/mem.h>

/**
 * The GVMM per VM data.
 */
typedef struct GVMMPERVCPU
{
    /** The time the halted EMT thread expires.
     * 0 if the EMT thread is blocked here. */
    uint64_t volatile   u64HaltExpire;
    /** The event semaphore the EMT thread is blocking on. */
    RTSEMEVENTMULTI     HaltEventMulti;
    /** The ring-3 mapping of the VMCPU structure. */
    RTR0MEMOBJ          VMCpuMapObj;
    /** The APIC ID of the CPU that EMT was scheduled on the last time we checked.
     * @todo Extend to 32-bit and use most suitable APIC ID function when we
     *       start using this for something sensible... */
    uint8_t             iCpuEmt;
    uint8_t             bPadding;
    /** The EMT hash table index for this VCpu. */
    uint16_t            idxEmtHash;
} GVMMPERVCPU;
/** Pointer to the GVMM per VCPU data. */
typedef GVMMPERVCPU *PGVMMPERVCPU;


/**
 * EMT hash table entry.
 */
typedef struct GVMMEMTHASHENTRY
{
    /** The key. */
    RTNATIVETHREAD      hNativeEmt;
    /** The VCpu index. */
    VMCPUID             idVCpu;
#if HC_ARCH_BITS == 64
    uint32_t            u32Padding;
#endif
} GVMMEMTHASHENTRY;
AssertCompileSize(GVMMEMTHASHENTRY, sizeof(void *) * 2);

/** The EMT hash table size. */
#define GVMM_EMT_HASH_SIZE                  (VMM_MAX_CPU_COUNT * 4)
/** Primary EMT hash table hash function, sans range limit.
 * @note We assume the native ring-0 thread handle is a pointer to a pretty big
 *       structure of at least 1 KiB.
 *          - NT AMD64 6.0 ETHREAD: 0x450. See
 *            https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/ntos/ps/ethread/index.htm
 *            for more details.
 *          - Solaris kthread_t is at least 0x370 in Solaris 10.
 *          - Linux task_struct looks pretty big too.
 *          - As does struct thread in xnu.
 * @todo Make platform specific adjustment as needed. */
#define GVMM_EMT_HASH_CORE(a_hNativeSelf)   ( (uintptr_t)(a_hNativeSelf) >> 10 )
/** Primary EMT hash table function. */
#define GVMM_EMT_HASH_1(a_hNativeSelf)      ( GVMM_EMT_HASH_CORE(a_hNativeSelf) % GVMM_EMT_HASH_SIZE )
/** Secondary EMT hash table function, added to the primary one on collision.
 * This uses the bits above the primary hash.
 * @note It is always odd, which guarantees that we'll visit all hash table
 *       entries in case of a collision. */
#define GVMM_EMT_HASH_2(a_hNativeSelf)      ( ((GVMM_EMT_HASH_CORE(a_hNativeSelf) / GVMM_EMT_HASH_SIZE) | 1) % GVMM_EMT_HASH_SIZE )

/**
 * The GVMM per VM data.
 */
typedef struct GVMMPERVM
{
    /** The shared VM data structure allocation object (PVMR0). */
    RTR0MEMOBJ          VMMemObj;
    /** The Ring-3 mapping of the shared VM data structure (PVMR3). */
    RTR0MEMOBJ          VMMapObj;
    /** The allocation object for the VM pages. */
    RTR0MEMOBJ          VMPagesMemObj;
    /** The ring-3 mapping of the VM pages. */
    RTR0MEMOBJ          VMPagesMapObj;

    /** The scheduler statistics. */
    GVMMSTATSSCHED      StatsSched;

    /** Whether the per-VM ring-0 initialization has been performed. */
    bool                fDoneVMMR0Init;
    /** Whether the per-VM ring-0 termination is being or has been performed. */
    bool                fDoneVMMR0Term;
    bool                afPadding[6];

    /** EMT lookup hash table. */
    GVMMEMTHASHENTRY    aEmtHash[GVMM_EMT_HASH_SIZE];
} GVMMPERVM;
/** Pointer to the GVMM per VM data. */
typedef GVMMPERVM *PGVMMPERVM;


#endif /* !VMM_INCLUDED_SRC_VMMR0_GVMMR0Internal_h */

