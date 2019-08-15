/* $Id$ */
/** @file
 * GVM - The Global VM Data.
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
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

#ifndef VBOX_INCLUDED_vmm_gvm_h
#define VBOX_INCLUDED_vmm_gvm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifndef USING_VMM_COMMON_DEFS
# error "Compile job does not include VMM_COMMON_DEFS from src/VBox/Config.kmk - make sure you really need to include this file!"
#endif
#include <VBox/types.h>
#ifdef VBOX_BUGREF_9217
# include <VBox/vmm/vm.h>
#endif
#include <VBox/param.h>
#include <iprt/thread.h>
#include <iprt/assertcompile.h>


/** @defgroup grp_gvmcpu    GVMCPU - The Global VMCPU Data
 * @ingroup grp_vmm
 * @{
 */

#if defined(VBOX_BUGREF_9217) && defined(__cplusplus)
typedef struct GVMCPU : public VMCPU
#else
typedef struct GVMCPU
#endif
{
#if defined(VBOX_BUGREF_9217) && !defined(__cplusplus)
    VMCPU           s;
#endif

    /** VCPU id (0 - (pVM->cCpus - 1). */
    VMCPUID         idCpu;
    /** Padding. */
    uint32_t        uPadding;

    /** Handle to the EMT thread. */
    RTNATIVETHREAD  hEMT;

    /** Pointer to the global (ring-0) VM structure this CPU belongs to. */
    R0PTRTYPE(PGVM) pGVM;
#ifndef VBOX_BUGREF_9217
    /** Pointer to the corresponding cross context CPU structure. */
    PVMCPU          pVCpu;
    /** Pointer to the corresponding cross context VM structure. */
    PVM             pVM;
#else
    /** Pointer to the GVM structure, for CTX_SUFF use in VMMAll code.  */
    PGVM            pVMR0;
    /** The ring-3 address of this structure (only VMCPU part). */
    PVMCPUR3        pVCpuR3;
#endif

    /** Padding so gvmm starts on a 64 byte boundrary.
     * @note Keeping this working for 32-bit header syntax checking.  */
    uint8_t         abPadding[HC_ARCH_BITS == 32 ? 40 : 24];

    /** The GVMM per vcpu data. */
    union
    {
#ifdef VMM_INCLUDED_SRC_VMMR0_GVMMR0Internal_h
        struct GVMMPERVCPU  s;
#endif
        uint8_t             padding[64];
    } gvmm;

#ifdef VBOX_WITH_NEM_R0
    /** The NEM per vcpu data. */
    union
    {
# if defined(VMM_INCLUDED_SRC_include_NEMInternal_h) && defined(IN_RING0)
        struct NEMR0PERVCPU s;
# endif
        uint8_t             padding[64];
    } nemr0;
#endif

#ifdef VBOX_BUGREF_9217
    /** Padding the structure size to page boundrary. */
# ifdef VBOX_WITH_NEM_R0
    uint8_t                 abPadding2[4096 - 64 - 64 - 64];
# else
    uint8_t                 abPadding2[4096 - 64 - 64];
# endif
#endif
} GVMCPU;
#ifdef VBOX_BUGREF_9217
AssertCompileMemberAlignment(GVMCPU, idCpu,  4096);
AssertCompileMemberAlignment(GVMCPU, gvmm,   64);
# ifdef VBOX_WITH_NEM_R0
AssertCompileMemberAlignment(GVMCPU, nem,    64);
# endif
AssertCompileSizeAlignment(GVMCPU,           4096);
#else
AssertCompileMemberOffset(GVMCPU, gvmm,      64);
# ifdef VBOX_WITH_NEM_R0
AssertCompileMemberOffset(GVMCPU, nemr0,     64 + 64);
AssertCompileSize(        GVMCPU,            64 + 64 + 64);
# else
AssertCompileSize(        GVMCPU,            64 + 64);
# endif
#endif

/** @} */

/** @defgroup grp_gvm   GVM - The Global VM Data
 * @ingroup grp_vmm
 * @{
 */

/**
 * The Global VM Data.
 *
 * This is a ring-0 only structure where we put items we don't need to
 * share with ring-3 or GC, like for instance various RTR0MEMOBJ handles.
 *
 * Unlike VM, there are no special alignment restrictions here. The
 * paddings are checked by compile time assertions.
 */
#if defined(VBOX_BUGREF_9217) && defined(__cplusplus)
typedef struct GVM : public VM
#else
typedef struct GVM
#endif
{
#if defined(VBOX_BUGREF_9217) && !defined(__cplusplus)
    VM              s;
#endif
    /** Magic / eye-catcher (GVM_MAGIC). */
    uint32_t        u32Magic;
    /** The global VM handle for this VM. */
    uint32_t        hSelf;
#ifdef VBOX_BUGREF_9217
    /** Pointer to this structure (for validation purposes). */
    PGVM            pSelf;
#else
    /** The ring-0 mapping of the VM structure. */
    PVM             pVM;
#endif
    /** The ring-3 mapping of the VM structure. */
    PVMR3           pVMR3;
    /** The support driver session the VM is associated with. */
    PSUPDRVSESSION  pSession;
    /** Number of Virtual CPUs, i.e. how many entries there are in aCpus.
     * Same same as VM::cCpus. */
    uint32_t        cCpus;
    /** Padding so gvmm starts on a 64 byte boundrary.   */
    uint8_t         abPadding[HC_ARCH_BITS == 32 ? 12 + 28 : 28];

    /** The GVMM per vm data. */
    union
    {
#ifdef VMM_INCLUDED_SRC_VMMR0_GVMMR0Internal_h
        struct GVMMPERVM    s;
#endif
        uint8_t             padding[256];
    } gvmm;

    /** The GMM per vm data. */
    union
    {
#ifdef VMM_INCLUDED_SRC_VMMR0_GMMR0Internal_h
        struct GMMPERVM     s;
#endif
        uint8_t             padding[512];
    } gmm;

#ifdef VBOX_WITH_NEM_R0
    /** The NEM per vcpu data. */
    union
    {
# if defined(VMM_INCLUDED_SRC_include_NEMInternal_h) && defined(IN_RING0)
        struct NEMR0PERVM   s;
# endif
        uint8_t             padding[256];
    } nemr0;
#endif

    /** The RAWPCIVM per vm data. */
    union
    {
#ifdef VBOX_INCLUDED_rawpci_h
        struct RAWPCIPERVM s;
#endif
        uint8_t             padding[64];
    } rawpci;

#ifdef VBOX_BUGREF_9217
    /** Padding so aCpus starts on a page boundrary.  */
# ifdef VBOX_WITH_NEM_R0
    uint8_t         abPadding2[4096 - 64 - 256 - 512 - 256 - 64 - sizeof(PGVMCPU) * VMM_MAX_CPU_COUNT];
# else
    uint8_t         abPadding2[4096 - 64 - 256 - 512       - 64 - sizeof(PGVMCPU) * VMM_MAX_CPU_COUNT];
# endif
#endif
    /** For simplifying CPU enumeration in VMMAll code. */
    PGVMCPU         apCpusR0[VMM_MAX_CPU_COUNT];

    /** GVMCPU array for the configured number of virtual CPUs. */
    GVMCPU          aCpus[1];
} GVM;
#ifdef VBOX_BUGREF_9217
AssertCompileMemberAlignment(GVM, gvmm,     64);
AssertCompileMemberAlignment(GVM, gmm,      64);
# ifdef VBOX_WITH_NEM_R0
AssertCompileMemberAlignment(GVM, nem,      64);
# endif
AssertCompileMemberAlignment(GVM, rawpci,   64);
AssertCompileMemberAlignment(GVM, aCpus,    4096);
AssertCompileSizeAlignment(GVM,             4096);
#else
AssertCompileMemberOffset(GVM, gvmm,        64);
AssertCompileMemberOffset(GVM, gmm,         64 + 256);
# ifdef VBOX_WITH_NEM_R0
AssertCompileMemberOffset(GVM, nemr0,       64 + 256 + 512);
AssertCompileMemberOffset(GVM, rawpci,      64 + 256 + 512 + 256);
AssertCompileMemberOffset(GVM, aCpus,       64 + 256 + 512 + 256 + 64 + sizeof(PGVMCPU) * VMM_MAX_CPU_COUNT);
# else
AssertCompileMemberOffset(GVM, rawpci,      64 + 256 + 512);
AssertCompileMemberOffset(GVM, aCpus,       64 + 256 + 512 + 64 + sizeof(PGVMCPU) * VMM_MAX_CPU_COUNT);
# endif
#endif

/** The GVM::u32Magic value (Wayne Shorter). */
#define GVM_MAGIC       0x19330825

/** @} */

#endif /* !VBOX_INCLUDED_vmm_gvm_h */

