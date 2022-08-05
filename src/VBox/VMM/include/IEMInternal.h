/* $Id$ */
/** @file
 * IEM - Internal header file.
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

#ifndef VMM_INCLUDED_SRC_include_IEMInternal_h
#define VMM_INCLUDED_SRC_include_IEMInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/cpum.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/stam.h>
#include <VBox/param.h>

#include <iprt/setjmp-without-sigmask.h>


RT_C_DECLS_BEGIN


/** @defgroup grp_iem_int       Internals
 * @ingroup grp_iem
 * @internal
 * @{
 */

/** For expanding symbol in slickedit and other products tagging and
 *  crossreferencing IEM symbols. */
#ifndef IEM_STATIC
# define IEM_STATIC static
#endif

/** @def IEM_WITH_SETJMP
 * Enables alternative status code handling using setjmps.
 *
 * This adds a bit of expense via the setjmp() call since it saves all the
 * non-volatile registers.  However, it eliminates return code checks and allows
 * for more optimal return value passing (return regs instead of stack buffer).
 */
#if defined(DOXYGEN_RUNNING) || defined(RT_OS_WINDOWS) || 1
# define IEM_WITH_SETJMP
#endif

#define IEM_IMPLEMENTS_TASKSWITCH

/** @def IEM_WITH_3DNOW
 * Includes the 3DNow decoding.  */
#define IEM_WITH_3DNOW

/** @def IEM_WITH_THREE_0F_38
 * Includes the three byte opcode map for instrs starting with 0x0f 0x38. */
#define IEM_WITH_THREE_0F_38

/** @def IEM_WITH_THREE_0F_3A
 * Includes the three byte opcode map for instrs starting with 0x0f 0x38. */
#define IEM_WITH_THREE_0F_3A

/** @def IEM_WITH_VEX
 * Includes the VEX decoding. */
#define IEM_WITH_VEX

/** @def IEM_CFG_TARGET_CPU
 * The minimum target CPU for the IEM emulation (IEMTARGETCPU_XXX value).
 *
 * By default we allow this to be configured by the user via the
 * CPUM/GuestCpuName config string, but this comes at a slight cost during
 * decoding.  So, for applications of this code where there is no need to
 * be dynamic wrt target CPU, just modify this define.
 */
#if !defined(IEM_CFG_TARGET_CPU) || defined(DOXYGEN_RUNNING)
# define IEM_CFG_TARGET_CPU     IEMTARGETCPU_DYNAMIC
#endif

//#define IEM_WITH_CODE_TLB // - work in progress
//#define IEM_WITH_DATA_TLB // - work in progress


/** @def IEM_USE_UNALIGNED_DATA_ACCESS
 * Use unaligned accesses instead of elaborate byte assembly. */
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86) || defined(DOXYGEN_RUNNING)
# define IEM_USE_UNALIGNED_DATA_ACCESS
#endif

//#define IEM_LOG_MEMORY_WRITES

#if !defined(IN_TSTVMSTRUCT) && !defined(DOXYGEN_RUNNING)
/** Instruction statistics.   */
typedef struct IEMINSTRSTATS
{
# define IEM_DO_INSTR_STAT(a_Name, a_szDesc) uint32_t a_Name;
# include "IEMInstructionStatisticsTmpl.h"
# undef IEM_DO_INSTR_STAT
} IEMINSTRSTATS;
#else
struct IEMINSTRSTATS;
typedef struct IEMINSTRSTATS IEMINSTRSTATS;
#endif
/** Pointer to IEM instruction statistics. */
typedef IEMINSTRSTATS *PIEMINSTRSTATS;


/** @name IEMTARGETCPU_EFL_BEHAVIOR_XXX - IEMCPU::aidxTargetCpuEflFlavour
 * @{ */
#define IEMTARGETCPU_EFL_BEHAVIOR_NATIVE      0     /**< Native x86 EFLAGS result; Intel EFLAGS when on non-x86 hosts. */
#define IEMTARGETCPU_EFL_BEHAVIOR_INTEL       1     /**< Intel EFLAGS result. */
#define IEMTARGETCPU_EFL_BEHAVIOR_AMD         2     /**< AMD EFLAGS result */
#define IEMTARGETCPU_EFL_BEHAVIOR_RESERVED    3     /**< Reserved/dummy entry slot that's the same as 0. */
#define IEMTARGETCPU_EFL_BEHAVIOR_MASK        3     /**< For masking the index before use. */
/** Selects the right variant from a_aArray.
 * pVCpu is implicit in the caller context. */
#define IEMTARGETCPU_EFL_BEHAVIOR_SELECT(a_aArray) \
    (a_aArray[pVCpu->iem.s.aidxTargetCpuEflFlavour[1] & IEMTARGETCPU_EFL_BEHAVIOR_MASK])
/** Variation of IEMTARGETCPU_EFL_BEHAVIOR_SELECT for when no native worker can
 * be used because the host CPU does not support the operation. */
#define IEMTARGETCPU_EFL_BEHAVIOR_SELECT_NON_NATIVE(a_aArray) \
    (a_aArray[pVCpu->iem.s.aidxTargetCpuEflFlavour[0] & IEMTARGETCPU_EFL_BEHAVIOR_MASK])
/** Variation of IEMTARGETCPU_EFL_BEHAVIOR_SELECT for a two dimentional
 *  array paralleling IEMCPU::aidxTargetCpuEflFlavour and a single bit index
 *  into the two.
 * @sa IEM_SELECT_NATIVE_OR_FALLBACK */
#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
# define IEMTARGETCPU_EFL_BEHAVIOR_SELECT_EX(a_aaArray, a_fNative) \
    (a_aaArray[a_fNative][pVCpu->iem.s.aidxTargetCpuEflFlavour[a_fNative] & IEMTARGETCPU_EFL_BEHAVIOR_MASK])
#else
# define IEMTARGETCPU_EFL_BEHAVIOR_SELECT_EX(a_aaArray, a_fNative) \
    (a_aaArray[0][pVCpu->iem.s.aidxTargetCpuEflFlavour[0] & IEMTARGETCPU_EFL_BEHAVIOR_MASK])
#endif
/** @} */

/**
 * Picks @a a_pfnNative or @a a_pfnFallback according to the host CPU feature
 * indicator given by @a a_fCpumFeatureMember (CPUMFEATURES member).
 *
 * On non-x86 hosts, this will shortcut to the fallback w/o checking the
 * indicator.
 *
 * @sa IEMTARGETCPU_EFL_BEHAVIOR_SELECT_EX
 */
#if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
# define IEM_SELECT_HOST_OR_FALLBACK(a_fCpumFeatureMember, a_pfnNative, a_pfnFallback) \
    (g_CpumHostFeatures.s.a_fCpumFeatureMember ? a_pfnNative : a_pfnFallback)
#else
# define IEM_SELECT_HOST_OR_FALLBACK(a_fCpumFeatureMember, a_pfnNative, a_pfnFallback) (a_pfnFallback)
#endif


/**
 * Extended operand mode that includes a representation of 8-bit.
 *
 * This is used for packing down modes when invoking some C instruction
 * implementations.
 */
typedef enum IEMMODEX
{
    IEMMODEX_16BIT = IEMMODE_16BIT,
    IEMMODEX_32BIT = IEMMODE_32BIT,
    IEMMODEX_64BIT = IEMMODE_64BIT,
    IEMMODEX_8BIT
} IEMMODEX;
AssertCompileSize(IEMMODEX, 4);


/**
 * Branch types.
 */
typedef enum IEMBRANCH
{
    IEMBRANCH_JUMP = 1,
    IEMBRANCH_CALL,
    IEMBRANCH_TRAP,
    IEMBRANCH_SOFTWARE_INT,
    IEMBRANCH_HARDWARE_INT
} IEMBRANCH;
AssertCompileSize(IEMBRANCH, 4);


/**
 * INT instruction types.
 */
typedef enum IEMINT
{
    /** INT n instruction (opcode 0xcd imm). */
    IEMINT_INTN  = 0,
    /** Single byte INT3 instruction (opcode 0xcc). */
    IEMINT_INT3  = IEM_XCPT_FLAGS_BP_INSTR,
    /** Single byte INTO instruction (opcode 0xce). */
    IEMINT_INTO  = IEM_XCPT_FLAGS_OF_INSTR,
    /** Single byte INT1 (ICEBP) instruction (opcode 0xf1). */
    IEMINT_INT1 = IEM_XCPT_FLAGS_ICEBP_INSTR
} IEMINT;
AssertCompileSize(IEMINT, 4);


/**
 * A FPU result.
 */
typedef struct IEMFPURESULT
{
    /** The output value. */
    RTFLOAT80U      r80Result;
    /** The output status. */
    uint16_t        FSW;
} IEMFPURESULT;
AssertCompileMemberOffset(IEMFPURESULT, FSW, 10);
/** Pointer to a FPU result. */
typedef IEMFPURESULT *PIEMFPURESULT;
/** Pointer to a const FPU result. */
typedef IEMFPURESULT const *PCIEMFPURESULT;


/**
 * A FPU result consisting of two output values and FSW.
 */
typedef struct IEMFPURESULTTWO
{
    /** The first output value. */
    RTFLOAT80U      r80Result1;
    /** The output status. */
    uint16_t        FSW;
    /** The second output value. */
    RTFLOAT80U      r80Result2;
} IEMFPURESULTTWO;
AssertCompileMemberOffset(IEMFPURESULTTWO, FSW, 10);
AssertCompileMemberOffset(IEMFPURESULTTWO, r80Result2, 12);
/** Pointer to a FPU result consisting of two output values and FSW. */
typedef IEMFPURESULTTWO *PIEMFPURESULTTWO;
/** Pointer to a const FPU result consisting of two output values and FSW. */
typedef IEMFPURESULTTWO const *PCIEMFPURESULTTWO;


/**
 * IEM TLB entry.
 *
 * Lookup assembly:
 * @code{.asm}
        ; Calculate tag.
        mov     rax, [VA]
        shl     rax, 16
        shr     rax, 16 + X86_PAGE_SHIFT
        or      rax, [uTlbRevision]

        ; Do indexing.
        movzx   ecx, al
        lea     rcx, [pTlbEntries + rcx]

        ; Check tag.
        cmp     [rcx + IEMTLBENTRY.uTag], rax
        jne     .TlbMiss

        ; Check access.
        mov     rax, ACCESS_FLAGS | MAPPING_R3_NOT_VALID | 0xffffff00
        and     rax, [rcx + IEMTLBENTRY.fFlagsAndPhysRev]
        cmp     rax, [uTlbPhysRev]
        jne     .TlbMiss

        ; Calc address and we're done.
        mov     eax, X86_PAGE_OFFSET_MASK
        and     eax, [VA]
        or      rax, [rcx + IEMTLBENTRY.pMappingR3]
    %ifdef VBOX_WITH_STATISTICS
        inc     qword [cTlbHits]
    %endif
        jmp     .Done

    .TlbMiss:
        mov     r8d, ACCESS_FLAGS
        mov     rdx, [VA]
        mov     rcx, [pVCpu]
        call    iemTlbTypeMiss
    .Done:

   @endcode
 *
 */
typedef struct IEMTLBENTRY
{
    /** The TLB entry tag.
     * Bits 35 thru 0 are made up of the virtual address shifted right 12 bits, this
     * is ASSUMING a virtual address width of 48 bits.
     *
     * Bits 63 thru 36 are made up of the TLB revision (zero means invalid).
     *
     * The TLB lookup code uses the current TLB revision, which won't ever be zero,
     * enabling an extremely cheap TLB invalidation most of the time.  When the TLB
     * revision wraps around though, the tags needs to be zeroed.
     *
     * @note    Try use SHRD instruction?  After seeing
     *          https://gmplib.org/~tege/x86-timing.pdf, maybe not.
     *
     * @todo    This will need to be reorganized for 57-bit wide virtual address and
     *          PCID (currently 12 bits) and ASID (currently 6 bits) support.  We'll
     *          have to move the TLB entry versioning entirely to the
     *          fFlagsAndPhysRev member then, 57 bit wide VAs means we'll only have
     *          19 bits left (64 - 57 + 12 = 19) and they'll almost entire be
     *          consumed by PCID and ASID (12 + 6 = 18).
     */
    uint64_t                uTag;
    /** Access flags and physical TLB revision.
     *
     * - Bit  0 - page tables   - not executable (X86_PTE_PAE_NX).
     * - Bit  1 - page tables   - not writable (complemented X86_PTE_RW).
     * - Bit  2 - page tables   - not user (complemented X86_PTE_US).
     * - Bit  3 - pgm phys/virt - not directly writable.
     * - Bit  4 - pgm phys page - not directly readable.
     * - Bit  5 - page tables   - not accessed (complemented X86_PTE_A).
     * - Bit  6 - page tables   - not dirty (complemented X86_PTE_D).
     * - Bit  7 - tlb entry     - pMappingR3 member not valid.
     * - Bits 63 thru 8 are used for the physical TLB revision number.
     *
     * We're using complemented bit meanings here because it makes it easy to check
     * whether special action is required.  For instance a user mode write access
     * would do a "TEST fFlags, (X86_PTE_RW | X86_PTE_US | X86_PTE_D)" and a
     * non-zero result would mean special handling needed because either it wasn't
     * writable, or it wasn't user, or the page wasn't dirty.  A user mode read
     * access would do "TEST fFlags, X86_PTE_US"; and a kernel mode read wouldn't
     * need to check any PTE flag.
     */
    uint64_t                fFlagsAndPhysRev;
    /** The guest physical page address. */
    uint64_t                GCPhys;
    /** Pointer to the ring-3 mapping. */
    R3PTRTYPE(uint8_t *)    pbMappingR3;
#if HC_ARCH_BITS == 32
    uint32_t                u32Padding1;
#endif
} IEMTLBENTRY;
AssertCompileSize(IEMTLBENTRY, 32);
/** Pointer to an IEM TLB entry. */
typedef IEMTLBENTRY *PIEMTLBENTRY;

/** @name IEMTLBE_F_XXX - TLB entry flags (IEMTLBENTRY::fFlagsAndPhysRev)
 * @{  */
#define IEMTLBE_F_PT_NO_EXEC        RT_BIT_64(0) /**< Page tables: Not executable. */
#define IEMTLBE_F_PT_NO_WRITE       RT_BIT_64(1) /**< Page tables: Not writable. */
#define IEMTLBE_F_PT_NO_USER        RT_BIT_64(2) /**< Page tables: Not user accessible (supervisor only). */
#define IEMTLBE_F_PG_NO_WRITE       RT_BIT_64(3) /**< Phys page:   Not writable (access handler, ROM, whatever). */
#define IEMTLBE_F_PG_NO_READ        RT_BIT_64(4) /**< Phys page:   Not readable (MMIO / access handler, ROM) */
#define IEMTLBE_F_PT_NO_ACCESSED    RT_BIT_64(5) /**< Phys tables: Not accessed (need to be marked accessed). */
#define IEMTLBE_F_PT_NO_DIRTY       RT_BIT_64(6) /**< Page tables: Not dirty (needs to be made dirty on write). */
#define IEMTLBE_F_NO_MAPPINGR3      RT_BIT_64(7) /**< TLB entry:   The IEMTLBENTRY::pMappingR3 member is invalid. */
#define IEMTLBE_F_PG_UNASSIGNED     RT_BIT_64(8) /**< Phys page:   Unassigned memory (not RAM, ROM, MMIO2 or MMIO). */
#define IEMTLBE_F_PHYS_REV          UINT64_C(0xfffffffffffffe00) /**< Physical revision mask. @sa IEMTLB_PHYS_REV_INCR */
/** @} */


/**
 * An IEM TLB.
 *
 * We've got two of these, one for data and one for instructions.
 */
typedef struct IEMTLB
{
    /** The TLB entries.
     * We've choosen 256 because that way we can obtain the result directly from a
     * 8-bit register without an additional AND instruction. */
    IEMTLBENTRY         aEntries[256];
    /** The TLB revision.
     * This is actually only 28 bits wide (see IEMTLBENTRY::uTag) and is incremented
     * by adding RT_BIT_64(36) to it.  When it wraps around and becomes zero, all
     * the tags in the TLB must be zeroed and the revision set to RT_BIT_64(36).
     * (The revision zero indicates an invalid TLB entry.)
     *
     * The initial value is choosen to cause an early wraparound. */
    uint64_t            uTlbRevision;
    /** The TLB physical address revision - shadow of PGM variable.
     *
     * This is actually only 56 bits wide (see IEMTLBENTRY::fFlagsAndPhysRev) and is
     * incremented by adding RT_BIT_64(8).  When it wraps around and becomes zero,
     * a rendezvous is called and each CPU wipe the IEMTLBENTRY::pMappingR3 as well
     * as IEMTLBENTRY::fFlagsAndPhysRev bits 63 thru 8, 4, and 3.
     *
     * The initial value is choosen to cause an early wraparound. */
    uint64_t volatile   uTlbPhysRev;

    /* Statistics: */

    /** TLB hits (VBOX_WITH_STATISTICS only). */
    uint64_t            cTlbHits;
    /** TLB misses. */
    uint32_t            cTlbMisses;
    /** Slow read path.  */
    uint32_t            cTlbSlowReadPath;
#if 0
    /** TLB misses because of tag mismatch. */
    uint32_t            cTlbMissesTag;
    /** TLB misses because of virtual access violation. */
    uint32_t            cTlbMissesVirtAccess;
    /** TLB misses because of dirty bit. */
    uint32_t            cTlbMissesDirty;
    /** TLB misses because of MMIO */
    uint32_t            cTlbMissesMmio;
    /** TLB misses because of write access handlers. */
    uint32_t            cTlbMissesWriteHandler;
    /** TLB misses because no r3(/r0) mapping. */
    uint32_t            cTlbMissesMapping;
#endif
    /** Alignment padding. */
    uint32_t            au32Padding[3+5];
} IEMTLB;
AssertCompileSizeAlignment(IEMTLB, 64);
/** IEMTLB::uTlbRevision increment.  */
#define IEMTLB_REVISION_INCR    RT_BIT_64(36)
/** IEMTLB::uTlbRevision mask.  */
#define IEMTLB_REVISION_MASK    (~(RT_BIT_64(36) - 1))
/** IEMTLB::uTlbPhysRev increment.
 * @sa IEMTLBE_F_PHYS_REV */
#define IEMTLB_PHYS_REV_INCR    RT_BIT_64(9)
/**
 * Calculates the TLB tag for a virtual address.
 * @returns Tag value for indexing and comparing with IEMTLB::uTag.
 * @param   a_pTlb      The TLB.
 * @param   a_GCPtr     The virtual address.
 */
#define IEMTLB_CALC_TAG(a_pTlb, a_GCPtr)    ( IEMTLB_CALC_TAG_NO_REV(a_GCPtr) | (a_pTlb)->uTlbRevision )
/**
 * Calculates the TLB tag for a virtual address but without TLB revision.
 * @returns Tag value for indexing and comparing with IEMTLB::uTag.
 * @param   a_GCPtr     The virtual address.
 */
#define IEMTLB_CALC_TAG_NO_REV(a_GCPtr)     ( (((a_GCPtr) << 16) >> (GUEST_PAGE_SHIFT + 16)) )
/**
 * Converts a TLB tag value into a TLB index.
 * @returns Index into IEMTLB::aEntries.
 * @param   a_uTag      Value returned by IEMTLB_CALC_TAG.
 */
#define IEMTLB_TAG_TO_INDEX(a_uTag)         ( (uint8_t)(a_uTag) )
/**
 * Converts a TLB tag value into a TLB index.
 * @returns Index into IEMTLB::aEntries.
 * @param   a_pTlb      The TLB.
 * @param   a_uTag      Value returned by IEMTLB_CALC_TAG.
 */
#define IEMTLB_TAG_TO_ENTRY(a_pTlb, a_uTag) ( &(a_pTlb)->aEntries[IEMTLB_TAG_TO_INDEX(a_uTag)] )


/**
 * The per-CPU IEM state.
 */
typedef struct IEMCPU
{
    /** Info status code that needs to be propagated to the IEM caller.
     * This cannot be passed internally, as it would complicate all success
     * checks within the interpreter making the code larger and almost impossible
     * to get right.  Instead, we'll store status codes to pass on here.  Each
     * source of these codes will perform appropriate sanity checks. */
    int32_t                 rcPassUp;                                                                       /* 0x00 */

    /** The current CPU execution mode (CS). */
    IEMMODE                 enmCpuMode;                                                                     /* 0x04 */
    /** The CPL. */
    uint8_t                 uCpl;                                                                           /* 0x05 */

    /** Whether to bypass access handlers or not. */
    bool                    fBypassHandlers;                                                                /* 0x06 */
    /** Whether to disregard the lock prefix (implied or not). */
    bool                    fDisregardLock;                                                                 /* 0x07 */

    /** @name Decoder state.
     * @{ */
#ifdef IEM_WITH_CODE_TLB
    /** The offset of the next instruction byte. */
    uint32_t                offInstrNextByte;                                                               /* 0x08 */
    /** The number of bytes available at pbInstrBuf for the current instruction.
     * This takes the max opcode length into account so that doesn't need to be
     * checked separately. */
    uint32_t                cbInstrBuf;                                                                     /* 0x0c */
    /** Pointer to the page containing RIP, user specified buffer or abOpcode.
     * This can be NULL if the page isn't mappable for some reason, in which
     * case we'll do fallback stuff.
     *
     * If we're executing an instruction from a user specified buffer,
     * IEMExecOneWithPrefetchedByPC and friends, this is not necessarily a page
     * aligned pointer but pointer to the user data.
     *
     * For instructions crossing pages, this will start on the first page and be
     * advanced to the next page by the time we've decoded the instruction.  This
     * therefore precludes stuff like <tt>pbInstrBuf[offInstrNextByte + cbInstrBuf - cbCurInstr]</tt>
     */
    uint8_t const          *pbInstrBuf;                                                                     /* 0x10 */
# if ARCH_BITS == 32
    uint32_t                uInstrBufHigh; /** The high dword of the host context pbInstrBuf member. */
# endif
    /** The program counter corresponding to pbInstrBuf.
     * This is set to a non-canonical address when we need to invalidate it. */
    uint64_t                uInstrBufPc;                                                                    /* 0x18 */
    /** The number of bytes available at pbInstrBuf in total (for IEMExecLots).
     * This takes the CS segment limit into account. */
    uint16_t                cbInstrBufTotal;                                                                /* 0x20 */
    /** Offset into pbInstrBuf of the first byte of the current instruction.
     * Can be negative to efficiently handle cross page instructions. */
    int16_t                 offCurInstrStart;                                                               /* 0x22 */

    /** The prefix mask (IEM_OP_PRF_XXX). */
    uint32_t                fPrefixes;                                                                      /* 0x24 */
    /** The extra REX ModR/M register field bit (REX.R << 3). */
    uint8_t                 uRexReg;                                                                        /* 0x28 */
    /** The extra REX ModR/M r/m field, SIB base and opcode reg bit
     * (REX.B << 3). */
    uint8_t                 uRexB;                                                                          /* 0x29 */
    /** The extra REX SIB index field bit (REX.X << 3). */
    uint8_t                 uRexIndex;                                                                      /* 0x2a */

    /** The effective segment register (X86_SREG_XXX). */
    uint8_t                 iEffSeg;                                                                        /* 0x2b */

    /** The offset of the ModR/M byte relative to the start of the instruction. */
    uint8_t                 offModRm;                                                                       /* 0x2c */
#else
    /** The size of what has currently been fetched into abOpcode. */
    uint8_t                 cbOpcode;                                                                       /*       0x08 */
    /** The current offset into abOpcode. */
    uint8_t                 offOpcode;                                                                      /*       0x09 */
    /** The offset of the ModR/M byte relative to the start of the instruction. */
    uint8_t                 offModRm;                                                                       /*       0x0a */

    /** The effective segment register (X86_SREG_XXX). */
    uint8_t                 iEffSeg;                                                                        /*       0x0b */

    /** The prefix mask (IEM_OP_PRF_XXX). */
    uint32_t                fPrefixes;                                                                      /*       0x0c */
    /** The extra REX ModR/M register field bit (REX.R << 3). */
    uint8_t                 uRexReg;                                                                        /*       0x10 */
    /** The extra REX ModR/M r/m field, SIB base and opcode reg bit
     * (REX.B << 3). */
    uint8_t                 uRexB;                                                                          /*       0x11 */
    /** The extra REX SIB index field bit (REX.X << 3). */
    uint8_t                 uRexIndex;                                                                      /*       0x12 */

#endif

    /** The effective operand mode. */
    IEMMODE                 enmEffOpSize;                                                                   /* 0x2d, 0x13 */
    /** The default addressing mode. */
    IEMMODE                 enmDefAddrMode;                                                                 /* 0x2e, 0x14 */
    /** The effective addressing mode. */
    IEMMODE                 enmEffAddrMode;                                                                 /* 0x2f, 0x15 */
    /** The default operand mode. */
    IEMMODE                 enmDefOpSize;                                                                   /* 0x30, 0x16 */

    /** Prefix index (VEX.pp) for two byte and three byte tables. */
    uint8_t                 idxPrefix;                                                                      /* 0x31, 0x17 */
    /** 3rd VEX/EVEX/XOP register.
     * Please use IEM_GET_EFFECTIVE_VVVV to access.  */
    uint8_t                 uVex3rdReg;                                                                     /* 0x32, 0x18 */
    /** The VEX/EVEX/XOP length field. */
    uint8_t                 uVexLength;                                                                     /* 0x33, 0x19 */
    /** Additional EVEX stuff. */
    uint8_t                 fEvexStuff;                                                                     /* 0x34, 0x1a */

    /** Explicit alignment padding. */
    uint8_t                 abAlignment2a[1];                                                               /* 0x35, 0x1b */
    /** The FPU opcode (FOP). */
    uint16_t                uFpuOpcode;                                                                     /* 0x36, 0x1c */
#ifndef IEM_WITH_CODE_TLB
    /** Explicit alignment padding. */
    uint8_t                 abAlignment2b[2];                                                               /*       0x1e */
#endif

    /** The opcode bytes. */
    uint8_t                 abOpcode[15];                                                                   /* 0x48, 0x20 */
    /** Explicit alignment padding. */
#ifdef IEM_WITH_CODE_TLB
    uint8_t                 abAlignment2c[0x48 - 0x47];                                                     /* 0x37 */
#else
    uint8_t                 abAlignment2c[0x48 - 0x2f];                                                     /*       0x2f */
#endif
    /** @} */


    /** The flags of the current exception / interrupt. */
    uint32_t                fCurXcpt;                                                                       /* 0x48, 0x48 */
    /** The current exception / interrupt. */
    uint8_t                 uCurXcpt;
    /** Exception / interrupt recursion depth. */
    int8_t                  cXcptRecursions;

    /** The number of active guest memory mappings. */
    uint8_t                 cActiveMappings;
    /** The next unused mapping index. */
    uint8_t                 iNextMapping;
    /** Records for tracking guest memory mappings. */
    struct
    {
        /** The address of the mapped bytes. */
        void               *pv;
        /** The access flags (IEM_ACCESS_XXX).
         * IEM_ACCESS_INVALID if the entry is unused. */
        uint32_t            fAccess;
#if HC_ARCH_BITS == 64
        uint32_t            u32Alignment4; /**< Alignment padding. */
#endif
    } aMemMappings[3];

    /** Locking records for the mapped memory. */
    union
    {
        PGMPAGEMAPLOCK      Lock;
        uint64_t            au64Padding[2];
    } aMemMappingLocks[3];

    /** Bounce buffer info.
     * This runs in parallel to aMemMappings. */
    struct
    {
        /** The physical address of the first byte. */
        RTGCPHYS            GCPhysFirst;
        /** The physical address of the second page. */
        RTGCPHYS            GCPhysSecond;
        /** The number of bytes in the first page. */
        uint16_t            cbFirst;
        /** The number of bytes in the second page. */
        uint16_t            cbSecond;
        /** Whether it's unassigned memory. */
        bool                fUnassigned;
        /** Explicit alignment padding. */
        bool                afAlignment5[3];
    } aMemBbMappings[3];

    /** Bounce buffer storage.
     * This runs in parallel to aMemMappings and aMemBbMappings. */
    struct
    {
        uint8_t             ab[512];
    } aBounceBuffers[3];


    /** Pointer set jump buffer - ring-3 context. */
    R3PTRTYPE(jmp_buf *)    pJmpBufR3;
    /** Pointer set jump buffer - ring-0 context. */
    R0PTRTYPE(jmp_buf *)    pJmpBufR0;

    /** @todo Should move this near @a fCurXcpt later. */
    /** The CR2 for the current exception / interrupt. */
    uint64_t                uCurXcptCr2;
    /** The error code for the current exception / interrupt. */
    uint32_t                uCurXcptErr;

    /** @name Statistics
     * @{  */
    /** The number of instructions we've executed. */
    uint32_t                cInstructions;
    /** The number of potential exits. */
    uint32_t                cPotentialExits;
    /** The number of bytes data or stack written (mostly for IEMExecOneEx).
     * This may contain uncommitted writes.  */
    uint32_t                cbWritten;
    /** Counts the VERR_IEM_INSTR_NOT_IMPLEMENTED returns. */
    uint32_t                cRetInstrNotImplemented;
    /** Counts the VERR_IEM_ASPECT_NOT_IMPLEMENTED returns. */
    uint32_t                cRetAspectNotImplemented;
    /** Counts informational statuses returned (other than VINF_SUCCESS). */
    uint32_t                cRetInfStatuses;
    /** Counts other error statuses returned. */
    uint32_t                cRetErrStatuses;
    /** Number of times rcPassUp has been used. */
    uint32_t                cRetPassUpStatus;
    /** Number of times RZ left with instruction commit pending for ring-3. */
    uint32_t                cPendingCommit;
    /** Number of long jumps. */
    uint32_t                cLongJumps;
    /** @} */

    /** @name Target CPU information.
     * @{ */
#if IEM_CFG_TARGET_CPU == IEMTARGETCPU_DYNAMIC
    /** The target CPU. */
    uint8_t                 uTargetCpu;
#else
    uint8_t                 bTargetCpuPadding;
#endif
    /** For selecting assembly works matching the target CPU EFLAGS behaviour, see
     * IEMTARGETCPU_EFL_BEHAVIOR_XXX for values, with the 1st entry for when no
     * native host support and the 2nd for when there is.
     *
     * The two values are typically indexed by a g_CpumHostFeatures bit.
     *
     * This is for instance used for the BSF & BSR instructions where AMD and
     * Intel CPUs produce different EFLAGS. */
    uint8_t                 aidxTargetCpuEflFlavour[2];

    /** The CPU vendor. */
    CPUMCPUVENDOR           enmCpuVendor;
    /** @} */

    /** @name Host CPU information.
     * @{ */
    /** The CPU vendor. */
    CPUMCPUVENDOR           enmHostCpuVendor;
    /** @} */

    /** Counts RDMSR \#GP(0) LogRel(). */
    uint8_t                 cLogRelRdMsr;
    /** Counts WRMSR \#GP(0) LogRel(). */
    uint8_t                 cLogRelWrMsr;
    /** Alignment padding. */
    uint8_t                 abAlignment8[50];

    /** Data TLB.
     * @remarks Must be 64-byte aligned. */
    IEMTLB                  DataTlb;
    /** Instruction TLB.
     * @remarks Must be 64-byte aligned. */
    IEMTLB                  CodeTlb;

    /** Exception statistics. */
    STAMCOUNTER             aStatXcpts[32];
    /** Interrupt statistics. */
    uint32_t                aStatInts[256];

#if defined(VBOX_WITH_STATISTICS) && !defined(IN_TSTVMSTRUCT) && !defined(DOXYGEN_RUNNING)
    /** Instruction statistics for ring-0/raw-mode. */
    IEMINSTRSTATS           StatsRZ;
    /** Instruction statistics for ring-3. */
    IEMINSTRSTATS           StatsR3;
#endif
} IEMCPU;
AssertCompileMemberOffset(IEMCPU, fCurXcpt, 0x48);
AssertCompileMemberAlignment(IEMCPU, DataTlb, 64);
AssertCompileMemberAlignment(IEMCPU, CodeTlb, 64);
/** Pointer to the per-CPU IEM state. */
typedef IEMCPU *PIEMCPU;
/** Pointer to the const per-CPU IEM state. */
typedef IEMCPU const *PCIEMCPU;


/** @def IEM_GET_CTX
 * Gets the guest CPU context for the calling EMT.
 * @returns PCPUMCTX
 * @param   a_pVCpu The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_CTX(a_pVCpu)                    (&(a_pVCpu)->cpum.GstCtx)

/** @def IEM_CTX_ASSERT
 * Asserts that the @a a_fExtrnMbz is present in the CPU context.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 * @param   a_fExtrnMbz     The mask of CPUMCTX_EXTRN_XXX flags that must be zero.
 */
#define IEM_CTX_ASSERT(a_pVCpu, a_fExtrnMbz)    AssertMsg(!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnMbz)), \
                                                          ("fExtrn=%#RX64 fExtrnMbz=%#RX64\n", (a_pVCpu)->cpum.GstCtx.fExtrn, \
                                                          (a_fExtrnMbz)))

/** @def IEM_CTX_IMPORT_RET
 * Makes sure the CPU context bits given by @a a_fExtrnImport are imported.
 *
 * Will call the keep to import the bits as needed.
 *
 * Returns on import failure.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 * @param   a_fExtrnImport  The mask of CPUMCTX_EXTRN_XXX flags to import.
 */
#define IEM_CTX_IMPORT_RET(a_pVCpu, a_fExtrnImport) \
    do { \
        if (!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnImport))) \
        { /* likely */ } \
        else \
        { \
            int rcCtxImport = CPUMImportGuestStateOnDemand(a_pVCpu, a_fExtrnImport); \
            AssertRCReturn(rcCtxImport, rcCtxImport); \
        } \
    } while (0)

/** @def IEM_CTX_IMPORT_NORET
 * Makes sure the CPU context bits given by @a a_fExtrnImport are imported.
 *
 * Will call the keep to import the bits as needed.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 * @param   a_fExtrnImport  The mask of CPUMCTX_EXTRN_XXX flags to import.
 */
#define IEM_CTX_IMPORT_NORET(a_pVCpu, a_fExtrnImport) \
    do { \
        if (!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnImport))) \
        { /* likely */ } \
        else \
        { \
            int rcCtxImport = CPUMImportGuestStateOnDemand(a_pVCpu, a_fExtrnImport); \
            AssertLogRelRC(rcCtxImport); \
        } \
    } while (0)

/** @def IEM_CTX_IMPORT_JMP
 * Makes sure the CPU context bits given by @a a_fExtrnImport are imported.
 *
 * Will call the keep to import the bits as needed.
 *
 * Jumps on import failure.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 * @param   a_fExtrnImport  The mask of CPUMCTX_EXTRN_XXX flags to import.
 */
#define IEM_CTX_IMPORT_JMP(a_pVCpu, a_fExtrnImport) \
    do { \
        if (!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnImport))) \
        { /* likely */ } \
        else \
        { \
            int rcCtxImport = CPUMImportGuestStateOnDemand(a_pVCpu, a_fExtrnImport); \
            AssertRCStmt(rcCtxImport, longjmp(*pVCpu->iem.s.CTX_SUFF(pJmpBuf), rcCtxImport)); \
        } \
    } while (0)



/** @def IEM_GET_TARGET_CPU
 * Gets the current IEMTARGETCPU value.
 * @returns IEMTARGETCPU value.
 * @param   a_pVCpu The cross context virtual CPU structure of the calling thread.
 */
#if IEM_CFG_TARGET_CPU != IEMTARGETCPU_DYNAMIC
# define IEM_GET_TARGET_CPU(a_pVCpu)    (IEM_CFG_TARGET_CPU)
#else
# define IEM_GET_TARGET_CPU(a_pVCpu)    ((a_pVCpu)->iem.s.uTargetCpu)
#endif

/** @def IEM_GET_INSTR_LEN
 * Gets the instruction length. */
#ifdef IEM_WITH_CODE_TLB
# define IEM_GET_INSTR_LEN(a_pVCpu)     ((a_pVCpu)->iem.s.offInstrNextByte - (uint32_t)(int32_t)(a_pVCpu)->iem.s.offCurInstrStart)
#else
# define IEM_GET_INSTR_LEN(a_pVCpu)     ((a_pVCpu)->iem.s.offOpcode)
#endif


/**
 * Shared per-VM IEM data.
 */
typedef struct IEM
{
    /** The VMX APIC-access page handler type. */
    PGMPHYSHANDLERTYPE      hVmxApicAccessPage;
} IEM;



/** @name IEM_ACCESS_XXX - Access details.
 * @{ */
#define IEM_ACCESS_INVALID              UINT32_C(0x000000ff)
#define IEM_ACCESS_TYPE_READ            UINT32_C(0x00000001)
#define IEM_ACCESS_TYPE_WRITE           UINT32_C(0x00000002)
#define IEM_ACCESS_TYPE_EXEC            UINT32_C(0x00000004)
#define IEM_ACCESS_TYPE_MASK            UINT32_C(0x00000007)
#define IEM_ACCESS_WHAT_CODE            UINT32_C(0x00000010)
#define IEM_ACCESS_WHAT_DATA            UINT32_C(0x00000020)
#define IEM_ACCESS_WHAT_STACK           UINT32_C(0x00000030)
#define IEM_ACCESS_WHAT_SYS             UINT32_C(0x00000040)
#define IEM_ACCESS_WHAT_MASK            UINT32_C(0x00000070)
/** The writes are partial, so if initialize the bounce buffer with the
 * orignal RAM content. */
#define IEM_ACCESS_PARTIAL_WRITE        UINT32_C(0x00000100)
/** Used in aMemMappings to indicate that the entry is bounce buffered. */
#define IEM_ACCESS_BOUNCE_BUFFERED      UINT32_C(0x00000200)
/** Bounce buffer with ring-3 write pending, first page. */
#define IEM_ACCESS_PENDING_R3_WRITE_1ST UINT32_C(0x00000400)
/** Bounce buffer with ring-3 write pending, second page. */
#define IEM_ACCESS_PENDING_R3_WRITE_2ND UINT32_C(0x00000800)
/** Not locked, accessed via the TLB. */
#define IEM_ACCESS_NOT_LOCKED           UINT32_C(0x00001000)
/** Valid bit mask. */
#define IEM_ACCESS_VALID_MASK           UINT32_C(0x00001fff)
/** Shift count for the TLB flags (upper word). */
#define IEM_ACCESS_SHIFT_TLB_FLAGS      16

/** Read+write data alias. */
#define IEM_ACCESS_DATA_RW              (IEM_ACCESS_TYPE_READ  | IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_WHAT_DATA)
/** Write data alias. */
#define IEM_ACCESS_DATA_W               (IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_WHAT_DATA)
/** Read data alias. */
#define IEM_ACCESS_DATA_R               (IEM_ACCESS_TYPE_READ  | IEM_ACCESS_WHAT_DATA)
/** Instruction fetch alias. */
#define IEM_ACCESS_INSTRUCTION          (IEM_ACCESS_TYPE_EXEC  | IEM_ACCESS_WHAT_CODE)
/** Stack write alias. */
#define IEM_ACCESS_STACK_W              (IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_WHAT_STACK)
/** Stack read alias. */
#define IEM_ACCESS_STACK_R              (IEM_ACCESS_TYPE_READ  | IEM_ACCESS_WHAT_STACK)
/** Stack read+write alias. */
#define IEM_ACCESS_STACK_RW             (IEM_ACCESS_TYPE_READ  | IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_WHAT_STACK)
/** Read system table alias. */
#define IEM_ACCESS_SYS_R                (IEM_ACCESS_TYPE_READ  | IEM_ACCESS_WHAT_SYS)
/** Read+write system table alias. */
#define IEM_ACCESS_SYS_RW               (IEM_ACCESS_TYPE_READ  | IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_WHAT_SYS)
/** @} */

/** @name Prefix constants (IEMCPU::fPrefixes)
 * @{ */
#define IEM_OP_PRF_SEG_CS               RT_BIT_32(0)  /**< CS segment prefix (0x2e). */
#define IEM_OP_PRF_SEG_SS               RT_BIT_32(1)  /**< SS segment prefix (0x36). */
#define IEM_OP_PRF_SEG_DS               RT_BIT_32(2)  /**< DS segment prefix (0x3e). */
#define IEM_OP_PRF_SEG_ES               RT_BIT_32(3)  /**< ES segment prefix (0x26). */
#define IEM_OP_PRF_SEG_FS               RT_BIT_32(4)  /**< FS segment prefix (0x64). */
#define IEM_OP_PRF_SEG_GS               RT_BIT_32(5)  /**< GS segment prefix (0x65). */
#define IEM_OP_PRF_SEG_MASK             UINT32_C(0x3f)

#define IEM_OP_PRF_SIZE_OP              RT_BIT_32(8)  /**< Operand size prefix (0x66). */
#define IEM_OP_PRF_SIZE_REX_W           RT_BIT_32(9)  /**< REX.W prefix (0x48-0x4f). */
#define IEM_OP_PRF_SIZE_ADDR            RT_BIT_32(10) /**< Address size prefix (0x67). */

#define IEM_OP_PRF_LOCK                 RT_BIT_32(16) /**< Lock prefix (0xf0). */
#define IEM_OP_PRF_REPNZ                RT_BIT_32(17) /**< Repeat-not-zero prefix (0xf2). */
#define IEM_OP_PRF_REPZ                 RT_BIT_32(18) /**< Repeat-if-zero prefix (0xf3). */

#define IEM_OP_PRF_REX                  RT_BIT_32(24) /**< Any REX prefix (0x40-0x4f). */
#define IEM_OP_PRF_REX_R                RT_BIT_32(25) /**< REX.R prefix (0x44,0x45,0x46,0x47,0x4c,0x4d,0x4e,0x4f). */
#define IEM_OP_PRF_REX_B                RT_BIT_32(26) /**< REX.B prefix (0x41,0x43,0x45,0x47,0x49,0x4b,0x4d,0x4f). */
#define IEM_OP_PRF_REX_X                RT_BIT_32(27) /**< REX.X prefix (0x42,0x43,0x46,0x47,0x4a,0x4b,0x4e,0x4f). */
/** Mask with all the REX prefix flags.
 * This is generally for use when needing to undo the REX prefixes when they
 * are followed legacy prefixes and therefore does not immediately preceed
 * the first opcode byte.
 * For testing whether any REX prefix is present, use  IEM_OP_PRF_REX instead. */
#define IEM_OP_PRF_REX_MASK  (IEM_OP_PRF_REX | IEM_OP_PRF_REX_R | IEM_OP_PRF_REX_B | IEM_OP_PRF_REX_X | IEM_OP_PRF_SIZE_REX_W )

#define IEM_OP_PRF_VEX                  RT_BIT_32(28) /**< Indiciates VEX prefix. */
#define IEM_OP_PRF_EVEX                 RT_BIT_32(29) /**< Indiciates EVEX prefix. */
#define IEM_OP_PRF_XOP                  RT_BIT_32(30) /**< Indiciates XOP prefix. */
/** @} */

/** @name IEMOPFORM_XXX - Opcode forms
 * @note These are ORed together with IEMOPHINT_XXX.
 * @{ */
/** ModR/M: reg, r/m */
#define IEMOPFORM_RM            0
/** ModR/M: reg, r/m (register) */
#define IEMOPFORM_RM_REG        (IEMOPFORM_RM | IEMOPFORM_MOD3)
/** ModR/M: reg, r/m (memory)   */
#define IEMOPFORM_RM_MEM        (IEMOPFORM_RM | IEMOPFORM_NOT_MOD3)
/** ModR/M: reg, r/m */
#define IEMOPFORM_RMI           1
/** ModR/M: reg, r/m (register) */
#define IEMOPFORM_RMI_REG       (IEMOPFORM_RM | IEMOPFORM_MOD3)
/** ModR/M: reg, r/m (memory)   */
#define IEMOPFORM_RMI_MEM       (IEMOPFORM_RM | IEMOPFORM_NOT_MOD3)
/** ModR/M: r/m, reg */
#define IEMOPFORM_MR            2
/** ModR/M: r/m (register), reg */
#define IEMOPFORM_MR_REG        (IEMOPFORM_MR | IEMOPFORM_MOD3)
/** ModR/M: r/m (memory), reg */
#define IEMOPFORM_MR_MEM        (IEMOPFORM_MR | IEMOPFORM_NOT_MOD3)
/** ModR/M: r/m only */
#define IEMOPFORM_M             3
/** ModR/M: r/m only (register). */
#define IEMOPFORM_M_REG         (IEMOPFORM_M | IEMOPFORM_MOD3)
/** ModR/M: r/m only (memory). */
#define IEMOPFORM_M_MEM         (IEMOPFORM_M | IEMOPFORM_NOT_MOD3)
/** ModR/M: reg only */
#define IEMOPFORM_R             4

/** VEX+ModR/M: reg, r/m */
#define IEMOPFORM_VEX_RM        8
/** VEX+ModR/M: reg, r/m (register) */
#define IEMOPFORM_VEX_RM_REG    (IEMOPFORM_VEX_RM | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, r/m (memory)   */
#define IEMOPFORM_VEX_RM_MEM    (IEMOPFORM_VEX_RM | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: r/m, reg */
#define IEMOPFORM_VEX_MR        9
/** VEX+ModR/M: r/m (register), reg */
#define IEMOPFORM_VEX_MR_REG    (IEMOPFORM_VEX_MR | IEMOPFORM_MOD3)
/** VEX+ModR/M: r/m (memory), reg */
#define IEMOPFORM_VEX_MR_MEM    (IEMOPFORM_VEX_MR | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: r/m only */
#define IEMOPFORM_VEX_M         10
/** VEX+ModR/M: r/m only (register). */
#define IEMOPFORM_VEX_M_REG     (IEMOPFORM_VEX_M | IEMOPFORM_MOD3)
/** VEX+ModR/M: r/m only (memory). */
#define IEMOPFORM_VEX_M_MEM     (IEMOPFORM_VEX_M | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: reg only */
#define IEMOPFORM_VEX_R         11
/** VEX+ModR/M: reg, vvvv, r/m */
#define IEMOPFORM_VEX_RVM       12
/** VEX+ModR/M: reg, vvvv, r/m (register). */
#define IEMOPFORM_VEX_RVM_REG   (IEMOPFORM_VEX_RVM | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, vvvv, r/m (memory). */
#define IEMOPFORM_VEX_RVM_MEM   (IEMOPFORM_VEX_RVM | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: reg, r/m, vvvv */
#define IEMOPFORM_VEX_RMV       13
/** VEX+ModR/M: reg, r/m, vvvv (register). */
#define IEMOPFORM_VEX_RMV_REG   (IEMOPFORM_VEX_RMV | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, r/m, vvvv (memory). */
#define IEMOPFORM_VEX_RMV_MEM   (IEMOPFORM_VEX_RMV | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: reg, r/m, imm8 */
#define IEMOPFORM_VEX_RMI       14
/** VEX+ModR/M: reg, r/m, imm8 (register). */
#define IEMOPFORM_VEX_RMI_REG   (IEMOPFORM_VEX_RMI | IEMOPFORM_MOD3)
/** VEX+ModR/M: reg, r/m, imm8 (memory). */
#define IEMOPFORM_VEX_RMI_MEM   (IEMOPFORM_VEX_RMI | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M: r/m, vvvv, reg */
#define IEMOPFORM_VEX_MVR       15
/** VEX+ModR/M: r/m, vvvv, reg (register) */
#define IEMOPFORM_VEX_MVR_REG   (IEMOPFORM_VEX_MVR | IEMOPFORM_MOD3)
/** VEX+ModR/M: r/m, vvvv, reg (memory) */
#define IEMOPFORM_VEX_MVR_MEM   (IEMOPFORM_VEX_MVR | IEMOPFORM_NOT_MOD3)
/** VEX+ModR/M+/n: vvvv, r/m */
#define IEMOPFORM_VEX_VM        16
/** VEX+ModR/M+/n: vvvv, r/m (register) */
#define IEMOPFORM_VEX_VM_REG    (IEMOPFORM_VEX_VM | IEMOPFORM_MOD3)
/** VEX+ModR/M+/n: vvvv, r/m (memory) */
#define IEMOPFORM_VEX_VM_MEM    (IEMOPFORM_VEX_VM | IEMOPFORM_NOT_MOD3)

/** Fixed register instruction, no R/M. */
#define IEMOPFORM_FIXED         32

/** The r/m is a register. */
#define IEMOPFORM_MOD3          RT_BIT_32(8)
/** The r/m is a memory access. */
#define IEMOPFORM_NOT_MOD3      RT_BIT_32(9)
/** @} */

/** @name IEMOPHINT_XXX - Additional Opcode Hints
 * @note These are ORed together with IEMOPFORM_XXX.
 * @{ */
/** Ignores the operand size prefix (66h). */
#define IEMOPHINT_IGNORES_OZ_PFX    RT_BIT_32(10)
/** Ignores REX.W (aka WIG). */
#define IEMOPHINT_IGNORES_REXW      RT_BIT_32(11)
/** Both the operand size prefixes (66h + REX.W) are ignored. */
#define IEMOPHINT_IGNORES_OP_SIZES  (IEMOPHINT_IGNORES_OZ_PFX | IEMOPHINT_IGNORES_REXW)
/** Allowed with the lock prefix. */
#define IEMOPHINT_LOCK_ALLOWED      RT_BIT_32(11)
/** The VEX.L value is ignored (aka LIG). */
#define IEMOPHINT_VEX_L_IGNORED     RT_BIT_32(12)
/** The VEX.L value must be zero (i.e. 128-bit width only). */
#define IEMOPHINT_VEX_L_ZERO        RT_BIT_32(13)
/** The VEX.V value must be zero. */
#define IEMOPHINT_VEX_V_ZERO        RT_BIT_32(14)

/** Hint to IEMAllInstructionPython.py that this macro should be skipped.  */
#define IEMOPHINT_SKIP_PYTHON       RT_BIT_32(31)
/** @} */

/**
 * Possible hardware task switch sources.
 */
typedef enum IEMTASKSWITCH
{
    /** Task switch caused by an interrupt/exception. */
    IEMTASKSWITCH_INT_XCPT = 1,
    /** Task switch caused by a far CALL. */
    IEMTASKSWITCH_CALL,
    /** Task switch caused by a far JMP. */
    IEMTASKSWITCH_JUMP,
    /** Task switch caused by an IRET. */
    IEMTASKSWITCH_IRET
} IEMTASKSWITCH;
AssertCompileSize(IEMTASKSWITCH, 4);

/**
 * Possible CrX load (write) sources.
 */
typedef enum IEMACCESSCRX
{
    /** CrX access caused by 'mov crX' instruction. */
    IEMACCESSCRX_MOV_CRX,
    /** CrX (CR0) write caused by 'lmsw' instruction. */
    IEMACCESSCRX_LMSW,
    /** CrX (CR0) write caused by 'clts' instruction. */
    IEMACCESSCRX_CLTS,
    /** CrX (CR0) read caused by 'smsw' instruction. */
    IEMACCESSCRX_SMSW
} IEMACCESSCRX;

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
/** @name IEM_SLAT_FAIL_XXX - Second-level address translation failure information.
 *
 * These flags provide further context to SLAT page-walk failures that could not be
 * determined by PGM (e.g, PGM is not privy to memory access permissions).
 *
 * @{
 */
/** Translating a nested-guest linear address failed accessing a nested-guest
 *  physical address. */
# define IEM_SLAT_FAIL_LINEAR_TO_PHYS_ADDR          RT_BIT_32(0)
/** Translating a nested-guest linear address failed accessing a
 *  paging-structure entry or updating accessed/dirty bits. */
# define IEM_SLAT_FAIL_LINEAR_TO_PAGE_TABLE         RT_BIT_32(1)
/** @} */

DECLCALLBACK(FNPGMPHYSHANDLER)      iemVmxApicAccessPageHandler;
# ifndef IN_RING3
DECLCALLBACK(FNPGMRZPHYSPFHANDLER)  iemVmxApicAccessPagePfHandler;
# endif
#endif

/**
 * Indicates to the verifier that the given flag set is undefined.
 *
 * Can be invoked again to add more flags.
 *
 * This is a NOOP if the verifier isn't compiled in.
 *
 * @note We're temporarily keeping this until code is converted to new
 *       disassembler style opcode handling.
 */
#define IEMOP_VERIFICATION_UNDEFINED_EFLAGS(a_fEfl) do { } while (0)


/** @def IEM_DECL_IMPL_TYPE
 * For typedef'ing an instruction implementation function.
 *
 * @param   a_RetType           The return type.
 * @param   a_Name              The name of the type.
 * @param   a_ArgList           The argument list enclosed in parentheses.
 */

/** @def IEM_DECL_IMPL_DEF
 * For defining an instruction implementation function.
 *
 * @param   a_RetType           The return type.
 * @param   a_Name              The name of the type.
 * @param   a_ArgList           The argument list enclosed in parentheses.
 */

#if defined(__GNUC__) && defined(RT_ARCH_X86)
# define IEM_DECL_IMPL_TYPE(a_RetType, a_Name, a_ArgList) \
    __attribute__((__fastcall__)) a_RetType (a_Name) a_ArgList
# define IEM_DECL_IMPL_DEF(a_RetType, a_Name, a_ArgList) \
    __attribute__((__fastcall__, __nothrow__)) a_RetType a_Name a_ArgList
# define IEM_DECL_IMPL_PROTO(a_RetType, a_Name, a_ArgList) \
    __attribute__((__fastcall__, __nothrow__)) a_RetType a_Name a_ArgList

#elif defined(_MSC_VER) && defined(RT_ARCH_X86)
# define IEM_DECL_IMPL_TYPE(a_RetType, a_Name, a_ArgList) \
    a_RetType (__fastcall a_Name) a_ArgList
# define IEM_DECL_IMPL_DEF(a_RetType, a_Name, a_ArgList) \
    a_RetType __fastcall a_Name a_ArgList RT_NOEXCEPT
# define IEM_DECL_IMPL_PROTO(a_RetType, a_Name, a_ArgList) \
    a_RetType __fastcall a_Name a_ArgList RT_NOEXCEPT

#elif __cplusplus >= 201700 /* P0012R1 support */
# define IEM_DECL_IMPL_TYPE(a_RetType, a_Name, a_ArgList) \
    a_RetType (VBOXCALL a_Name) a_ArgList RT_NOEXCEPT
# define IEM_DECL_IMPL_DEF(a_RetType, a_Name, a_ArgList) \
    a_RetType VBOXCALL a_Name a_ArgList RT_NOEXCEPT
# define IEM_DECL_IMPL_PROTO(a_RetType, a_Name, a_ArgList) \
    a_RetType VBOXCALL a_Name a_ArgList RT_NOEXCEPT

#else
# define IEM_DECL_IMPL_TYPE(a_RetType, a_Name, a_ArgList) \
    a_RetType (VBOXCALL a_Name) a_ArgList
# define IEM_DECL_IMPL_DEF(a_RetType, a_Name, a_ArgList) \
    a_RetType VBOXCALL a_Name a_ArgList
# define IEM_DECL_IMPL_PROTO(a_RetType, a_Name, a_ArgList) \
    a_RetType VBOXCALL a_Name a_ArgList

#endif

/** Defined in IEMAllAImplC.cpp but also used by IEMAllAImplA.asm. */
RT_C_DECLS_BEGIN
extern uint8_t const g_afParity[256];
RT_C_DECLS_END


/** @name Arithmetic assignment operations on bytes (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINU8,  (uint8_t  *pu8Dst,  uint8_t  u8Src,  uint32_t *pEFlags));
typedef FNIEMAIMPLBINU8  *PFNIEMAIMPLBINU8;
FNIEMAIMPLBINU8 iemAImpl_add_u8, iemAImpl_add_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_adc_u8, iemAImpl_adc_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_sub_u8, iemAImpl_sub_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_sbb_u8, iemAImpl_sbb_u8_locked;
FNIEMAIMPLBINU8  iemAImpl_or_u8,  iemAImpl_or_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_xor_u8, iemAImpl_xor_u8_locked;
FNIEMAIMPLBINU8 iemAImpl_and_u8, iemAImpl_and_u8_locked;
/** @} */

/** @name Arithmetic assignment operations on words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINU16,  (uint16_t *pu16Dst, uint16_t u16Src, uint32_t *pEFlags));
typedef FNIEMAIMPLBINU16  *PFNIEMAIMPLBINU16;
FNIEMAIMPLBINU16 iemAImpl_add_u16, iemAImpl_add_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_adc_u16, iemAImpl_adc_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_sub_u16, iemAImpl_sub_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_sbb_u16, iemAImpl_sbb_u16_locked;
FNIEMAIMPLBINU16  iemAImpl_or_u16,  iemAImpl_or_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_xor_u16, iemAImpl_xor_u16_locked;
FNIEMAIMPLBINU16 iemAImpl_and_u16, iemAImpl_and_u16_locked;
/** @}  */

/** @name Arithmetic assignment operations on double words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINU32, (uint32_t *pu32Dst, uint32_t u32Src, uint32_t *pEFlags));
typedef FNIEMAIMPLBINU32 *PFNIEMAIMPLBINU32;
FNIEMAIMPLBINU32 iemAImpl_add_u32, iemAImpl_add_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_adc_u32, iemAImpl_adc_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_sub_u32, iemAImpl_sub_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_sbb_u32, iemAImpl_sbb_u32_locked;
FNIEMAIMPLBINU32  iemAImpl_or_u32,  iemAImpl_or_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_xor_u32, iemAImpl_xor_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_and_u32, iemAImpl_and_u32_locked;
FNIEMAIMPLBINU32 iemAImpl_blsi_u32, iemAImpl_blsi_u32_fallback;
FNIEMAIMPLBINU32 iemAImpl_blsr_u32, iemAImpl_blsr_u32_fallback;
FNIEMAIMPLBINU32 iemAImpl_blsmsk_u32, iemAImpl_blsmsk_u32_fallback;
/** @}  */

/** @name Arithmetic assignment operations on quad words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINU64, (uint64_t *pu64Dst, uint64_t u64Src, uint32_t *pEFlags));
typedef FNIEMAIMPLBINU64 *PFNIEMAIMPLBINU64;
FNIEMAIMPLBINU64 iemAImpl_add_u64, iemAImpl_add_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_adc_u64, iemAImpl_adc_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_sub_u64, iemAImpl_sub_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_sbb_u64, iemAImpl_sbb_u64_locked;
FNIEMAIMPLBINU64  iemAImpl_or_u64,  iemAImpl_or_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_xor_u64, iemAImpl_xor_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_and_u64, iemAImpl_and_u64_locked;
FNIEMAIMPLBINU64 iemAImpl_blsi_u64, iemAImpl_blsi_u64_fallback;
FNIEMAIMPLBINU64 iemAImpl_blsr_u64, iemAImpl_blsr_u64_fallback;
FNIEMAIMPLBINU64 iemAImpl_blsmsk_u64, iemAImpl_blsmsk_u64_fallback;
/** @}  */

/** @name Compare operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINU8  iemAImpl_cmp_u8;
FNIEMAIMPLBINU16 iemAImpl_cmp_u16;
FNIEMAIMPLBINU32 iemAImpl_cmp_u32;
FNIEMAIMPLBINU64 iemAImpl_cmp_u64;
/** @}  */

/** @name Test operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINU8  iemAImpl_test_u8;
FNIEMAIMPLBINU16 iemAImpl_test_u16;
FNIEMAIMPLBINU32 iemAImpl_test_u32;
FNIEMAIMPLBINU64 iemAImpl_test_u64;
/** @}  */

/** @name Bit operations operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINU16 iemAImpl_bt_u16;
FNIEMAIMPLBINU32 iemAImpl_bt_u32;
FNIEMAIMPLBINU64 iemAImpl_bt_u64;
FNIEMAIMPLBINU16 iemAImpl_btc_u16, iemAImpl_btc_u16_locked;
FNIEMAIMPLBINU32 iemAImpl_btc_u32, iemAImpl_btc_u32_locked;
FNIEMAIMPLBINU64 iemAImpl_btc_u64, iemAImpl_btc_u64_locked;
FNIEMAIMPLBINU16 iemAImpl_btr_u16, iemAImpl_btr_u16_locked;
FNIEMAIMPLBINU32 iemAImpl_btr_u32, iemAImpl_btr_u32_locked;
FNIEMAIMPLBINU64 iemAImpl_btr_u64, iemAImpl_btr_u64_locked;
FNIEMAIMPLBINU16 iemAImpl_bts_u16, iemAImpl_bts_u16_locked;
FNIEMAIMPLBINU32 iemAImpl_bts_u32, iemAImpl_bts_u32_locked;
FNIEMAIMPLBINU64 iemAImpl_bts_u64, iemAImpl_bts_u64_locked;
/** @}  */

/** @name Arithmetic three operand operations on double words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINVEXU32, (uint32_t *pu32Dst, uint32_t u32Src1, uint32_t u32Src2, uint32_t *pEFlags));
typedef FNIEMAIMPLBINVEXU32 *PFNIEMAIMPLBINVEXU32;
FNIEMAIMPLBINVEXU32 iemAImpl_andn_u32, iemAImpl_andn_u32_fallback;
FNIEMAIMPLBINVEXU32 iemAImpl_bextr_u32, iemAImpl_bextr_u32_fallback;
FNIEMAIMPLBINVEXU32 iemAImpl_bzhi_u32, iemAImpl_bzhi_u32_fallback;
/** @}  */

/** @name Arithmetic three operand operations on quad words (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINVEXU64, (uint64_t *pu64Dst, uint64_t u64Src1, uint64_t u64Src2, uint32_t *pEFlags));
typedef FNIEMAIMPLBINVEXU64 *PFNIEMAIMPLBINVEXU64;
FNIEMAIMPLBINVEXU64 iemAImpl_andn_u64, iemAImpl_andn_u64_fallback;
FNIEMAIMPLBINVEXU64 iemAImpl_bextr_u64, iemAImpl_bextr_u64_fallback;
FNIEMAIMPLBINVEXU64 iemAImpl_bzhi_u64, iemAImpl_bzhi_u64_fallback;
/** @}  */

/** @name Arithmetic three operand operations on double words w/o EFLAGS (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINVEXU32NOEFL, (uint32_t *pu32Dst, uint32_t u32Src1, uint32_t u32Src2));
typedef FNIEMAIMPLBINVEXU32NOEFL *PFNIEMAIMPLBINVEXU32NOEFL;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_pdep_u32, iemAImpl_pdep_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_pext_u32, iemAImpl_pext_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_sarx_u32, iemAImpl_sarx_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_shlx_u32, iemAImpl_shlx_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_shrx_u32, iemAImpl_shrx_u32_fallback;
FNIEMAIMPLBINVEXU32NOEFL iemAImpl_rorx_u32;
/** @}  */

/** @name Arithmetic three operand operations on quad words w/o EFLAGS (binary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLBINVEXU64NOEFL, (uint64_t *pu64Dst, uint64_t u64Src1, uint64_t u64Src2));
typedef FNIEMAIMPLBINVEXU64NOEFL *PFNIEMAIMPLBINVEXU64NOEFL;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_pdep_u64, iemAImpl_pdep_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_pext_u64, iemAImpl_pext_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_sarx_u64, iemAImpl_sarx_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_shlx_u64, iemAImpl_shlx_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_shrx_u64, iemAImpl_shrx_u64_fallback;
FNIEMAIMPLBINVEXU64NOEFL iemAImpl_rorx_u64;
/** @}  */

/** @name MULX 32-bit and 64-bit.
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMULXVEXU32, (uint32_t *puDst1, uint32_t *puDst2, uint32_t uSrc1, uint32_t uSrc2));
typedef FNIEMAIMPLMULXVEXU32 *PFNIEMAIMPLMULXVEXU32;
FNIEMAIMPLMULXVEXU32 iemAImpl_mulx_u32, iemAImpl_mulx_u32_fallback;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMULXVEXU64, (uint64_t *puDst1, uint64_t *puDst2, uint64_t uSrc1, uint64_t uSrc2));
typedef FNIEMAIMPLMULXVEXU64 *PFNIEMAIMPLMULXVEXU64;
FNIEMAIMPLMULXVEXU64 iemAImpl_mulx_u64, iemAImpl_mulx_u64_fallback;
/** @}  */


/** @name Exchange memory with register operations.
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_locked, (uint8_t  *pu8Mem,  uint8_t  *pu8Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16_locked,(uint16_t *pu16Mem, uint16_t *pu16Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32_locked,(uint32_t *pu32Mem, uint32_t *pu32Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64_locked,(uint64_t *pu64Mem, uint64_t *pu64Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_unlocked, (uint8_t  *pu8Mem,  uint8_t  *pu8Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16_unlocked,(uint16_t *pu16Mem, uint16_t *pu16Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32_unlocked,(uint32_t *pu32Mem, uint32_t *pu32Reg));
IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64_unlocked,(uint64_t *pu64Mem, uint64_t *pu64Reg));
/** @}  */

/** @name Exchange and add operations.
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u8, (uint8_t  *pu8Dst,  uint8_t  *pu8Reg,  uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u16,(uint16_t *pu16Dst, uint16_t *pu16Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u32,(uint32_t *pu32Dst, uint32_t *pu32Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u64,(uint64_t *pu64Dst, uint64_t *pu64Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u8_locked, (uint8_t  *pu8Dst,  uint8_t  *pu8Reg,  uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u16_locked,(uint16_t *pu16Dst, uint16_t *pu16Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u32_locked,(uint32_t *pu32Dst, uint32_t *pu32Reg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u64_locked,(uint64_t *pu64Dst, uint64_t *pu64Reg, uint32_t *pEFlags));
/** @}  */

/** @name Compare and exchange.
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u8,        (uint8_t  *pu8Dst,  uint8_t  *puAl,  uint8_t  uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u8_locked, (uint8_t  *pu8Dst,  uint8_t  *puAl,  uint8_t  uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u16,       (uint16_t *pu16Dst, uint16_t *puAx,  uint16_t uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u16_locked,(uint16_t *pu16Dst, uint16_t *puAx,  uint16_t uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u32,       (uint32_t *pu32Dst, uint32_t *puEax, uint32_t uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u32_locked,(uint32_t *pu32Dst, uint32_t *puEax, uint32_t uSrcReg, uint32_t *pEFlags));
#if ARCH_BITS == 32
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64,       (uint64_t *pu64Dst, uint64_t *puRax, uint64_t *puSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64_locked,(uint64_t *pu64Dst, uint64_t *puRax, uint64_t *puSrcReg, uint32_t *pEFlags));
#else
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64,       (uint64_t *pu64Dst, uint64_t *puRax, uint64_t uSrcReg, uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64_locked,(uint64_t *pu64Dst, uint64_t *puRax, uint64_t uSrcReg, uint32_t *pEFlags));
#endif
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg8b,(uint64_t *pu64Dst, PRTUINT64U pu64EaxEdx, PRTUINT64U pu64EbxEcx,
                                            uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg8b_locked,(uint64_t *pu64Dst, PRTUINT64U pu64EaxEdx, PRTUINT64U pu64EbxEcx,
                                                   uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx, PRTUINT128U pu128RbxRcx,
                                             uint32_t *pEFlags));
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b_locked,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx, PRTUINT128U pu128RbxRcx,
                                                    uint32_t *pEFlags));
#ifndef RT_ARCH_ARM64
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b_fallback,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx,
                                                      PRTUINT128U pu128RbxRcx, uint32_t *pEFlags));
#endif
/** @} */

/** @name Memory ordering
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEMFENCE,(void));
typedef FNIEMAIMPLMEMFENCE *PFNIEMAIMPLMEMFENCE;
IEM_DECL_IMPL_DEF(void, iemAImpl_mfence,(void));
IEM_DECL_IMPL_DEF(void, iemAImpl_sfence,(void));
IEM_DECL_IMPL_DEF(void, iemAImpl_lfence,(void));
#ifndef RT_ARCH_ARM64
IEM_DECL_IMPL_DEF(void, iemAImpl_alt_mem_fence,(void));
#endif
/** @} */

/** @name Double precision shifts
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTDBLU16,(uint16_t *pu16Dst, uint16_t u16Src, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTDBLU16  *PFNIEMAIMPLSHIFTDBLU16;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTDBLU32,(uint32_t *pu32Dst, uint32_t u32Src, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTDBLU32  *PFNIEMAIMPLSHIFTDBLU32;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTDBLU64,(uint64_t *pu64Dst, uint64_t u64Src, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTDBLU64  *PFNIEMAIMPLSHIFTDBLU64;
FNIEMAIMPLSHIFTDBLU16 iemAImpl_shld_u16, iemAImpl_shld_u16_amd, iemAImpl_shld_u16_intel;
FNIEMAIMPLSHIFTDBLU32 iemAImpl_shld_u32, iemAImpl_shld_u32_amd, iemAImpl_shld_u32_intel;
FNIEMAIMPLSHIFTDBLU64 iemAImpl_shld_u64, iemAImpl_shld_u64_amd, iemAImpl_shld_u64_intel;
FNIEMAIMPLSHIFTDBLU16 iemAImpl_shrd_u16, iemAImpl_shrd_u16_amd, iemAImpl_shrd_u16_intel;
FNIEMAIMPLSHIFTDBLU32 iemAImpl_shrd_u32, iemAImpl_shrd_u32_amd, iemAImpl_shrd_u32_intel;
FNIEMAIMPLSHIFTDBLU64 iemAImpl_shrd_u64, iemAImpl_shrd_u64_amd, iemAImpl_shrd_u64_intel;
/** @}  */


/** @name Bit search operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINU16 iemAImpl_bsf_u16, iemAImpl_bsf_u16_amd, iemAImpl_bsf_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_bsf_u32, iemAImpl_bsf_u32_amd, iemAImpl_bsf_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_bsf_u64, iemAImpl_bsf_u64_amd, iemAImpl_bsf_u64_intel;
FNIEMAIMPLBINU16 iemAImpl_bsr_u16, iemAImpl_bsr_u16_amd, iemAImpl_bsr_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_bsr_u32, iemAImpl_bsr_u32_amd, iemAImpl_bsr_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_bsr_u64, iemAImpl_bsr_u64_amd, iemAImpl_bsr_u64_intel;
FNIEMAIMPLBINU16 iemAImpl_lzcnt_u16, iemAImpl_lzcnt_u16_amd, iemAImpl_lzcnt_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_lzcnt_u32, iemAImpl_lzcnt_u32_amd, iemAImpl_lzcnt_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_lzcnt_u64, iemAImpl_lzcnt_u64_amd, iemAImpl_lzcnt_u64_intel;
FNIEMAIMPLBINU16 iemAImpl_tzcnt_u16, iemAImpl_tzcnt_u16_amd, iemAImpl_tzcnt_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_tzcnt_u32, iemAImpl_tzcnt_u32_amd, iemAImpl_tzcnt_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_tzcnt_u64, iemAImpl_tzcnt_u64_amd, iemAImpl_tzcnt_u64_intel;
FNIEMAIMPLBINU16 iemAImpl_popcnt_u16, iemAImpl_popcnt_u16_fallback;
FNIEMAIMPLBINU32 iemAImpl_popcnt_u32, iemAImpl_popcnt_u32_fallback;
FNIEMAIMPLBINU64 iemAImpl_popcnt_u64, iemAImpl_popcnt_u64_fallback;
/** @}  */

/** @name Signed multiplication operations (thrown in with the binary ops).
 * @{ */
FNIEMAIMPLBINU16 iemAImpl_imul_two_u16, iemAImpl_imul_two_u16_amd, iemAImpl_imul_two_u16_intel;
FNIEMAIMPLBINU32 iemAImpl_imul_two_u32, iemAImpl_imul_two_u32_amd, iemAImpl_imul_two_u32_intel;
FNIEMAIMPLBINU64 iemAImpl_imul_two_u64, iemAImpl_imul_two_u64_amd, iemAImpl_imul_two_u64_intel;
/** @}  */

/** @name Arithmetic assignment operations on bytes (unary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLUNARYU8,  (uint8_t  *pu8Dst,  uint32_t *pEFlags));
typedef FNIEMAIMPLUNARYU8  *PFNIEMAIMPLUNARYU8;
FNIEMAIMPLUNARYU8 iemAImpl_inc_u8, iemAImpl_inc_u8_locked;
FNIEMAIMPLUNARYU8 iemAImpl_dec_u8, iemAImpl_dec_u8_locked;
FNIEMAIMPLUNARYU8 iemAImpl_not_u8, iemAImpl_not_u8_locked;
FNIEMAIMPLUNARYU8 iemAImpl_neg_u8, iemAImpl_neg_u8_locked;
/** @} */

/** @name Arithmetic assignment operations on words (unary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLUNARYU16,  (uint16_t  *pu16Dst,  uint32_t *pEFlags));
typedef FNIEMAIMPLUNARYU16  *PFNIEMAIMPLUNARYU16;
FNIEMAIMPLUNARYU16 iemAImpl_inc_u16, iemAImpl_inc_u16_locked;
FNIEMAIMPLUNARYU16 iemAImpl_dec_u16, iemAImpl_dec_u16_locked;
FNIEMAIMPLUNARYU16 iemAImpl_not_u16, iemAImpl_not_u16_locked;
FNIEMAIMPLUNARYU16 iemAImpl_neg_u16, iemAImpl_neg_u16_locked;
/** @} */

/** @name Arithmetic assignment operations on double words (unary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLUNARYU32,  (uint32_t  *pu32Dst,  uint32_t *pEFlags));
typedef FNIEMAIMPLUNARYU32  *PFNIEMAIMPLUNARYU32;
FNIEMAIMPLUNARYU32 iemAImpl_inc_u32, iemAImpl_inc_u32_locked;
FNIEMAIMPLUNARYU32 iemAImpl_dec_u32, iemAImpl_dec_u32_locked;
FNIEMAIMPLUNARYU32 iemAImpl_not_u32, iemAImpl_not_u32_locked;
FNIEMAIMPLUNARYU32 iemAImpl_neg_u32, iemAImpl_neg_u32_locked;
/** @} */

/** @name Arithmetic assignment operations on quad words (unary).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLUNARYU64,  (uint64_t  *pu64Dst,  uint32_t *pEFlags));
typedef FNIEMAIMPLUNARYU64  *PFNIEMAIMPLUNARYU64;
FNIEMAIMPLUNARYU64 iemAImpl_inc_u64, iemAImpl_inc_u64_locked;
FNIEMAIMPLUNARYU64 iemAImpl_dec_u64, iemAImpl_dec_u64_locked;
FNIEMAIMPLUNARYU64 iemAImpl_not_u64, iemAImpl_not_u64_locked;
FNIEMAIMPLUNARYU64 iemAImpl_neg_u64, iemAImpl_neg_u64_locked;
/** @} */


/** @name Shift operations on bytes (Group 2).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTU8,(uint8_t *pu8Dst, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTU8  *PFNIEMAIMPLSHIFTU8;
FNIEMAIMPLSHIFTU8 iemAImpl_rol_u8, iemAImpl_rol_u8_amd, iemAImpl_rol_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_ror_u8, iemAImpl_ror_u8_amd, iemAImpl_ror_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_rcl_u8, iemAImpl_rcl_u8_amd, iemAImpl_rcl_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_rcr_u8, iemAImpl_rcr_u8_amd, iemAImpl_rcr_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_shl_u8, iemAImpl_shl_u8_amd, iemAImpl_shl_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_shr_u8, iemAImpl_shr_u8_amd, iemAImpl_shr_u8_intel;
FNIEMAIMPLSHIFTU8 iemAImpl_sar_u8, iemAImpl_sar_u8_amd, iemAImpl_sar_u8_intel;
/** @} */

/** @name Shift operations on words (Group 2).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTU16,(uint16_t *pu16Dst, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTU16  *PFNIEMAIMPLSHIFTU16;
FNIEMAIMPLSHIFTU16 iemAImpl_rol_u16, iemAImpl_rol_u16_amd, iemAImpl_rol_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_ror_u16, iemAImpl_ror_u16_amd, iemAImpl_ror_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_rcl_u16, iemAImpl_rcl_u16_amd, iemAImpl_rcl_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_rcr_u16, iemAImpl_rcr_u16_amd, iemAImpl_rcr_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_shl_u16, iemAImpl_shl_u16_amd, iemAImpl_shl_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_shr_u16, iemAImpl_shr_u16_amd, iemAImpl_shr_u16_intel;
FNIEMAIMPLSHIFTU16 iemAImpl_sar_u16, iemAImpl_sar_u16_amd, iemAImpl_sar_u16_intel;
/** @} */

/** @name Shift operations on double words (Group 2).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTU32,(uint32_t *pu32Dst, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTU32  *PFNIEMAIMPLSHIFTU32;
FNIEMAIMPLSHIFTU32 iemAImpl_rol_u32, iemAImpl_rol_u32_amd, iemAImpl_rol_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_ror_u32, iemAImpl_ror_u32_amd, iemAImpl_ror_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_rcl_u32, iemAImpl_rcl_u32_amd, iemAImpl_rcl_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_rcr_u32, iemAImpl_rcr_u32_amd, iemAImpl_rcr_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_shl_u32, iemAImpl_shl_u32_amd, iemAImpl_shl_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_shr_u32, iemAImpl_shr_u32_amd, iemAImpl_shr_u32_intel;
FNIEMAIMPLSHIFTU32 iemAImpl_sar_u32, iemAImpl_sar_u32_amd, iemAImpl_sar_u32_intel;
/** @} */

/** @name Shift operations on words (Group 2).
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLSHIFTU64,(uint64_t *pu64Dst, uint8_t cShift, uint32_t *pEFlags));
typedef FNIEMAIMPLSHIFTU64  *PFNIEMAIMPLSHIFTU64;
FNIEMAIMPLSHIFTU64 iemAImpl_rol_u64, iemAImpl_rol_u64_amd, iemAImpl_rol_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_ror_u64, iemAImpl_ror_u64_amd, iemAImpl_ror_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_rcl_u64, iemAImpl_rcl_u64_amd, iemAImpl_rcl_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_rcr_u64, iemAImpl_rcr_u64_amd, iemAImpl_rcr_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_shl_u64, iemAImpl_shl_u64_amd, iemAImpl_shl_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_shr_u64, iemAImpl_shr_u64_amd, iemAImpl_shr_u64_intel;
FNIEMAIMPLSHIFTU64 iemAImpl_sar_u64, iemAImpl_sar_u64_amd, iemAImpl_sar_u64_intel;
/** @} */

/** @name Multiplication and division operations.
 * @{ */
typedef IEM_DECL_IMPL_TYPE(int, FNIEMAIMPLMULDIVU8,(uint16_t *pu16AX, uint8_t u8FactorDivisor, uint32_t *pEFlags));
typedef FNIEMAIMPLMULDIVU8  *PFNIEMAIMPLMULDIVU8;
FNIEMAIMPLMULDIVU8 iemAImpl_mul_u8,  iemAImpl_mul_u8_amd,  iemAImpl_mul_u8_intel;
FNIEMAIMPLMULDIVU8 iemAImpl_imul_u8, iemAImpl_imul_u8_amd, iemAImpl_imul_u8_intel;
FNIEMAIMPLMULDIVU8 iemAImpl_div_u8,  iemAImpl_div_u8_amd,  iemAImpl_div_u8_intel;
FNIEMAIMPLMULDIVU8 iemAImpl_idiv_u8, iemAImpl_idiv_u8_amd, iemAImpl_idiv_u8_intel;

typedef IEM_DECL_IMPL_TYPE(int, FNIEMAIMPLMULDIVU16,(uint16_t *pu16AX, uint16_t *pu16DX, uint16_t u16FactorDivisor, uint32_t *pEFlags));
typedef FNIEMAIMPLMULDIVU16  *PFNIEMAIMPLMULDIVU16;
FNIEMAIMPLMULDIVU16 iemAImpl_mul_u16,  iemAImpl_mul_u16_amd,  iemAImpl_mul_u16_intel;
FNIEMAIMPLMULDIVU16 iemAImpl_imul_u16, iemAImpl_imul_u16_amd, iemAImpl_imul_u16_intel;
FNIEMAIMPLMULDIVU16 iemAImpl_div_u16,  iemAImpl_div_u16_amd,  iemAImpl_div_u16_intel;
FNIEMAIMPLMULDIVU16 iemAImpl_idiv_u16, iemAImpl_idiv_u16_amd, iemAImpl_idiv_u16_intel;

typedef IEM_DECL_IMPL_TYPE(int, FNIEMAIMPLMULDIVU32,(uint32_t *pu32EAX, uint32_t *pu32EDX, uint32_t u32FactorDivisor, uint32_t *pEFlags));
typedef FNIEMAIMPLMULDIVU32  *PFNIEMAIMPLMULDIVU32;
FNIEMAIMPLMULDIVU32 iemAImpl_mul_u32,  iemAImpl_mul_u32_amd,  iemAImpl_mul_u32_intel;
FNIEMAIMPLMULDIVU32 iemAImpl_imul_u32, iemAImpl_imul_u32_amd, iemAImpl_imul_u32_intel;
FNIEMAIMPLMULDIVU32 iemAImpl_div_u32,  iemAImpl_div_u32_amd,  iemAImpl_div_u32_intel;
FNIEMAIMPLMULDIVU32 iemAImpl_idiv_u32, iemAImpl_idiv_u32_amd, iemAImpl_idiv_u32_intel;

typedef IEM_DECL_IMPL_TYPE(int, FNIEMAIMPLMULDIVU64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64FactorDivisor, uint32_t *pEFlags));
typedef FNIEMAIMPLMULDIVU64  *PFNIEMAIMPLMULDIVU64;
FNIEMAIMPLMULDIVU64 iemAImpl_mul_u64,  iemAImpl_mul_u64_amd,  iemAImpl_mul_u64_intel;
FNIEMAIMPLMULDIVU64 iemAImpl_imul_u64, iemAImpl_imul_u64_amd, iemAImpl_imul_u64_intel;
FNIEMAIMPLMULDIVU64 iemAImpl_div_u64,  iemAImpl_div_u64_amd,  iemAImpl_div_u64_intel;
FNIEMAIMPLMULDIVU64 iemAImpl_idiv_u64, iemAImpl_idiv_u64_amd, iemAImpl_idiv_u64_intel;
/** @} */

/** @name Byte Swap.
 * @{  */
IEM_DECL_IMPL_TYPE(void, iemAImpl_bswap_u16,(uint32_t *pu32Dst)); /* Yes, 32-bit register access. */
IEM_DECL_IMPL_TYPE(void, iemAImpl_bswap_u32,(uint32_t *pu32Dst));
IEM_DECL_IMPL_TYPE(void, iemAImpl_bswap_u64,(uint64_t *pu64Dst));
/** @}  */

/** @name Misc.
 * @{ */
FNIEMAIMPLBINU16 iemAImpl_arpl;
/** @} */


/** @name FPU operations taking a 32-bit float argument
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR32FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2));
typedef FNIEMAIMPLFPUR32FSW *PFNIEMAIMPLFPUR32FSW;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2));
typedef FNIEMAIMPLFPUR32    *PFNIEMAIMPLFPUR32;

FNIEMAIMPLFPUR32FSW iemAImpl_fcom_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fadd_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fmul_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fsub_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fsubr_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fdiv_r80_by_r32;
FNIEMAIMPLFPUR32    iemAImpl_fdivr_r80_by_r32;

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT32U pr32Val));
IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r32,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT32U pr32Val, PCRTFLOAT80U pr80Val));
/** @} */

/** @name FPU operations taking a 64-bit float argument
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR64FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2));
typedef FNIEMAIMPLFPUR64FSW *PFNIEMAIMPLFPUR64FSW;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2));
typedef FNIEMAIMPLFPUR64   *PFNIEMAIMPLFPUR64;

FNIEMAIMPLFPUR64FSW iemAImpl_fcom_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fadd_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fmul_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fsub_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fsubr_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fdiv_r80_by_r64;
FNIEMAIMPLFPUR64    iemAImpl_fdivr_r80_by_r64;

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT64U pr64Val));
IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r64,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT64U pr32Val, PCRTFLOAT80U pr80Val));
/** @} */

/** @name FPU operations taking a 80-bit float argument
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2));
typedef FNIEMAIMPLFPUR80    *PFNIEMAIMPLFPUR80;
FNIEMAIMPLFPUR80            iemAImpl_fadd_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fmul_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fsub_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fsubr_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fdiv_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fdivr_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fprem_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fprem1_r80_by_r80;
FNIEMAIMPLFPUR80            iemAImpl_fscale_r80_by_r80;

FNIEMAIMPLFPUR80            iemAImpl_fpatan_r80_by_r80,  iemAImpl_fpatan_r80_by_r80_amd,  iemAImpl_fpatan_r80_by_r80_intel;
FNIEMAIMPLFPUR80            iemAImpl_fyl2x_r80_by_r80,   iemAImpl_fyl2x_r80_by_r80_amd,   iemAImpl_fyl2x_r80_by_r80_intel;
FNIEMAIMPLFPUR80            iemAImpl_fyl2xp1_r80_by_r80, iemAImpl_fyl2xp1_r80_by_r80_amd, iemAImpl_fyl2xp1_r80_by_r80_intel;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2));
typedef FNIEMAIMPLFPUR80FSW *PFNIEMAIMPLFPUR80FSW;
FNIEMAIMPLFPUR80FSW         iemAImpl_fcom_r80_by_r80;
FNIEMAIMPLFPUR80FSW         iemAImpl_fucom_r80_by_r80;

typedef IEM_DECL_IMPL_TYPE(uint32_t, FNIEMAIMPLFPUR80EFL,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw,
                                                          PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2));
typedef FNIEMAIMPLFPUR80EFL *PFNIEMAIMPLFPUR80EFL;
FNIEMAIMPLFPUR80EFL         iemAImpl_fcomi_r80_by_r80;
FNIEMAIMPLFPUR80EFL         iemAImpl_fucomi_r80_by_r80;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80UNARY,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val));
typedef FNIEMAIMPLFPUR80UNARY *PFNIEMAIMPLFPUR80UNARY;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fabs_r80;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fchs_r80;
FNIEMAIMPLFPUR80UNARY       iemAImpl_f2xm1_r80, iemAImpl_f2xm1_r80_amd, iemAImpl_f2xm1_r80_intel;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fsqrt_r80;
FNIEMAIMPLFPUR80UNARY       iemAImpl_frndint_r80;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fsin_r80, iemAImpl_fsin_r80_amd, iemAImpl_fsin_r80_intel;
FNIEMAIMPLFPUR80UNARY       iemAImpl_fcos_r80, iemAImpl_fcos_r80_amd, iemAImpl_fcos_r80_intel;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80UNARYFSW,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw, PCRTFLOAT80U pr80Val));
typedef FNIEMAIMPLFPUR80UNARYFSW *PFNIEMAIMPLFPUR80UNARYFSW;
FNIEMAIMPLFPUR80UNARYFSW    iemAImpl_ftst_r80;
FNIEMAIMPLFPUR80UNARYFSW    iemAImpl_fxam_r80;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80LDCONST,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes));
typedef FNIEMAIMPLFPUR80LDCONST *PFNIEMAIMPLFPUR80LDCONST;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fld1;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldl2t;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldl2e;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldpi;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldlg2;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldln2;
FNIEMAIMPLFPUR80LDCONST     iemAImpl_fldz;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUR80UNARYTWO,(PCX86FXSTATE pFpuState, PIEMFPURESULTTWO pFpuResTwo,
                                                           PCRTFLOAT80U pr80Val));
typedef FNIEMAIMPLFPUR80UNARYTWO *PFNIEMAIMPLFPUR80UNARYTWO;
FNIEMAIMPLFPUR80UNARYTWO    iemAImpl_fptan_r80_r80, iemAImpl_fptan_r80_r80_amd, iemAImpl_fptan_r80_r80_intel;
FNIEMAIMPLFPUR80UNARYTWO    iemAImpl_fxtract_r80_r80;
FNIEMAIMPLFPUR80UNARYTWO    iemAImpl_fsincos_r80_r80, iemAImpl_fsincos_r80_r80_amd, iemAImpl_fsincos_r80_r80_intel;

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val));
IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r80,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT80U pr80Dst, PCRTFLOAT80U pr80Src));

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_d80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTPBCD80U pd80Val));
IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_d80,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTPBCD80U pd80Dst, PCRTFLOAT80U pr80Src));

/** @} */

/** @name FPU operations taking a 16-bit signed integer argument
 * @{  */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUI16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2));
typedef FNIEMAIMPLFPUI16 *PFNIEMAIMPLFPUI16;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUSTR80TOI16,(PCX86FXSTATE pFpuState, uint16_t *pFpuRes,
                                                          int16_t *pi16Dst, PCRTFLOAT80U pr80Src));
typedef FNIEMAIMPLFPUSTR80TOI16 *PFNIEMAIMPLFPUSTR80TOI16;

FNIEMAIMPLFPUI16    iemAImpl_fiadd_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fimul_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fisub_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fisubr_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fidiv_r80_by_i16;
FNIEMAIMPLFPUI16    iemAImpl_fidivr_r80_by_i16;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUI16FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2));
typedef FNIEMAIMPLFPUI16FSW *PFNIEMAIMPLFPUI16FSW;
FNIEMAIMPLFPUI16FSW     iemAImpl_ficom_r80_by_i16;

IEM_DECL_IMPL_DEF(void, iemAImpl_fild_r80_from_i16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int16_t const *pi16Val));
FNIEMAIMPLFPUSTR80TOI16 iemAImpl_fist_r80_to_i16;
FNIEMAIMPLFPUSTR80TOI16 iemAImpl_fistt_r80_to_i16, iemAImpl_fistt_r80_to_i16_amd, iemAImpl_fistt_r80_to_i16_intel;
/** @}  */

/** @name FPU operations taking a 32-bit signed integer argument
 * @{  */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUI32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2));
typedef FNIEMAIMPLFPUI32 *PFNIEMAIMPLFPUI32;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUSTR80TOI32,(PCX86FXSTATE pFpuState, uint16_t *pFpuRes,
                                                          int32_t *pi32Dst, PCRTFLOAT80U pr80Src));
typedef FNIEMAIMPLFPUSTR80TOI32 *PFNIEMAIMPLFPUSTR80TOI32;

FNIEMAIMPLFPUI32    iemAImpl_fiadd_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fimul_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fisub_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fisubr_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fidiv_r80_by_i32;
FNIEMAIMPLFPUI32    iemAImpl_fidivr_r80_by_i32;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUI32FSW,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                      PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2));
typedef FNIEMAIMPLFPUI32FSW *PFNIEMAIMPLFPUI32FSW;
FNIEMAIMPLFPUI32FSW     iemAImpl_ficom_r80_by_i32;

IEM_DECL_IMPL_DEF(void, iemAImpl_fild_r80_from_i32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int32_t const *pi32Val));
FNIEMAIMPLFPUSTR80TOI32 iemAImpl_fist_r80_to_i32;
FNIEMAIMPLFPUSTR80TOI32 iemAImpl_fistt_r80_to_i32;
/** @}  */

/** @name FPU operations taking a 64-bit signed integer argument
 * @{  */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLFPUSTR80TOI64,(PCX86FXSTATE pFpuState, uint16_t *pFpuRes,
                                                          int64_t *pi64Dst, PCRTFLOAT80U pr80Src));
typedef FNIEMAIMPLFPUSTR80TOI64 *PFNIEMAIMPLFPUSTR80TOI64;

IEM_DECL_IMPL_DEF(void, iemAImpl_fild_r80_from_i64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int64_t const *pi64Val));
FNIEMAIMPLFPUSTR80TOI64 iemAImpl_fist_r80_to_i64;
FNIEMAIMPLFPUSTR80TOI64 iemAImpl_fistt_r80_to_i64;
/** @} */


/** Temporary type representing a 256-bit vector register. */
typedef struct { uint64_t au64[4]; } IEMVMM256;
/** Temporary type pointing to a 256-bit vector register. */
typedef IEMVMM256 *PIEMVMM256;
/** Temporary type pointing to a const 256-bit vector register. */
typedef IEMVMM256 *PCIEMVMM256;


/** @name Media (SSE/MMX/AVX) operations: full1 + full2 -> full1.
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAF2U64,(PCX86FXSTATE pFpuState, uint64_t *puDst, uint64_t const *puSrc));
typedef FNIEMAIMPLMEDIAF2U64   *PFNIEMAIMPLMEDIAF2U64;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAF2U128,(PCX86FXSTATE pFpuState, PRTUINT128U puDst, PCRTUINT128U puSrc));
typedef FNIEMAIMPLMEDIAF2U128  *PFNIEMAIMPLMEDIAF2U128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAF3U128,(PX86XSAVEAREA pExtState, PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2));
typedef FNIEMAIMPLMEDIAF3U128  *PFNIEMAIMPLMEDIAF3U128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAF3U256,(PX86XSAVEAREA pExtState, PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2));
typedef FNIEMAIMPLMEDIAF3U256  *PFNIEMAIMPLMEDIAF3U256;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF2U64,(uint64_t *puDst, uint64_t const *puSrc));
typedef FNIEMAIMPLMEDIAOPTF2U64   *PFNIEMAIMPLMEDIAOPTF2U64;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF2U128,(PRTUINT128U puDst, PCRTUINT128U puSrc));
typedef FNIEMAIMPLMEDIAOPTF2U128  *PFNIEMAIMPLMEDIAOPTF2U128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF3U128,(PRTUINT128U puDst, PCRTUINT128U puSrc1, PCRTUINT128U puSrc2));
typedef FNIEMAIMPLMEDIAOPTF3U128  *PFNIEMAIMPLMEDIAOPTF3U128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF3U256,(PRTUINT256U puDst, PCRTUINT256U puSrc1, PCRTUINT256U puSrc2));
typedef FNIEMAIMPLMEDIAOPTF3U256  *PFNIEMAIMPLMEDIAOPTF3U256;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAOPTF2U256,(PRTUINT256U puDst, PCRTUINT256U puSrc));
typedef FNIEMAIMPLMEDIAOPTF2U256  *PFNIEMAIMPLMEDIAOPTF2U256;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pshufb_u64, iemAImpl_pshufb_u64_fallback;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pand_u64, iemAImpl_pandn_u64, iemAImpl_por_u64, iemAImpl_pxor_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pcmpeqb_u64,  iemAImpl_pcmpeqw_u64,  iemAImpl_pcmpeqd_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pcmpgtb_u64,  iemAImpl_pcmpgtw_u64,  iemAImpl_pcmpgtd_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_paddb_u64, iemAImpl_paddsb_u64, iemAImpl_paddusb_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_paddw_u64, iemAImpl_paddsw_u64, iemAImpl_paddusw_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_paddd_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_paddq_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_psubb_u64, iemAImpl_psubsb_u64, iemAImpl_psubusb_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_psubw_u64, iemAImpl_psubsw_u64, iemAImpl_psubusw_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_psubd_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_psubq_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pmaddwd_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pmullw_u64, iemAImpl_pmulhw_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pminub_u64, iemAImpl_pmaxub_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pminsw_u64, iemAImpl_pmaxsw_u64;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pabsb_u64, iemAImpl_pabsb_u64_fallback;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pabsw_u64, iemAImpl_pabsw_u64_fallback;
FNIEMAIMPLMEDIAF2U64     iemAImpl_pabsd_u64, iemAImpl_pabsd_u64_fallback;
FNIEMAIMPLMEDIAF2U64     iemAImpl_psignb_u64, iemAImpl_psignb_u64_fallback;
FNIEMAIMPLMEDIAF2U64     iemAImpl_psignw_u64, iemAImpl_psignw_u64_fallback;
FNIEMAIMPLMEDIAF2U64     iemAImpl_psignd_u64, iemAImpl_psignd_u64_fallback;
FNIEMAIMPLMEDIAF2U64     iemAImpl_phaddw_u64, iemAImpl_phaddw_u64_fallback;
FNIEMAIMPLMEDIAF2U64     iemAImpl_phaddd_u64, iemAImpl_phaddd_u64_fallback;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psllw_u64, iemAImpl_psrlw_u64, iemAImpl_psraw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pslld_u64, iemAImpl_psrld_u64, iemAImpl_psrad_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_psllq_u64, iemAImpl_psrlq_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_packsswb_u64, iemAImpl_packuswb_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_packssdw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pmulhuw_u64;
FNIEMAIMPLMEDIAOPTF2U64  iemAImpl_pavgb_u64, iemAImpl_pavgw_u64;

FNIEMAIMPLMEDIAF2U128    iemAImpl_pshufb_u128, iemAImpl_pshufb_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pand_u128, iemAImpl_pandn_u128, iemAImpl_por_u128, iemAImpl_pxor_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pcmpeqb_u128, iemAImpl_pcmpeqw_u128, iemAImpl_pcmpeqd_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pcmpeqq_u128, iemAImpl_pcmpeqq_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pcmpgtb_u128, iemAImpl_pcmpgtw_u128, iemAImpl_pcmpgtd_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pcmpgtq_u128, iemAImpl_pcmpgtq_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_paddb_u128, iemAImpl_paddsb_u128, iemAImpl_paddusb_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_paddw_u128, iemAImpl_paddsw_u128, iemAImpl_paddusw_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_paddd_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_paddq_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_psubb_u128, iemAImpl_psubsb_u128, iemAImpl_psubusb_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_psubw_u128, iemAImpl_psubsw_u128, iemAImpl_psubusw_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_psubd_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_psubq_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmullw_u128, iemAImpl_pmullw_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmulhw_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmulld_u128, iemAImpl_pmulld_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmaddwd_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pminub_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pminud_u128, iemAImpl_pminud_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pminuw_u128, iemAImpl_pminuw_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pminsb_u128, iemAImpl_pminsb_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pminsd_u128, iemAImpl_pminsd_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pminsw_u128, iemAImpl_pminsw_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmaxub_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmaxud_u128, iemAImpl_pmaxud_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmaxuw_u128, iemAImpl_pmaxuw_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmaxsb_u128, iemAImpl_pmaxsb_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmaxsw_u128;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pmaxsd_u128, iemAImpl_pmaxsd_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pabsb_u128, iemAImpl_pabsb_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pabsw_u128, iemAImpl_pabsw_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_pabsd_u128, iemAImpl_pabsd_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_psignb_u128, iemAImpl_psignb_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_psignw_u128, iemAImpl_psignw_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_psignd_u128, iemAImpl_psignd_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_phaddw_u128, iemAImpl_phaddw_u128_fallback;
FNIEMAIMPLMEDIAF2U128    iemAImpl_phaddd_u128, iemAImpl_phaddd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_packsswb_u128, iemAImpl_packuswb_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_packssdw_u128, iemAImpl_packusdw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psllw_u128, iemAImpl_psrlw_u128, iemAImpl_psraw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pslld_u128, iemAImpl_psrld_u128, iemAImpl_psrad_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_psllq_u128, iemAImpl_psrlq_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pmulhuw_u128;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_pavgb_u128, iemAImpl_pavgw_u128;

FNIEMAIMPLMEDIAF3U128    iemAImpl_vpshufb_u128,    iemAImpl_vpshufb_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpand_u128,      iemAImpl_vpand_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpandn_u128,     iemAImpl_vpandn_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpor_u128,       iemAImpl_vpor_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpxor_u128,      iemAImpl_vpxor_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpcmpeqb_u128,   iemAImpl_vpcmpeqb_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpcmpeqw_u128,   iemAImpl_vpcmpeqw_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpcmpeqd_u128,   iemAImpl_vpcmpeqd_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpcmpeqq_u128,   iemAImpl_vpcmpeqq_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpcmpgtb_u128,   iemAImpl_vpcmpgtb_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpcmpgtw_u128,   iemAImpl_vpcmpgtw_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpcmpgtd_u128,   iemAImpl_vpcmpgtd_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpcmpgtq_u128,   iemAImpl_vpcmpgtq_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpaddb_u128,     iemAImpl_vpaddb_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpaddw_u128,     iemAImpl_vpaddw_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpaddd_u128,     iemAImpl_vpaddd_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpaddq_u128,     iemAImpl_vpaddq_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpsubb_u128,     iemAImpl_vpsubb_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpsubw_u128,     iemAImpl_vpsubw_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpsubd_u128,     iemAImpl_vpsubd_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpsubq_u128,     iemAImpl_vpsubq_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpminub_u128,    iemAImpl_vpminub_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpminuw_u128,    iemAImpl_vpminuw_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpminud_u128,    iemAImpl_vpminud_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpminsb_u128,    iemAImpl_vpminsb_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpminsw_u128,    iemAImpl_vpminsw_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpminsd_u128,    iemAImpl_vpminsd_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpmaxub_u128,    iemAImpl_vpmaxub_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpmaxuw_u128,    iemAImpl_vpmaxuw_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpmaxud_u128,    iemAImpl_vpmaxud_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpmaxsb_u128,    iemAImpl_vpmaxsb_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpmaxsw_u128,    iemAImpl_vpmaxsw_u128_fallback;
FNIEMAIMPLMEDIAF3U128    iemAImpl_vpmaxsd_u128,    iemAImpl_vpmaxsd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpacksswb_u128,  iemAImpl_vpacksswb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpackssdw_u128,  iemAImpl_vpackssdw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpackuswb_u128,  iemAImpl_vpackuswb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpackusdw_u128,  iemAImpl_vpackusdw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmullw_u128,    iemAImpl_vpmullw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmulld_u128,    iemAImpl_vpmulld_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmulhw_u128,    iemAImpl_vpmulhw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpmulhuw_u128,   iemAImpl_vpmulhuw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpavgb_u128,     iemAImpl_vpavgb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpavgw_u128,     iemAImpl_vpavgw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsignb_u128,    iemAImpl_vpsignb_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsignw_u128,    iemAImpl_vpsignw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vpsignd_u128,    iemAImpl_vpsignd_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vphaddw_u128,    iemAImpl_vphaddw_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U128 iemAImpl_vphaddd_u128,    iemAImpl_vphaddd_u128_fallback;

FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_vpabsb_u128,     iemAImpl_vpabsb_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_vpabsw_u128,     iemAImpl_vpabsd_u128_fallback;
FNIEMAIMPLMEDIAOPTF2U128 iemAImpl_vpabsd_u128,     iemAImpl_vpabsw_u128_fallback;

FNIEMAIMPLMEDIAF3U256    iemAImpl_vpshufb_u256,    iemAImpl_vpshufb_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpand_u256,      iemAImpl_vpand_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpandn_u256,     iemAImpl_vpandn_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpor_u256,       iemAImpl_vpor_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpxor_u256,      iemAImpl_vpxor_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpcmpeqb_u256,   iemAImpl_vpcmpeqb_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpcmpeqw_u256,   iemAImpl_vpcmpeqw_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpcmpeqd_u256,   iemAImpl_vpcmpeqd_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpcmpeqq_u256,   iemAImpl_vpcmpeqq_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpcmpgtb_u256,   iemAImpl_vpcmpgtb_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpcmpgtw_u256,   iemAImpl_vpcmpgtw_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpcmpgtd_u256,   iemAImpl_vpcmpgtd_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpcmpgtq_u256,   iemAImpl_vpcmpgtq_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpaddb_u256,     iemAImpl_vpaddb_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpaddw_u256,     iemAImpl_vpaddw_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpaddd_u256,     iemAImpl_vpaddd_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpaddq_u256,     iemAImpl_vpaddq_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpsubb_u256,     iemAImpl_vpsubb_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpsubw_u256,     iemAImpl_vpsubw_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpsubd_u256,     iemAImpl_vpsubd_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpsubq_u256,     iemAImpl_vpsubq_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpminub_u256,    iemAImpl_vpminub_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpminuw_u256,    iemAImpl_vpminuw_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpminud_u256,    iemAImpl_vpminud_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpminsb_u256,    iemAImpl_vpminsb_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpminsw_u256,    iemAImpl_vpminsw_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpminsd_u256,    iemAImpl_vpminsd_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpmaxub_u256,    iemAImpl_vpmaxub_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpmaxuw_u256,    iemAImpl_vpmaxuw_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpmaxud_u256,    iemAImpl_vpmaxud_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpmaxsb_u256,    iemAImpl_vpmaxsb_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpmaxsw_u256,    iemAImpl_vpmaxsw_u256_fallback;
FNIEMAIMPLMEDIAF3U256    iemAImpl_vpmaxsd_u256,    iemAImpl_vpmaxsd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpacksswb_u256,  iemAImpl_vpacksswb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpackssdw_u256,  iemAImpl_vpackssdw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpackuswb_u256,  iemAImpl_vpackuswb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpackusdw_u256,  iemAImpl_vpackusdw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmullw_u256,    iemAImpl_vpmullw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmulld_u256,    iemAImpl_vpmulld_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmulhw_u256,    iemAImpl_vpmulhw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpmulhuw_u256,   iemAImpl_vpmulhuw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpavgb_u256,     iemAImpl_vpavgb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpavgw_u256,     iemAImpl_vpavgw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsignb_u256,    iemAImpl_vpsignb_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsignw_u256,    iemAImpl_vpsignw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vpsignd_u256,    iemAImpl_vpsignd_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vphaddw_u256,    iemAImpl_vphaddw_u256_fallback;
FNIEMAIMPLMEDIAOPTF3U256 iemAImpl_vphaddd_u256,    iemAImpl_vphaddd_u256_fallback;

FNIEMAIMPLMEDIAOPTF2U256 iemAImpl_vpabsb_u256,     iemAImpl_vpabsb_u256_fallback;
FNIEMAIMPLMEDIAOPTF2U256 iemAImpl_vpabsw_u256,     iemAImpl_vpabsw_u256_fallback;
FNIEMAIMPLMEDIAOPTF2U256 iemAImpl_vpabsd_u256,     iemAImpl_vpabsd_u256_fallback;
/** @} */

/** @name Media (SSE/MMX/AVX) operations: lowhalf1 + lowhalf1 -> full1.
 * @{ */
FNIEMAIMPLMEDIAOPTF2U64   iemAImpl_punpcklbw_u64,  iemAImpl_punpcklwd_u64,  iemAImpl_punpckldq_u64;
FNIEMAIMPLMEDIAOPTF2U128  iemAImpl_punpcklbw_u128, iemAImpl_punpcklwd_u128, iemAImpl_punpckldq_u128, iemAImpl_punpcklqdq_u128;
FNIEMAIMPLMEDIAOPTF3U128  iemAImpl_vpunpcklbw_u128,  iemAImpl_vpunpcklbw_u128_fallback,
                          iemAImpl_vpunpcklwd_u128,  iemAImpl_vpunpcklwd_u128_fallback,
                          iemAImpl_vpunpckldq_u128,  iemAImpl_vpunpckldq_u128_fallback,
                          iemAImpl_vpunpcklqdq_u128, iemAImpl_vpunpcklqdq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256  iemAImpl_vpunpcklbw_u256,  iemAImpl_vpunpcklbw_u256_fallback,
                          iemAImpl_vpunpcklwd_u256,  iemAImpl_vpunpcklwd_u256_fallback,
                          iemAImpl_vpunpckldq_u256,  iemAImpl_vpunpckldq_u256_fallback,
                          iemAImpl_vpunpcklqdq_u256, iemAImpl_vpunpcklqdq_u256_fallback;
/** @} */

/** @name Media (SSE/MMX/AVX) operations: hihalf1 + hihalf2 -> full1.
 * @{ */
FNIEMAIMPLMEDIAOPTF2U64   iemAImpl_punpckhbw_u64,  iemAImpl_punpckhwd_u64,  iemAImpl_punpckhdq_u64;
FNIEMAIMPLMEDIAOPTF2U128  iemAImpl_punpckhbw_u128, iemAImpl_punpckhwd_u128, iemAImpl_punpckhdq_u128, iemAImpl_punpckhqdq_u128;
FNIEMAIMPLMEDIAOPTF3U128  iemAImpl_vpunpckhbw_u128,  iemAImpl_vpunpckhbw_u128_fallback,
                          iemAImpl_vpunpckhwd_u128,  iemAImpl_vpunpckhwd_u128_fallback,
                          iemAImpl_vpunpckhdq_u128,  iemAImpl_vpunpckhdq_u128_fallback,
                          iemAImpl_vpunpckhqdq_u128, iemAImpl_vpunpckhqdq_u128_fallback;
FNIEMAIMPLMEDIAOPTF3U256  iemAImpl_vpunpckhbw_u256,  iemAImpl_vpunpckhbw_u256_fallback,
                          iemAImpl_vpunpckhwd_u256,  iemAImpl_vpunpckhwd_u256_fallback,
                          iemAImpl_vpunpckhdq_u256,  iemAImpl_vpunpckhdq_u256_fallback,
                          iemAImpl_vpunpckhqdq_u256, iemAImpl_vpunpckhqdq_u256_fallback;
/** @} */

/** @name Media (SSE/MMX/AVX) operation: Packed Shuffle Stuff (evil)
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHUFU128,(PRTUINT128U puDst, PCRTUINT128U puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAPSHUFU128 *PFNIEMAIMPLMEDIAPSHUFU128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHUFU256,(PRTUINT256U puDst, PCRTUINT256U puSrc, uint8_t bEvil));
typedef FNIEMAIMPLMEDIAPSHUFU256 *PFNIEMAIMPLMEDIAPSHUFU256;
IEM_DECL_IMPL_DEF(void, iemAImpl_pshufw_u64,(uint64_t *puDst, uint64_t const *puSrc, uint8_t bEvil));
FNIEMAIMPLMEDIAPSHUFU128 iemAImpl_pshufhw_u128, iemAImpl_pshuflw_u128, iemAImpl_pshufd_u128;
#ifndef IEM_WITHOUT_ASSEMBLY
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpshufhw_u256, iemAImpl_vpshuflw_u256, iemAImpl_vpshufd_u256;
#endif
FNIEMAIMPLMEDIAPSHUFU256 iemAImpl_vpshufhw_u256_fallback, iemAImpl_vpshuflw_u256_fallback, iemAImpl_vpshufd_u256_fallback;
/** @} */

/** @name Media (SSE/MMX/AVX) operation: Shift Immediate Stuff (evil)
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHIFTU64,(uint64_t *puDst, uint8_t bShift));
typedef FNIEMAIMPLMEDIAPSHIFTU64 *PFNIEMAIMPLMEDIAPSHIFTU64;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHIFTU128,(PRTUINT128U puDst, uint8_t bShift));
typedef FNIEMAIMPLMEDIAPSHIFTU128 *PFNIEMAIMPLMEDIAPSHIFTU128;
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLMEDIAPSHIFTU256,(PRTUINT256U puDst, uint8_t bShift));
typedef FNIEMAIMPLMEDIAPSHIFTU256 *PFNIEMAIMPLMEDIAPSHIFTU256;
FNIEMAIMPLMEDIAPSHIFTU64  iemAImpl_psllw_imm_u64,  iemAImpl_pslld_imm_u64,  iemAImpl_psllq_imm_u64;
FNIEMAIMPLMEDIAPSHIFTU64  iemAImpl_psrlw_imm_u64,  iemAImpl_psrld_imm_u64,  iemAImpl_psrlq_imm_u64;
FNIEMAIMPLMEDIAPSHIFTU64  iemAImpl_psraw_imm_u64,  iemAImpl_psrad_imm_u64;
FNIEMAIMPLMEDIAPSHIFTU128 iemAImpl_psllw_imm_u128, iemAImpl_pslld_imm_u128, iemAImpl_psllq_imm_u128;
FNIEMAIMPLMEDIAPSHIFTU128 iemAImpl_psrlw_imm_u128, iemAImpl_psrld_imm_u128, iemAImpl_psrlq_imm_u128;
FNIEMAIMPLMEDIAPSHIFTU128 iemAImpl_psraw_imm_u128, iemAImpl_psrad_imm_u128;
FNIEMAIMPLMEDIAPSHIFTU128 iemAImpl_pslldq_imm_u128, iemAImpl_psrldq_imm_u128;
/** @} */

/** @name Media (SSE/MMX/AVX) operation: Move Byte Mask
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_pmovmskb_u64,(uint64_t *pu64Dst, uint64_t const *puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_pmovmskb_u128,(uint64_t *pu64Dst, PCRTUINT128U puSrc));
#ifndef IEM_WITHOUT_ASSEMBLY
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovmskb_u256,(uint64_t *pu64Dst, PCRTUINT256U puSrc));
#endif
IEM_DECL_IMPL_DEF(void, iemAImpl_vpmovmskb_u256_fallback,(uint64_t *pu64Dst, PCRTUINT256U puSrc));
/** @} */

/** @name Media (SSE/MMX/AVX) operation: Sort this later
 * @{ */
IEM_DECL_IMPL_DEF(void, iemAImpl_movsldup,(PRTUINT128U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_movshdup,(PRTUINT128U puDst, PCRTUINT128U puSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_movddup,(PRTUINT128U puDst, uint64_t uSrc));

IEM_DECL_IMPL_DEF(void, iemAImpl_vmovsldup_256_rr,(PX86XSAVEAREA pXState, uint8_t iYRegDst, uint8_t iYRegSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovsldup_256_rm,(PX86XSAVEAREA pXState, uint8_t iYRegDst, PCRTUINT256U pSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovshdup_256_rr,(PX86XSAVEAREA pXState, uint8_t iYRegDst, uint8_t iYRegSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovshdup_256_rm,(PX86XSAVEAREA pXState, uint8_t iYRegDst, PCRTUINT256U pSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovddup_256_rr,(PX86XSAVEAREA pXState, uint8_t iYRegDst, uint8_t iYRegSrc));
IEM_DECL_IMPL_DEF(void, iemAImpl_vmovddup_256_rm,(PX86XSAVEAREA pXState, uint8_t iYRegDst, PCRTUINT256U pSrc));

/** @} */

/** @name Media Odds and Ends
 * @{ */
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLCR32U8,(uint32_t *puDst, uint8_t uSrc));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLCR32U16,(uint32_t *puDst, uint16_t uSrc));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLCR32U32,(uint32_t *puDst, uint32_t uSrc));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLCR32U64,(uint32_t *puDst, uint64_t uSrc));
FNIEMAIMPLCR32U8  iemAImpl_crc32_u8,  iemAImpl_crc32_u8_fallback;
FNIEMAIMPLCR32U16 iemAImpl_crc32_u16, iemAImpl_crc32_u16_fallback;
FNIEMAIMPLCR32U32 iemAImpl_crc32_u32, iemAImpl_crc32_u32_fallback;
FNIEMAIMPLCR32U64 iemAImpl_crc32_u64, iemAImpl_crc32_u64_fallback;

typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLF2EFL128,(PCRTUINT128U puSrc1, PCRTUINT128U puSrc2, uint32_t *pEFlags));
typedef IEM_DECL_IMPL_TYPE(void, FNIEMAIMPLF2EFL256,(PCRTUINT256U puSrc1, PCRTUINT256U puSrc2, uint32_t *pEFlags));
FNIEMAIMPLF2EFL128 iemAImpl_ptest_u128;
FNIEMAIMPLF2EFL256 iemAImpl_vptest_u256, iemAImpl_vptest_u256_fallback;
/** @} */


/** @name Function tables.
 * @{
 */

/**
 * Function table for a binary operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPBINSIZES
{
    PFNIEMAIMPLBINU8  pfnNormalU8,    pfnLockedU8;
    PFNIEMAIMPLBINU16 pfnNormalU16,   pfnLockedU16;
    PFNIEMAIMPLBINU32 pfnNormalU32,   pfnLockedU32;
    PFNIEMAIMPLBINU64 pfnNormalU64,   pfnLockedU64;
} IEMOPBINSIZES;
/** Pointer to a binary operator function table. */
typedef IEMOPBINSIZES const *PCIEMOPBINSIZES;


/**
 * Function table for a unary operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPUNARYSIZES
{
    PFNIEMAIMPLUNARYU8  pfnNormalU8,    pfnLockedU8;
    PFNIEMAIMPLUNARYU16 pfnNormalU16,   pfnLockedU16;
    PFNIEMAIMPLUNARYU32 pfnNormalU32,   pfnLockedU32;
    PFNIEMAIMPLUNARYU64 pfnNormalU64,   pfnLockedU64;
} IEMOPUNARYSIZES;
/** Pointer to a unary operator function table. */
typedef IEMOPUNARYSIZES const *PCIEMOPUNARYSIZES;


/**
 * Function table for a shift operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPSHIFTSIZES
{
    PFNIEMAIMPLSHIFTU8  pfnNormalU8;
    PFNIEMAIMPLSHIFTU16 pfnNormalU16;
    PFNIEMAIMPLSHIFTU32 pfnNormalU32;
    PFNIEMAIMPLSHIFTU64 pfnNormalU64;
} IEMOPSHIFTSIZES;
/** Pointer to a shift operator function table. */
typedef IEMOPSHIFTSIZES const *PCIEMOPSHIFTSIZES;


/**
 * Function table for a multiplication or division operation.
 */
typedef struct IEMOPMULDIVSIZES
{
    PFNIEMAIMPLMULDIVU8  pfnU8;
    PFNIEMAIMPLMULDIVU16 pfnU16;
    PFNIEMAIMPLMULDIVU32 pfnU32;
    PFNIEMAIMPLMULDIVU64 pfnU64;
} IEMOPMULDIVSIZES;
/** Pointer to a multiplication or division operation function table. */
typedef IEMOPMULDIVSIZES const *PCIEMOPMULDIVSIZES;


/**
 * Function table for a double precision shift operator providing implementation
 * based on operand size.
 */
typedef struct IEMOPSHIFTDBLSIZES
{
    PFNIEMAIMPLSHIFTDBLU16 pfnNormalU16;
    PFNIEMAIMPLSHIFTDBLU32 pfnNormalU32;
    PFNIEMAIMPLSHIFTDBLU64 pfnNormalU64;
} IEMOPSHIFTDBLSIZES;
/** Pointer to a double precision shift function table. */
typedef IEMOPSHIFTDBLSIZES const *PCIEMOPSHIFTDBLSIZES;


/**
 * Function table for media instruction taking two full sized media source
 * registers and one full sized destination register (AVX).
 */
typedef struct IEMOPMEDIAF3
{
    PFNIEMAIMPLMEDIAF3U128 pfnU128;
    PFNIEMAIMPLMEDIAF3U256 pfnU256;
} IEMOPMEDIAF3;
/** Pointer to a media operation function table for 3 full sized ops (AVX). */
typedef IEMOPMEDIAF3 const *PCIEMOPMEDIAF3;

/** @def IEMOPMEDIAF3_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAF3_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAF3 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAF3 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAF3_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAF3 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAF3_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAF3_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAF3_INIT_VARS_EX */
#define IEMOPMEDIAF3_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAF3_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                              RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))

/**
 * Function table for media instruction taking two full sized media source
 * registers and one full sized destination register, but no additional state
 * (AVX).
 */
typedef struct IEMOPMEDIAOPTF3
{
    PFNIEMAIMPLMEDIAOPTF3U128 pfnU128;
    PFNIEMAIMPLMEDIAOPTF3U256 pfnU256;
} IEMOPMEDIAOPTF3;
/** Pointer to a media operation function table for 3 full sized ops (AVX). */
typedef IEMOPMEDIAOPTF3 const *PCIEMOPMEDIAOPTF3;

/** @def IEMOPMEDIAOPTF3_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAOPTF3_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF3 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAOPTF3 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAOPTF3_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF3 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAOPTF3_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAOPTF3_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAOPTF3_INIT_VARS_EX */
#define IEMOPMEDIAOPTF3_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAOPTF3_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                                 RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))

/**
 * Function table for media instruction taking one full sized media source
 * registers and one full sized destination register, but no additional state
 * (AVX).
 */
typedef struct IEMOPMEDIAOPTF2
{
    PFNIEMAIMPLMEDIAOPTF2U128 pfnU128;
    PFNIEMAIMPLMEDIAOPTF2U256 pfnU256;
} IEMOPMEDIAOPTF2;
/** Pointer to a media operation function table for 2 full sized ops (AVX). */
typedef IEMOPMEDIAOPTF2 const *PCIEMOPMEDIAOPTF2;

/** @def IEMOPMEDIAOPTF2_INIT_VARS_EX
 * Declares a s_Host (x86 & amd64 only) and a s_Fallback variable with the
 * given functions as initializers.  For use in AVX functions where a pair of
 * functions are only used once and the function table need not be public. */
#ifndef TST_IEM_CHECK_MC
# if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && !defined(IEM_WITHOUT_ASSEMBLY)
#  define IEMOPMEDIAOPTF2_INIT_VARS_EX(a_pfnHostU128, a_pfnHostU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF2 const s_Host     = { a_pfnHostU128,     a_pfnHostU256 }; \
    static IEMOPMEDIAOPTF2 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# else
#  define IEMOPMEDIAOPTF2_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) \
    static IEMOPMEDIAOPTF2 const s_Fallback = { a_pfnFallbackU128, a_pfnFallbackU256 }
# endif
#else
# define IEMOPMEDIAOPTF2_INIT_VARS_EX(a_pfnU128, a_pfnU256, a_pfnFallbackU128, a_pfnFallbackU256) (void)0
#endif
/** @def IEMOPMEDIAOPTF2_INIT_VARS
 * Generate AVX function tables for the @a a_InstrNm instruction.
 * @sa IEMOPMEDIAOPTF2_INIT_VARS_EX */
#define IEMOPMEDIAOPTF2_INIT_VARS(a_InstrNm) \
    IEMOPMEDIAOPTF2_INIT_VARS_EX(RT_CONCAT3(iemAImpl_,a_InstrNm,_u128),           RT_CONCAT3(iemAImpl_,a_InstrNm,_u256),\
                                 RT_CONCAT3(iemAImpl_,a_InstrNm,_u128_fallback),  RT_CONCAT3(iemAImpl_,a_InstrNm,_u256_fallback))
/** @} */


/** @name C instruction implementations for anything slightly complicated.
 * @{ */

/**
 * For typedef'ing or declaring a C instruction implementation function taking
 * no extra arguments.
 *
 * @param   a_Name              The name of the type.
 */
# define IEM_CIMPL_DECL_TYPE_0(a_Name) \
    IEM_DECL_IMPL_TYPE(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr))
/**
 * For defining a C instruction implementation function taking no extra
 * arguments.
 *
 * @param   a_Name              The name of the function
 */
# define IEM_CIMPL_DEF_0(a_Name) \
    IEM_DECL_IMPL_DEF(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr))
/**
 * Prototype version of IEM_CIMPL_DEF_0.
 */
# define IEM_CIMPL_PROTO_0(a_Name) \
    IEM_DECL_IMPL_PROTO(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr))
/**
 * For calling a C instruction implementation function taking no extra
 * arguments.
 *
 * This special call macro adds default arguments to the call and allow us to
 * change these later.
 *
 * @param   a_fn                The name of the function.
 */
# define IEM_CIMPL_CALL_0(a_fn)            a_fn(pVCpu, cbInstr)

/**
 * For typedef'ing or declaring a C instruction implementation function taking
 * one extra argument.
 *
 * @param   a_Name              The name of the type.
 * @param   a_Type0             The argument type.
 * @param   a_Arg0              The argument name.
 */
# define IEM_CIMPL_DECL_TYPE_1(a_Name, a_Type0, a_Arg0) \
    IEM_DECL_IMPL_TYPE(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0))
/**
 * For defining a C instruction implementation function taking one extra
 * argument.
 *
 * @param   a_Name              The name of the function
 * @param   a_Type0             The argument type.
 * @param   a_Arg0              The argument name.
 */
# define IEM_CIMPL_DEF_1(a_Name, a_Type0, a_Arg0) \
    IEM_DECL_IMPL_DEF(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0))
/**
 * Prototype version of IEM_CIMPL_DEF_1.
 */
# define IEM_CIMPL_PROTO_1(a_Name, a_Type0, a_Arg0) \
    IEM_DECL_IMPL_PROTO(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0))
/**
 * For calling a C instruction implementation function taking one extra
 * argument.
 *
 * This special call macro adds default arguments to the call and allow us to
 * change these later.
 *
 * @param   a_fn                The name of the function.
 * @param   a0                  The name of the 1st argument.
 */
# define IEM_CIMPL_CALL_1(a_fn, a0)        a_fn(pVCpu, cbInstr, (a0))

/**
 * For typedef'ing or declaring a C instruction implementation function taking
 * two extra arguments.
 *
 * @param   a_Name              The name of the type.
 * @param   a_Type0             The type of the 1st argument
 * @param   a_Arg0              The name of the 1st argument.
 * @param   a_Type1             The type of the 2nd argument.
 * @param   a_Arg1              The name of the 2nd argument.
 */
# define IEM_CIMPL_DECL_TYPE_2(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1) \
    IEM_DECL_IMPL_TYPE(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1))
/**
 * For defining a C instruction implementation function taking two extra
 * arguments.
 *
 * @param   a_Name              The name of the function.
 * @param   a_Type0             The type of the 1st argument
 * @param   a_Arg0              The name of the 1st argument.
 * @param   a_Type1             The type of the 2nd argument.
 * @param   a_Arg1              The name of the 2nd argument.
 */
# define IEM_CIMPL_DEF_2(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1) \
    IEM_DECL_IMPL_DEF(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1))
/**
 * Prototype version of IEM_CIMPL_DEF_2.
 */
# define IEM_CIMPL_PROTO_2(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1) \
    IEM_DECL_IMPL_PROTO(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1))
/**
 * For calling a C instruction implementation function taking two extra
 * arguments.
 *
 * This special call macro adds default arguments to the call and allow us to
 * change these later.
 *
 * @param   a_fn                The name of the function.
 * @param   a0                  The name of the 1st argument.
 * @param   a1                  The name of the 2nd argument.
 */
# define IEM_CIMPL_CALL_2(a_fn, a0, a1)    a_fn(pVCpu, cbInstr, (a0), (a1))

/**
 * For typedef'ing or declaring a C instruction implementation function taking
 * three extra arguments.
 *
 * @param   a_Name              The name of the type.
 * @param   a_Type0             The type of the 1st argument
 * @param   a_Arg0              The name of the 1st argument.
 * @param   a_Type1             The type of the 2nd argument.
 * @param   a_Arg1              The name of the 2nd argument.
 * @param   a_Type2             The type of the 3rd argument.
 * @param   a_Arg2              The name of the 3rd argument.
 */
# define IEM_CIMPL_DECL_TYPE_3(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2) \
    IEM_DECL_IMPL_TYPE(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1, a_Type2 a_Arg2))
/**
 * For defining a C instruction implementation function taking three extra
 * arguments.
 *
 * @param   a_Name              The name of the function.
 * @param   a_Type0             The type of the 1st argument
 * @param   a_Arg0              The name of the 1st argument.
 * @param   a_Type1             The type of the 2nd argument.
 * @param   a_Arg1              The name of the 2nd argument.
 * @param   a_Type2             The type of the 3rd argument.
 * @param   a_Arg2              The name of the 3rd argument.
 */
# define IEM_CIMPL_DEF_3(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2) \
    IEM_DECL_IMPL_DEF(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1, a_Type2 a_Arg2))
/**
 * Prototype version of IEM_CIMPL_DEF_3.
 */
# define IEM_CIMPL_PROTO_3(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2) \
    IEM_DECL_IMPL_PROTO(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1, a_Type2 a_Arg2))
/**
 * For calling a C instruction implementation function taking three extra
 * arguments.
 *
 * This special call macro adds default arguments to the call and allow us to
 * change these later.
 *
 * @param   a_fn                The name of the function.
 * @param   a0                  The name of the 1st argument.
 * @param   a1                  The name of the 2nd argument.
 * @param   a2                  The name of the 3rd argument.
 */
# define IEM_CIMPL_CALL_3(a_fn, a0, a1, a2) a_fn(pVCpu, cbInstr, (a0), (a1), (a2))


/**
 * For typedef'ing or declaring a C instruction implementation function taking
 * four extra arguments.
 *
 * @param   a_Name              The name of the type.
 * @param   a_Type0             The type of the 1st argument
 * @param   a_Arg0              The name of the 1st argument.
 * @param   a_Type1             The type of the 2nd argument.
 * @param   a_Arg1              The name of the 2nd argument.
 * @param   a_Type2             The type of the 3rd argument.
 * @param   a_Arg2              The name of the 3rd argument.
 * @param   a_Type3             The type of the 4th argument.
 * @param   a_Arg3              The name of the 4th argument.
 */
# define IEM_CIMPL_DECL_TYPE_4(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2, a_Type3, a_Arg3) \
    IEM_DECL_IMPL_TYPE(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1, a_Type2 a_Arg2, a_Type3 a_Arg3))
/**
 * For defining a C instruction implementation function taking four extra
 * arguments.
 *
 * @param   a_Name              The name of the function.
 * @param   a_Type0             The type of the 1st argument
 * @param   a_Arg0              The name of the 1st argument.
 * @param   a_Type1             The type of the 2nd argument.
 * @param   a_Arg1              The name of the 2nd argument.
 * @param   a_Type2             The type of the 3rd argument.
 * @param   a_Arg2              The name of the 3rd argument.
 * @param   a_Type3             The type of the 4th argument.
 * @param   a_Arg3              The name of the 4th argument.
 */
# define IEM_CIMPL_DEF_4(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2, a_Type3, a_Arg3) \
    IEM_DECL_IMPL_DEF(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1, \
                                             a_Type2 a_Arg2, a_Type3 a_Arg3))
/**
 * Prototype version of IEM_CIMPL_DEF_4.
 */
# define IEM_CIMPL_PROTO_4(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2, a_Type3, a_Arg3) \
    IEM_DECL_IMPL_PROTO(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1, \
                                               a_Type2 a_Arg2, a_Type3 a_Arg3))
/**
 * For calling a C instruction implementation function taking four extra
 * arguments.
 *
 * This special call macro adds default arguments to the call and allow us to
 * change these later.
 *
 * @param   a_fn                The name of the function.
 * @param   a0                  The name of the 1st argument.
 * @param   a1                  The name of the 2nd argument.
 * @param   a2                  The name of the 3rd argument.
 * @param   a3                  The name of the 4th argument.
 */
# define IEM_CIMPL_CALL_4(a_fn, a0, a1, a2, a3) a_fn(pVCpu, cbInstr, (a0), (a1), (a2), (a3))


/**
 * For typedef'ing or declaring a C instruction implementation function taking
 * five extra arguments.
 *
 * @param   a_Name              The name of the type.
 * @param   a_Type0             The type of the 1st argument
 * @param   a_Arg0              The name of the 1st argument.
 * @param   a_Type1             The type of the 2nd argument.
 * @param   a_Arg1              The name of the 2nd argument.
 * @param   a_Type2             The type of the 3rd argument.
 * @param   a_Arg2              The name of the 3rd argument.
 * @param   a_Type3             The type of the 4th argument.
 * @param   a_Arg3              The name of the 4th argument.
 * @param   a_Type4             The type of the 5th argument.
 * @param   a_Arg4              The name of the 5th argument.
 */
# define IEM_CIMPL_DECL_TYPE_5(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2, a_Type3, a_Arg3, a_Type4, a_Arg4) \
    IEM_DECL_IMPL_TYPE(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, \
                                               a_Type0 a_Arg0, a_Type1 a_Arg1, a_Type2 a_Arg2, \
                                               a_Type3 a_Arg3, a_Type4 a_Arg4))
/**
 * For defining a C instruction implementation function taking five extra
 * arguments.
 *
 * @param   a_Name              The name of the function.
 * @param   a_Type0             The type of the 1st argument
 * @param   a_Arg0              The name of the 1st argument.
 * @param   a_Type1             The type of the 2nd argument.
 * @param   a_Arg1              The name of the 2nd argument.
 * @param   a_Type2             The type of the 3rd argument.
 * @param   a_Arg2              The name of the 3rd argument.
 * @param   a_Type3             The type of the 4th argument.
 * @param   a_Arg3              The name of the 4th argument.
 * @param   a_Type4             The type of the 5th argument.
 * @param   a_Arg4              The name of the 5th argument.
 */
# define IEM_CIMPL_DEF_5(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2, a_Type3, a_Arg3, a_Type4, a_Arg4) \
    IEM_DECL_IMPL_DEF(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1, \
                                             a_Type2 a_Arg2, a_Type3 a_Arg3, a_Type4 a_Arg4))
/**
 * Prototype version of IEM_CIMPL_DEF_5.
 */
# define IEM_CIMPL_PROTO_5(a_Name, a_Type0, a_Arg0, a_Type1, a_Arg1, a_Type2, a_Arg2, a_Type3, a_Arg3, a_Type4, a_Arg4) \
    IEM_DECL_IMPL_PROTO(VBOXSTRICTRC, a_Name, (PVMCPUCC pVCpu, uint8_t cbInstr, a_Type0 a_Arg0, a_Type1 a_Arg1, \
                                               a_Type2 a_Arg2, a_Type3 a_Arg3, a_Type4 a_Arg4))
/**
 * For calling a C instruction implementation function taking five extra
 * arguments.
 *
 * This special call macro adds default arguments to the call and allow us to
 * change these later.
 *
 * @param   a_fn                The name of the function.
 * @param   a0                  The name of the 1st argument.
 * @param   a1                  The name of the 2nd argument.
 * @param   a2                  The name of the 3rd argument.
 * @param   a3                  The name of the 4th argument.
 * @param   a4                  The name of the 5th argument.
 */
# define IEM_CIMPL_CALL_5(a_fn, a0, a1, a2, a3, a4) a_fn(pVCpu, cbInstr, (a0), (a1), (a2), (a3), (a4))

/** @}  */


/** @name Opcode Decoder Function Types.
 * @{ */

/** @typedef PFNIEMOP
 * Pointer to an opcode decoder function.
 */

/** @def FNIEMOP_DEF
 * Define an opcode decoder function.
 *
 * We're using macors for this so that adding and removing parameters as well as
 * tweaking compiler specific attributes becomes easier.  See FNIEMOP_CALL
 *
 * @param   a_Name      The function name.
 */

/** @typedef PFNIEMOPRM
 * Pointer to an opcode decoder function with RM byte.
 */

/** @def FNIEMOPRM_DEF
 * Define an opcode decoder function with RM byte.
 *
 * We're using macors for this so that adding and removing parameters as well as
 * tweaking compiler specific attributes becomes easier.  See FNIEMOP_CALL_1
 *
 * @param   a_Name      The function name.
 */

#if defined(__GNUC__) && defined(RT_ARCH_X86)
typedef VBOXSTRICTRC (__attribute__((__fastcall__)) * PFNIEMOP)(PVMCPUCC pVCpu);
typedef VBOXSTRICTRC (__attribute__((__fastcall__)) * PFNIEMOPRM)(PVMCPUCC pVCpu, uint8_t bRm);
# define FNIEMOP_DEF(a_Name) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PVMCPUCC pVCpu)
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PVMCPUCC pVCpu, a_Type0 a_Name0)
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PVMCPUCC pVCpu, a_Type0 a_Name0, a_Type1 a_Name1)

#elif defined(_MSC_VER) && defined(RT_ARCH_X86)
typedef VBOXSTRICTRC (__fastcall * PFNIEMOP)(PVMCPUCC pVCpu);
typedef VBOXSTRICTRC (__fastcall * PFNIEMOPRM)(PVMCPUCC pVCpu, uint8_t bRm);
# define FNIEMOP_DEF(a_Name) \
    IEM_STATIC /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PVMCPUCC pVCpu) RT_NO_THROW_DEF
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    IEM_STATIC /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PVMCPUCC pVCpu, a_Type0 a_Name0) RT_NO_THROW_DEF
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    IEM_STATIC /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PVMCPUCC pVCpu, a_Type0 a_Name0, a_Type1 a_Name1) RT_NO_THROW_DEF

#elif defined(__GNUC__)
typedef VBOXSTRICTRC (* PFNIEMOP)(PVMCPUCC pVCpu);
typedef VBOXSTRICTRC (* PFNIEMOPRM)(PVMCPUCC pVCpu, uint8_t bRm);
# define FNIEMOP_DEF(a_Name) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PVMCPUCC pVCpu)
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PVMCPUCC pVCpu, a_Type0 a_Name0)
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PVMCPUCC pVCpu, a_Type0 a_Name0, a_Type1 a_Name1)

#else
typedef VBOXSTRICTRC (* PFNIEMOP)(PVMCPUCC pVCpu);
typedef VBOXSTRICTRC (* PFNIEMOPRM)(PVMCPUCC pVCpu, uint8_t bRm);
# define FNIEMOP_DEF(a_Name) \
    IEM_STATIC VBOXSTRICTRC a_Name(PVMCPUCC pVCpu) RT_NO_THROW_DEF
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    IEM_STATIC VBOXSTRICTRC a_Name(PVMCPUCC pVCpu, a_Type0 a_Name0) RT_NO_THROW_DEF
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    IEM_STATIC VBOXSTRICTRC a_Name(PVMCPUCC pVCpu, a_Type0 a_Name0, a_Type1 a_Name1) RT_NO_THROW_DEF

#endif
#define FNIEMOPRM_DEF(a_Name) FNIEMOP_DEF_1(a_Name, uint8_t, bRm)

/**
 * Call an opcode decoder function.
 *
 * We're using macors for this so that adding and removing parameters can be
 * done as we please.  See FNIEMOP_DEF.
 */
#define FNIEMOP_CALL(a_pfn) (a_pfn)(pVCpu)

/**
 * Call a common opcode decoder function taking one extra argument.
 *
 * We're using macors for this so that adding and removing parameters can be
 * done as we please.  See FNIEMOP_DEF_1.
 */
#define FNIEMOP_CALL_1(a_pfn, a0)           (a_pfn)(pVCpu, a0)

/**
 * Call a common opcode decoder function taking one extra argument.
 *
 * We're using macors for this so that adding and removing parameters can be
 * done as we please.  See FNIEMOP_DEF_1.
 */
#define FNIEMOP_CALL_2(a_pfn, a0, a1)       (a_pfn)(pVCpu, a0, a1)
/** @} */


/** @name Misc Helpers
 * @{  */

/** Used to shut up GCC warnings about variables that 'may be used uninitialized'
 * due to GCC lacking knowledge about the value range of a switch. */
#define IEM_NOT_REACHED_DEFAULT_CASE_RET() default: AssertFailedReturn(VERR_IPE_NOT_REACHED_DEFAULT_CASE)

/** Variant of IEM_NOT_REACHED_DEFAULT_CASE_RET that returns a custom value. */
#define IEM_NOT_REACHED_DEFAULT_CASE_RET2(a_RetValue) default: AssertFailedReturn(a_RetValue)

/**
 * Returns IEM_RETURN_ASPECT_NOT_IMPLEMENTED, and in debug builds logs the
 * occation.
 */
#ifdef LOG_ENABLED
# define IEM_RETURN_ASPECT_NOT_IMPLEMENTED() \
    do { \
        /*Log*/ LogAlways(("%s: returning IEM_RETURN_ASPECT_NOT_IMPLEMENTED (line %d)\n", __FUNCTION__, __LINE__)); \
        return VERR_IEM_ASPECT_NOT_IMPLEMENTED; \
    } while (0)
#else
# define IEM_RETURN_ASPECT_NOT_IMPLEMENTED() \
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED
#endif

/**
 * Returns IEM_RETURN_ASPECT_NOT_IMPLEMENTED, and in debug builds logs the
 * occation using the supplied logger statement.
 *
 * @param   a_LoggerArgs    What to log on failure.
 */
#ifdef LOG_ENABLED
# define IEM_RETURN_ASPECT_NOT_IMPLEMENTED_LOG(a_LoggerArgs) \
    do { \
        LogAlways((LOG_FN_FMT ": ", __PRETTY_FUNCTION__)); LogAlways(a_LoggerArgs); \
        /*LogFunc(a_LoggerArgs);*/ \
        return VERR_IEM_ASPECT_NOT_IMPLEMENTED; \
    } while (0)
#else
# define IEM_RETURN_ASPECT_NOT_IMPLEMENTED_LOG(a_LoggerArgs) \
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED
#endif

/**
 * Check if we're currently executing in real or virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The IEM state of the current CPU.
 */
#define IEM_IS_REAL_OR_V86_MODE(a_pVCpu)    (CPUMIsGuestInRealOrV86ModeEx(IEM_GET_CTX(a_pVCpu)))

/**
 * Check if we're currently executing in virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_V86_MODE(a_pVCpu)            (CPUMIsGuestInV86ModeEx(IEM_GET_CTX(a_pVCpu)))

/**
 * Check if we're currently executing in long mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_LONG_MODE(a_pVCpu)           (CPUMIsGuestInLongModeEx(IEM_GET_CTX(a_pVCpu)))

/**
 * Check if we're currently executing in a 64-bit code segment.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_64BIT_CODE(a_pVCpu)          (CPUMIsGuestIn64BitCodeEx(IEM_GET_CTX(a_pVCpu)))

/**
 * Check if we're currently executing in real mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_IS_REAL_MODE(a_pVCpu)           (CPUMIsGuestInRealModeEx(IEM_GET_CTX(a_pVCpu)))

/**
 * Returns a (const) pointer to the CPUMFEATURES for the guest CPU.
 * @returns PCCPUMFEATURES
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_GUEST_CPU_FEATURES(a_pVCpu) (&((a_pVCpu)->CTX_SUFF(pVM)->cpum.ro.GuestFeatures))

/**
 * Returns a (const) pointer to the CPUMFEATURES for the host CPU.
 * @returns PCCPUMFEATURES
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_HOST_CPU_FEATURES(a_pVCpu)  (&g_CpumHostFeatures.s)

/**
 * Evaluates to true if we're presenting an Intel CPU to the guest.
 */
#define IEM_IS_GUEST_CPU_INTEL(a_pVCpu)     ( (a_pVCpu)->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL )

/**
 * Evaluates to true if we're presenting an AMD CPU to the guest.
 */
#define IEM_IS_GUEST_CPU_AMD(a_pVCpu)       ( (a_pVCpu)->iem.s.enmCpuVendor == CPUMCPUVENDOR_AMD || (a_pVCpu)->iem.s.enmCpuVendor == CPUMCPUVENDOR_HYGON )

/**
 * Check if the address is canonical.
 */
#define IEM_IS_CANONICAL(a_u64Addr)         X86_IS_CANONICAL(a_u64Addr)

/** Checks if the ModR/M byte is in register mode or not.  */
#define IEM_IS_MODRM_REG_MODE(a_bRm)        ( ((a_bRm) & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT) )
/** Checks if the ModR/M byte is in memory mode or not.  */
#define IEM_IS_MODRM_MEM_MODE(a_bRm)        ( ((a_bRm) & X86_MODRM_MOD_MASK) != (3 << X86_MODRM_MOD_SHIFT) )

/**
 * Gets the register (reg) part of a ModR/M encoding, with REX.R added in.
 *
 * For use during decoding.
 */
#define IEM_GET_MODRM_REG(a_pVCpu, a_bRm)   ( (((a_bRm) >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | (a_pVCpu)->iem.s.uRexReg )
/**
 * Gets the r/m part of a ModR/M encoding as a register index, with REX.B added in.
 *
 * For use during decoding.
 */
#define IEM_GET_MODRM_RM(a_pVCpu, a_bRm)    ( ((a_bRm) & X86_MODRM_RM_MASK) | (a_pVCpu)->iem.s.uRexB )

/**
 * Gets the register (reg) part of a ModR/M encoding, without REX.R.
 *
 * For use during decoding.
 */
#define IEM_GET_MODRM_REG_8(a_bRm)          ( (((a_bRm) >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) )
/**
 * Gets the r/m part of a ModR/M encoding as a register index, without REX.B.
 *
 * For use during decoding.
 */
#define IEM_GET_MODRM_RM_8(a_bRm)           ( ((a_bRm) & X86_MODRM_RM_MASK) )

/**
 * Gets the effective VEX.VVVV value.
 *
 * The 4th bit is ignored if not 64-bit code.
 * @returns effective V-register value.
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling thread.
 */
#define IEM_GET_EFFECTIVE_VVVV(a_pVCpu) \
    ((a_pVCpu)->iem.s.enmCpuMode == IEMMODE_64BIT ? (a_pVCpu)->iem.s.uVex3rdReg : (a_pVCpu)->iem.s.uVex3rdReg & 7)


#ifdef VBOX_WITH_NESTED_HWVIRT_VMX

/**
 * Check if the guest has entered VMX root operation.
 */
# define IEM_VMX_IS_ROOT_MODE(a_pVCpu)      (CPUMIsGuestInVmxRootMode(IEM_GET_CTX(a_pVCpu)))

/**
 * Check if the guest has entered VMX non-root operation.
 */
# define IEM_VMX_IS_NON_ROOT_MODE(a_pVCpu)  (CPUMIsGuestInVmxNonRootMode(IEM_GET_CTX(a_pVCpu)))

/**
 * Check if the nested-guest has the given Pin-based VM-execution control set.
 */
# define IEM_VMX_IS_PINCTLS_SET(a_pVCpu, a_PinCtl) \
    (CPUMIsGuestVmxPinCtlsSet(IEM_GET_CTX(a_pVCpu), (a_PinCtl)))

/**
 * Check if the nested-guest has the given Processor-based VM-execution control set.
 */
# define IEM_VMX_IS_PROCCTLS_SET(a_pVCpu, a_ProcCtl) \
    (CPUMIsGuestVmxProcCtlsSet(IEM_GET_CTX(a_pVCpu), (a_ProcCtl)))

/**
 * Check if the nested-guest has the given Secondary Processor-based VM-execution
 * control set.
 */
# define IEM_VMX_IS_PROCCTLS2_SET(a_pVCpu, a_ProcCtl2) \
    (CPUMIsGuestVmxProcCtls2Set(IEM_GET_CTX(a_pVCpu), (a_ProcCtl2)))

/** Gets the guest-physical address of the shadows VMCS for the given VCPU. */
# define IEM_VMX_GET_SHADOW_VMCS(a_pVCpu)           ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysShadowVmcs)

/** Whether a shadow VMCS is present for the given VCPU. */
# define IEM_VMX_HAS_SHADOW_VMCS(a_pVCpu)           RT_BOOL(IEM_VMX_GET_SHADOW_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Gets the VMXON region pointer. */
# define IEM_VMX_GET_VMXON_PTR(a_pVCpu)             ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)

/** Gets the guest-physical address of the current VMCS for the given VCPU. */
# define IEM_VMX_GET_CURRENT_VMCS(a_pVCpu)          ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs)

/** Whether a current VMCS is present for the given VCPU. */
# define IEM_VMX_HAS_CURRENT_VMCS(a_pVCpu)          RT_BOOL(IEM_VMX_GET_CURRENT_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Assigns the guest-physical address of the current VMCS for the given VCPU. */
# define IEM_VMX_SET_CURRENT_VMCS(a_pVCpu, a_GCPhysVmcs) \
    do \
    { \
        Assert((a_GCPhysVmcs) != NIL_RTGCPHYS); \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs = (a_GCPhysVmcs); \
    } while (0)

/** Clears any current VMCS for the given VCPU. */
# define IEM_VMX_CLEAR_CURRENT_VMCS(a_pVCpu) \
    do \
    { \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs = NIL_RTGCPHYS; \
    } while (0)

/**
 * Invokes the VMX VM-exit handler for an instruction intercept.
 */
# define IEM_VMX_VMEXIT_INSTR_RET(a_pVCpu, a_uExitReason, a_cbInstr) \
    do { return iemVmxVmexitInstr((a_pVCpu), (a_uExitReason), (a_cbInstr)); } while (0)

/**
 * Invokes the VMX VM-exit handler for an instruction intercept where the
 * instruction provides additional VM-exit information.
 */
# define IEM_VMX_VMEXIT_INSTR_NEEDS_INFO_RET(a_pVCpu, a_uExitReason, a_uInstrId, a_cbInstr) \
    do { return iemVmxVmexitInstrNeedsInfo((a_pVCpu), (a_uExitReason), (a_uInstrId), (a_cbInstr)); } while (0)

/**
 * Invokes the VMX VM-exit handler for a task switch.
 */
# define IEM_VMX_VMEXIT_TASK_SWITCH_RET(a_pVCpu, a_enmTaskSwitch, a_SelNewTss, a_cbInstr) \
    do { return iemVmxVmexitTaskSwitch((a_pVCpu), (a_enmTaskSwitch), (a_SelNewTss), (a_cbInstr)); } while (0)

/**
 * Invokes the VMX VM-exit handler for MWAIT.
 */
# define IEM_VMX_VMEXIT_MWAIT_RET(a_pVCpu, a_fMonitorArmed, a_cbInstr) \
    do { return iemVmxVmexitInstrMwait((a_pVCpu), (a_fMonitorArmed), (a_cbInstr)); } while (0)

/**
 * Invokes the VMX VM-exit handler for EPT faults.
 */
# define IEM_VMX_VMEXIT_EPT_RET(a_pVCpu, a_pPtWalk, a_fAccess, a_fSlatFail, a_cbInstr) \
    do { return iemVmxVmexitEpt(a_pVCpu, a_pPtWalk, a_fAccess, a_fSlatFail, a_cbInstr); } while (0)

/**
 * Invokes the VMX VM-exit handler.
 */
# define IEM_VMX_VMEXIT_TRIPLE_FAULT_RET(a_pVCpu, a_uExitReason, a_uExitQual) \
    do { return iemVmxVmexit((a_pVCpu), (a_uExitReason), (a_uExitQual)); } while (0)

#else
# define IEM_VMX_IS_ROOT_MODE(a_pVCpu)                                          (false)
# define IEM_VMX_IS_NON_ROOT_MODE(a_pVCpu)                                      (false)
# define IEM_VMX_IS_PINCTLS_SET(a_pVCpu, a_cbInstr)                             (false)
# define IEM_VMX_IS_PROCCTLS_SET(a_pVCpu, a_cbInstr)                            (false)
# define IEM_VMX_IS_PROCCTLS2_SET(a_pVCpu, a_cbInstr)                           (false)
# define IEM_VMX_VMEXIT_INSTR_RET(a_pVCpu, a_uExitReason, a_cbInstr)            do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_INSTR_NEEDS_INFO_RET(a_pVCpu, a_uExitReason, a_uInstrId, a_cbInstr)  do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_TASK_SWITCH_RET(a_pVCpu, a_enmTaskSwitch, a_SelNewTss, a_cbInstr)    do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_MWAIT_RET(a_pVCpu, a_fMonitorArmed, a_cbInstr)          do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_EPT_RET(a_pVCpu, a_pPtWalk, a_fAccess, a_fSlatFail, a_cbInstr)       do { return VERR_VMX_IPE_1; } while (0)
# define IEM_VMX_VMEXIT_TRIPLE_FAULT_RET(a_pVCpu, a_uExitReason, a_uExitQual)   do { return VERR_VMX_IPE_1; } while (0)

#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Check if an SVM control/instruction intercept is set.
 */
# define IEM_SVM_IS_CTRL_INTERCEPT_SET(a_pVCpu, a_Intercept) \
    (CPUMIsGuestSvmCtrlInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_Intercept)))

/**
 * Check if an SVM read CRx intercept is set.
 */
# define IEM_SVM_IS_READ_CR_INTERCEPT_SET(a_pVCpu, a_uCr) \
    (CPUMIsGuestSvmReadCRxInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uCr)))

/**
 * Check if an SVM write CRx intercept is set.
 */
# define IEM_SVM_IS_WRITE_CR_INTERCEPT_SET(a_pVCpu, a_uCr) \
    (CPUMIsGuestSvmWriteCRxInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uCr)))

/**
 * Check if an SVM read DRx intercept is set.
 */
# define IEM_SVM_IS_READ_DR_INTERCEPT_SET(a_pVCpu, a_uDr) \
    (CPUMIsGuestSvmReadDRxInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uDr)))

/**
 * Check if an SVM write DRx intercept is set.
 */
# define IEM_SVM_IS_WRITE_DR_INTERCEPT_SET(a_pVCpu, a_uDr) \
    (CPUMIsGuestSvmWriteDRxInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uDr)))

/**
 * Check if an SVM exception intercept is set.
 */
# define IEM_SVM_IS_XCPT_INTERCEPT_SET(a_pVCpu, a_uVector) \
    (CPUMIsGuestSvmXcptInterceptSet(a_pVCpu, IEM_GET_CTX(a_pVCpu), (a_uVector)))

/**
 * Invokes the SVM \#VMEXIT handler for the nested-guest.
 */
# define IEM_SVM_VMEXIT_RET(a_pVCpu, a_uExitCode, a_uExitInfo1, a_uExitInfo2) \
    do { return iemSvmVmexit((a_pVCpu), (a_uExitCode), (a_uExitInfo1), (a_uExitInfo2)); } while (0)

/**
 * Invokes the 'MOV CRx' SVM \#VMEXIT handler after constructing the
 * corresponding decode assist information.
 */
# define IEM_SVM_CRX_VMEXIT_RET(a_pVCpu, a_uExitCode, a_enmAccessCrX, a_iGReg) \
    do \
    { \
        uint64_t uExitInfo1; \
        if (   IEM_GET_GUEST_CPU_FEATURES(a_pVCpu)->fSvmDecodeAssists \
            && (a_enmAccessCrX) == IEMACCESSCRX_MOV_CRX) \
            uExitInfo1 = SVM_EXIT1_MOV_CRX_MASK | ((a_iGReg) & 7); \
        else \
            uExitInfo1 = 0; \
        IEM_SVM_VMEXIT_RET(a_pVCpu, a_uExitCode, uExitInfo1, 0); \
    } while (0)

/** Check and handles SVM nested-guest instruction intercept and updates
 *  NRIP if needed.
 */
# define IEM_SVM_CHECK_INSTR_INTERCEPT(a_pVCpu, a_Intercept, a_uExitCode, a_uExitInfo1, a_uExitInfo2) \
    do \
    { \
        if (IEM_SVM_IS_CTRL_INTERCEPT_SET(a_pVCpu, a_Intercept)) \
        { \
            IEM_SVM_UPDATE_NRIP(a_pVCpu); \
            IEM_SVM_VMEXIT_RET(a_pVCpu, a_uExitCode, a_uExitInfo1, a_uExitInfo2); \
        } \
    } while (0)

/** Checks and handles SVM nested-guest CR0 read intercept. */
# define IEM_SVM_CHECK_READ_CR0_INTERCEPT(a_pVCpu, a_uExitInfo1, a_uExitInfo2) \
    do \
    { \
        if (!IEM_SVM_IS_READ_CR_INTERCEPT_SET(a_pVCpu, 0)) \
        { /* probably likely */ } \
        else \
        { \
            IEM_SVM_UPDATE_NRIP(a_pVCpu); \
            IEM_SVM_VMEXIT_RET(a_pVCpu, SVM_EXIT_READ_CR0, a_uExitInfo1, a_uExitInfo2); \
        } \
    } while (0)

/**
 * Updates the NextRIP (NRI) field in the nested-guest VMCB.
 */
# define IEM_SVM_UPDATE_NRIP(a_pVCpu) \
    do { \
        if (IEM_GET_GUEST_CPU_FEATURES(a_pVCpu)->fSvmNextRipSave) \
            CPUMGuestSvmUpdateNRip(a_pVCpu, IEM_GET_CTX(a_pVCpu), IEM_GET_INSTR_LEN(a_pVCpu)); \
    } while (0)

#else
# define IEM_SVM_IS_CTRL_INTERCEPT_SET(a_pVCpu, a_Intercept)                              (false)
# define IEM_SVM_IS_READ_CR_INTERCEPT_SET(a_pVCpu, a_uCr)                                 (false)
# define IEM_SVM_IS_WRITE_CR_INTERCEPT_SET(a_pVCpu, a_uCr)                                (false)
# define IEM_SVM_IS_READ_DR_INTERCEPT_SET(a_pVCpu, a_uDr)                                 (false)
# define IEM_SVM_IS_WRITE_DR_INTERCEPT_SET(a_pVCpu, a_uDr)                                (false)
# define IEM_SVM_IS_XCPT_INTERCEPT_SET(a_pVCpu, a_uVector)                                (false)
# define IEM_SVM_VMEXIT_RET(a_pVCpu, a_uExitCode, a_uExitInfo1, a_uExitInfo2)             do { return VERR_SVM_IPE_1; } while (0)
# define IEM_SVM_CRX_VMEXIT_RET(a_pVCpu, a_uExitCode, a_enmAccessCrX, a_iGReg)            do { return VERR_SVM_IPE_1; } while (0)
# define IEM_SVM_CHECK_INSTR_INTERCEPT(a_pVCpu, a_Intercept, a_uExitCode, a_uExitInfo1, a_uExitInfo2)   do { } while (0)
# define IEM_SVM_CHECK_READ_CR0_INTERCEPT(a_pVCpu, a_uExitInfo1, a_uExitInfo2)                          do { } while (0)
# define IEM_SVM_UPDATE_NRIP(a_pVCpu)                                                     do { } while (0)

#endif

/** @} */



/**
 * Selector descriptor table entry as fetched by iemMemFetchSelDesc.
 */
typedef union IEMSELDESC
{
    /** The legacy view. */
    X86DESC     Legacy;
    /** The long mode view. */
    X86DESC64   Long;
} IEMSELDESC;
/** Pointer to a selector descriptor table entry. */
typedef IEMSELDESC *PIEMSELDESC;

/** @name  Raising Exceptions.
 * @{ */
VBOXSTRICTRC            iemTaskSwitch(PVMCPUCC pVCpu, IEMTASKSWITCH enmTaskSwitch, uint32_t uNextEip, uint32_t fFlags,
                                      uint16_t uErr, uint64_t uCr2, RTSEL SelTSS, PIEMSELDESC pNewDescTSS) RT_NOEXCEPT;

VBOXSTRICTRC            iemRaiseXcptOrInt(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t u8Vector, uint32_t fFlags,
                                          uint16_t uErr, uint64_t uCr2) RT_NOEXCEPT;
#ifdef IEM_WITH_SETJMP
DECL_NO_RETURN(void)    iemRaiseXcptOrIntJmp(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t u8Vector,
                                             uint32_t fFlags, uint16_t uErr, uint64_t uCr2) RT_NOEXCEPT;
#endif
VBOXSTRICTRC            iemRaiseDivideError(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseDebugException(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseBoundRangeExceeded(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseUndefinedOpcode(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseDeviceNotAvailable(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseTaskSwitchFaultWithErr(PVMCPUCC pVCpu, uint16_t uErr) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseTaskSwitchFaultCurrentTSS(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseTaskSwitchFault0(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseTaskSwitchFaultBySelector(PVMCPUCC pVCpu, uint16_t uSel) RT_NOEXCEPT;
/*VBOXSTRICTRC            iemRaiseSelectorNotPresent(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) RT_NOEXCEPT;*/
VBOXSTRICTRC            iemRaiseSelectorNotPresentWithErr(PVMCPUCC pVCpu, uint16_t uErr) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseSelectorNotPresentBySelector(PVMCPUCC pVCpu, uint16_t uSel) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseStackSelectorNotPresentBySelector(PVMCPUCC pVCpu, uint16_t uSel) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseStackSelectorNotPresentWithErr(PVMCPUCC pVCpu, uint16_t uErr) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseGeneralProtectionFault(PVMCPUCC pVCpu, uint16_t uErr) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseGeneralProtectionFault0(PVMCPUCC pVCpu) RT_NOEXCEPT;
#ifdef IEM_WITH_SETJMP
DECL_NO_RETURN(void)    iemRaiseGeneralProtectionFault0Jmp(PVMCPUCC pVCpu) RT_NOEXCEPT;
#endif
VBOXSTRICTRC            iemRaiseGeneralProtectionFaultBySelector(PVMCPUCC pVCpu, RTSEL Sel) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseNotCanonical(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC            iemRaiseSelectorBounds(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) RT_NOEXCEPT;
#ifdef IEM_WITH_SETJMP
DECL_NO_RETURN(void)    iemRaiseSelectorBoundsJmp(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) RT_NOEXCEPT;
#endif
VBOXSTRICTRC            iemRaiseSelectorBoundsBySelector(PVMCPUCC pVCpu, RTSEL Sel) RT_NOEXCEPT;
#ifdef IEM_WITH_SETJMP
DECL_NO_RETURN(void)    iemRaiseSelectorBoundsBySelectorJmp(PVMCPUCC pVCpu, RTSEL Sel) RT_NOEXCEPT;
#endif
VBOXSTRICTRC            iemRaiseSelectorInvalidAccess(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) RT_NOEXCEPT;
#ifdef IEM_WITH_SETJMP
DECL_NO_RETURN(void)    iemRaiseSelectorInvalidAccessJmp(PVMCPUCC pVCpu, uint32_t iSegReg, uint32_t fAccess) RT_NOEXCEPT;
#endif
VBOXSTRICTRC            iemRaisePageFault(PVMCPUCC pVCpu, RTGCPTR GCPtrWhere, uint32_t fAccess, int rc) RT_NOEXCEPT;
#ifdef IEM_WITH_SETJMP
DECL_NO_RETURN(void)    iemRaisePageFaultJmp(PVMCPUCC pVCpu, RTGCPTR GCPtrWhere, uint32_t fAccess, int rc)  RT_NOEXCEPT;
#endif
VBOXSTRICTRC            iemRaiseMathFault(PVMCPUCC pVCpu);
VBOXSTRICTRC            iemRaiseAlignmentCheckException(PVMCPUCC pVCpu);
#ifdef IEM_WITH_SETJMP
DECL_NO_RETURN(void)    iemRaiseAlignmentCheckExceptionJmp(PVMCPUCC pVCpu)  RT_NOEXCEPT;
#endif

IEM_CIMPL_DEF_0(iemCImplRaiseDivideError);
IEM_CIMPL_DEF_0(iemCImplRaiseInvalidLockPrefix);
IEM_CIMPL_DEF_0(iemCImplRaiseInvalidOpcode);

/**
 * Macro for calling iemCImplRaiseDivideError().
 *
 * This enables us to add/remove arguments and force different levels of
 * inlining as we wish.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_DIVIDE_ERROR()          IEM_MC_DEFER_TO_CIMPL_0(iemCImplRaiseDivideError)

/**
 * Macro for calling iemCImplRaiseInvalidLockPrefix().
 *
 * This enables us to add/remove arguments and force different levels of
 * inlining as we wish.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_INVALID_LOCK_PREFIX()   IEM_MC_DEFER_TO_CIMPL_0(iemCImplRaiseInvalidLockPrefix)

/**
 * Macro for calling iemCImplRaiseInvalidOpcode().
 *
 * This enables us to add/remove arguments and force different levels of
 * inlining as we wish.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_INVALID_OPCODE()        IEM_MC_DEFER_TO_CIMPL_0(iemCImplRaiseInvalidOpcode)
/** @} */

/** @name Register Access.
 * @{ */
VBOXSTRICTRC    iemRegRipRelativeJumpS8(PVMCPUCC pVCpu, int8_t offNextInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemRegRipRelativeJumpS16(PVMCPUCC pVCpu, int16_t offNextInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemRegRipRelativeJumpS32(PVMCPUCC pVCpu, int32_t offNextInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemRegRipJump(PVMCPUCC pVCpu, uint64_t uNewRip) RT_NOEXCEPT;
/** @} */

/** @name FPU access and helpers.
 * @{ */
void            iemFpuPushResult(PVMCPUCC pVCpu, PIEMFPURESULT pResult) RT_NOEXCEPT;
void            iemFpuPushResultWithMemOp(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iEffSeg, RTGCPTR GCPtrEff) RT_NOEXCEPT;
void            iemFpuPushResultTwo(PVMCPUCC pVCpu, PIEMFPURESULTTWO pResult) RT_NOEXCEPT;
void            iemFpuStoreResult(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iStReg) RT_NOEXCEPT;
void            iemFpuStoreResultThenPop(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iStReg) RT_NOEXCEPT;
void            iemFpuStoreResultWithMemOp(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iStReg,
                                           uint8_t iEffSeg, RTGCPTR GCPtrEff) RT_NOEXCEPT;
void            iemFpuStoreResultWithMemOpThenPop(PVMCPUCC pVCpu, PIEMFPURESULT pResult, uint8_t iStReg,
                                                  uint8_t iEffSeg, RTGCPTR GCPtrEff) RT_NOEXCEPT;
void            iemFpuUpdateOpcodeAndIp(PVMCPUCC pVCpu) RT_NOEXCEPT;
void            iemFpuUpdateFSW(PVMCPUCC pVCpu, uint16_t u16FSW) RT_NOEXCEPT;
void            iemFpuUpdateFSWThenPop(PVMCPUCC pVCpu, uint16_t u16FSW) RT_NOEXCEPT;
void            iemFpuUpdateFSWWithMemOp(PVMCPUCC pVCpu, uint16_t u16FSW, uint8_t iEffSeg, RTGCPTR GCPtrEff) RT_NOEXCEPT;
void            iemFpuUpdateFSWThenPopPop(PVMCPUCC pVCpu, uint16_t u16FSW) RT_NOEXCEPT;
void            iemFpuUpdateFSWWithMemOpThenPop(PVMCPUCC pVCpu, uint16_t u16FSW, uint8_t iEffSeg, RTGCPTR GCPtrEff) RT_NOEXCEPT;
void            iemFpuStackUnderflow(PVMCPUCC pVCpu, uint8_t iStReg) RT_NOEXCEPT;
void            iemFpuStackUnderflowWithMemOp(PVMCPUCC pVCpu, uint8_t iStReg, uint8_t iEffSeg, RTGCPTR GCPtrEff) RT_NOEXCEPT;
void            iemFpuStackUnderflowThenPop(PVMCPUCC pVCpu, uint8_t iStReg) RT_NOEXCEPT;
void            iemFpuStackUnderflowWithMemOpThenPop(PVMCPUCC pVCpu, uint8_t iStReg, uint8_t iEffSeg, RTGCPTR GCPtrEff) RT_NOEXCEPT;
void            iemFpuStackUnderflowThenPopPop(PVMCPUCC pVCpu) RT_NOEXCEPT;
void            iemFpuStackPushUnderflow(PVMCPUCC pVCpu) RT_NOEXCEPT;
void            iemFpuStackPushUnderflowTwo(PVMCPUCC pVCpu) RT_NOEXCEPT;
void            iemFpuStackPushOverflow(PVMCPUCC pVCpu) RT_NOEXCEPT;
void            iemFpuStackPushOverflowWithMemOp(PVMCPUCC pVCpu, uint8_t iEffSeg, RTGCPTR GCPtrEff) RT_NOEXCEPT;
/** @} */

/** @name   Memory access.
 * @{ */

/** Report a \#GP instead of \#AC and do not restrict to ring-3 */
#define IEM_MEMMAP_F_ALIGN_GP       RT_BIT_32(16)
/** SSE access that should report a \#GP instead of \#AC, unless MXCSR.MM=1
 *  when it works like normal \#AC. Always used with IEM_MEMMAP_F_ALIGN_GP. */
#define IEM_MEMMAP_F_ALIGN_SSE      RT_BIT_32(17)
/** If \#AC is applicable, raise it. Always used with IEM_MEMMAP_F_ALIGN_GP.
 * Users include FXSAVE & FXRSTOR. */
#define IEM_MEMMAP_F_ALIGN_GP_OR_AC RT_BIT_32(18)

VBOXSTRICTRC    iemMemMap(PVMCPUCC pVCpu, void **ppvMem, size_t cbMem, uint8_t iSegReg, RTGCPTR GCPtrMem,
                          uint32_t fAccess, uint32_t uAlignCtl) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemCommitAndUnmap(PVMCPUCC pVCpu, void *pvMem, uint32_t fAccess) RT_NOEXCEPT;
#ifndef IN_RING3
VBOXSTRICTRC    iemMemCommitAndUnmapPostponeTroubleToR3(PVMCPUCC pVCpu, void *pvMem, uint32_t fAccess) RT_NOEXCEPT;
#endif
void            iemMemRollback(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemApplySegment(PVMCPUCC pVCpu, uint32_t fAccess, uint8_t iSegReg, size_t cbMem, PRTGCPTR pGCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemMarkSelDescAccessed(PVMCPUCC pVCpu, uint16_t uSel) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemPageTranslateAndCheckAccess(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t fAccess, PRTGCPHYS pGCPhysMem) RT_NOEXCEPT;

#ifdef IEM_WITH_CODE_TLB
void            iemOpcodeFetchBytesJmp(PVMCPUCC pVCpu, size_t cbDst, void *pvDst) RT_NOEXCEPT;
#else
VBOXSTRICTRC    iemOpcodeFetchMoreBytes(PVMCPUCC pVCpu, size_t cbMin) RT_NOEXCEPT;
#endif
#ifdef IEM_WITH_SETJMP
uint8_t         iemOpcodeGetNextU8SlowJmp(PVMCPUCC pVCpu) RT_NOEXCEPT;
uint16_t        iemOpcodeGetNextU16SlowJmp(PVMCPUCC pVCpu) RT_NOEXCEPT;
uint32_t        iemOpcodeGetNextU32SlowJmp(PVMCPUCC pVCpu) RT_NOEXCEPT;
uint64_t        iemOpcodeGetNextU64SlowJmp(PVMCPUCC pVCpu) RT_NOEXCEPT;
#else
VBOXSTRICTRC    iemOpcodeGetNextU8Slow(PVMCPUCC pVCpu, uint8_t *pb) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextS8SxU16Slow(PVMCPUCC pVCpu, uint16_t *pu16) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextS8SxU32Slow(PVMCPUCC pVCpu, uint32_t *pu32) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextS8SxU64Slow(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextU16Slow(PVMCPUCC pVCpu, uint16_t *pu16) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextU16ZxU32Slow(PVMCPUCC pVCpu, uint32_t *pu32) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextU16ZxU64Slow(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextU32Slow(PVMCPUCC pVCpu, uint32_t *pu32) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextU32ZxU64Slow(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextS32SxU64Slow(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT;
VBOXSTRICTRC    iemOpcodeGetNextU64Slow(PVMCPUCC pVCpu, uint64_t *pu64) RT_NOEXCEPT;
#endif

VBOXSTRICTRC    iemMemFetchDataU8(PVMCPUCC pVCpu, uint8_t *pu8Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU16(PVMCPUCC pVCpu, uint16_t *pu16Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU32(PVMCPUCC pVCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU32_ZX_U64(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU64(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU64AlignedU128(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataR80(PVMCPUCC pVCpu, PRTFLOAT80U pr80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataD80(PVMCPUCC pVCpu, PRTPBCD80U pd80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU128(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU128AlignedSse(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU256(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataU256AlignedSse(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchDataXdtr(PVMCPUCC pVCpu, uint16_t *pcbLimit, PRTGCPTR pGCPtrBase, uint8_t iSegReg,
                                    RTGCPTR GCPtrMem, IEMMODE enmOpSize) RT_NOEXCEPT;
#ifdef IEM_WITH_SETJMP
uint8_t         iemMemFetchDataU8Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
uint16_t        iemMemFetchDataU16Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
uint32_t        iemMemFetchDataU32Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
uint64_t        iemMemFetchDataU64Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
uint64_t        iemMemFetchDataU64AlignedU128Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
void            iemMemFetchDataR80Jmp(PVMCPUCC pVCpu, PRTFLOAT80U pr80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
void            iemMemFetchDataD80Jmp(PVMCPUCC pVCpu, PRTPBCD80U pd80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
void            iemMemFetchDataU128Jmp(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
void            iemMemFetchDataU128AlignedSseJmp(PVMCPUCC pVCpu, PRTUINT128U pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
void            iemMemFetchDataU256Jmp(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
void            iemMemFetchDataU256AlignedSseJmp(PVMCPUCC pVCpu, PRTUINT256U pu256Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
#endif

VBOXSTRICTRC    iemMemFetchSysU8(PVMCPUCC pVCpu, uint8_t *pu8Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU16(PVMCPUCC pVCpu, uint16_t *pu16Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU32(PVMCPUCC pVCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSysU64(PVMCPUCC pVCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemFetchSelDesc(PVMCPUCC pVCpu, PIEMSELDESC pDesc, uint16_t uSel, uint8_t uXcpt) RT_NOEXCEPT;

VBOXSTRICTRC    iemMemStoreDataU8(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint8_t u8Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU16(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint16_t u16Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU32(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t u32Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU64(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint64_t u64Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU128(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU128AlignedSse(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU256(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataU256AlignedAvx(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStoreDataXdtr(PVMCPUCC pVCpu, uint16_t cbLimit, RTGCPTR GCPtrBase, uint8_t iSegReg, RTGCPTR GCPtrMem) RT_NOEXCEPT;
#ifdef IEM_WITH_SETJMP
void            iemMemStoreDataU8Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint8_t u8Value) RT_NOEXCEPT;
void            iemMemStoreDataU16Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint16_t u16Value) RT_NOEXCEPT;
void            iemMemStoreDataU32Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t u32Value) RT_NOEXCEPT;
void            iemMemStoreDataU64Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint64_t u64Value) RT_NOEXCEPT;
void            iemMemStoreDataU128Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) RT_NOEXCEPT;
void            iemMemStoreDataU128AlignedSseJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, RTUINT128U u128Value) RT_NOEXCEPT;
void            iemMemStoreDataU256Jmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) RT_NOEXCEPT;
void            iemMemStoreDataU256AlignedAvxJmp(PVMCPUCC pVCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, PCRTUINT256U pu256Value) RT_NOEXCEPT;
#endif

VBOXSTRICTRC    iemMemStackPushBeginSpecial(PVMCPUCC pVCpu, size_t cbMem, uint32_t cbAlign,
                                            void **ppvMem, uint64_t *puNewRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushCommitSpecial(PVMCPUCC pVCpu, void *pvMem, uint64_t uNewRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU16(PVMCPUCC pVCpu, uint16_t u16Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU32(PVMCPUCC pVCpu, uint32_t u32Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU64(PVMCPUCC pVCpu, uint64_t u64Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU16Ex(PVMCPUCC pVCpu, uint16_t u16Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU32Ex(PVMCPUCC pVCpu, uint32_t u32Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU64Ex(PVMCPUCC pVCpu, uint64_t u64Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPushU32SReg(PVMCPUCC pVCpu, uint32_t u32Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopBeginSpecial(PVMCPUCC pVCpu, size_t cbMem, uint32_t cbAlign,
                                           void const **ppvMem, uint64_t *puNewRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopContinueSpecial(PVMCPUCC pVCpu, size_t cbMem, void const **ppvMem, uint64_t *puNewRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopDoneSpecial(PVMCPUCC pVCpu, void const *pvMem) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU16(PVMCPUCC pVCpu, uint16_t *pu16Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU32(PVMCPUCC pVCpu, uint32_t *pu32Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU64(PVMCPUCC pVCpu, uint64_t *pu64Value) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU16Ex(PVMCPUCC pVCpu, uint16_t *pu16Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU32Ex(PVMCPUCC pVCpu, uint32_t *pu32Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
VBOXSTRICTRC    iemMemStackPopU64Ex(PVMCPUCC pVCpu, uint64_t *pu64Value, PRTUINT64U pTmpRsp) RT_NOEXCEPT;
/** @} */

/** @name IEMAllCImpl.cpp
 * @note sed -e '/IEM_CIMPL_DEF_/!d' -e 's/IEM_CIMPL_DEF_/IEM_CIMPL_PROTO_/' -e 's/$/;/'
 * @{ */
IEM_CIMPL_PROTO_0(iemCImpl_popa_16);
IEM_CIMPL_PROTO_0(iemCImpl_popa_32);
IEM_CIMPL_PROTO_0(iemCImpl_pusha_16);
IEM_CIMPL_PROTO_0(iemCImpl_pusha_32);
IEM_CIMPL_PROTO_1(iemCImpl_pushf, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_popf, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_call_16, uint16_t, uNewPC);
IEM_CIMPL_PROTO_1(iemCImpl_call_rel_16, int16_t, offDisp);
IEM_CIMPL_PROTO_1(iemCImpl_call_32, uint32_t, uNewPC);
IEM_CIMPL_PROTO_1(iemCImpl_call_rel_32, int32_t, offDisp);
IEM_CIMPL_PROTO_1(iemCImpl_call_64, uint64_t, uNewPC);
IEM_CIMPL_PROTO_1(iemCImpl_call_rel_64, int64_t, offDisp);
IEM_CIMPL_PROTO_4(iemCImpl_BranchTaskSegment, uint16_t, uSel, IEMBRANCH, enmBranch, IEMMODE, enmEffOpSize, PIEMSELDESC, pDesc);
IEM_CIMPL_PROTO_4(iemCImpl_BranchTaskGate, uint16_t, uSel, IEMBRANCH, enmBranch, IEMMODE, enmEffOpSize, PIEMSELDESC, pDesc);
IEM_CIMPL_PROTO_4(iemCImpl_BranchCallGate, uint16_t, uSel, IEMBRANCH, enmBranch, IEMMODE, enmEffOpSize, PIEMSELDESC, pDesc);
IEM_CIMPL_PROTO_4(iemCImpl_BranchSysSel, uint16_t, uSel, IEMBRANCH, enmBranch, IEMMODE, enmEffOpSize, PIEMSELDESC, pDesc);
IEM_CIMPL_PROTO_3(iemCImpl_FarJmp, uint16_t, uSel, uint64_t, offSeg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_callf, uint16_t, uSel, uint64_t, offSeg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_retf, IEMMODE, enmEffOpSize, uint16_t, cbPop);
IEM_CIMPL_PROTO_2(iemCImpl_retn, IEMMODE, enmEffOpSize, uint16_t, cbPop);
IEM_CIMPL_PROTO_3(iemCImpl_enter, IEMMODE, enmEffOpSize, uint16_t, cbFrame, uint8_t, cParameters);
IEM_CIMPL_PROTO_1(iemCImpl_leave, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_int, uint8_t, u8Int, IEMINT, enmInt);
IEM_CIMPL_PROTO_1(iemCImpl_iret_real_v8086, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_4(iemCImpl_iret_prot_v8086, uint32_t, uNewEip, uint16_t, uNewCs, uint32_t, uNewFlags, uint64_t, uNewRsp);
IEM_CIMPL_PROTO_1(iemCImpl_iret_prot_NestedTask, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_iret_prot, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_iret_64bit, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_1(iemCImpl_iret, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_0(iemCImpl_loadall286);
IEM_CIMPL_PROTO_0(iemCImpl_syscall);
IEM_CIMPL_PROTO_0(iemCImpl_sysret);
IEM_CIMPL_PROTO_0(iemCImpl_sysenter);
IEM_CIMPL_PROTO_1(iemCImpl_sysexit, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_LoadSReg, uint8_t, iSegReg, uint16_t, uSel);
IEM_CIMPL_PROTO_2(iemCImpl_load_SReg, uint8_t, iSegReg, uint16_t, uSel);
IEM_CIMPL_PROTO_2(iemCImpl_pop_Sreg, uint8_t, iSegReg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_5(iemCImpl_load_SReg_Greg, uint16_t, uSel, uint64_t, offSeg, uint8_t, iSegReg, uint8_t, iGReg, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_VerX, uint16_t, uSel, bool, fWrite);
IEM_CIMPL_PROTO_3(iemCImpl_LarLsl_u64, uint64_t *, pu64Dst, uint16_t, uSel, bool, fIsLar);
IEM_CIMPL_PROTO_3(iemCImpl_LarLsl_u16, uint16_t *, pu16Dst, uint16_t, uSel, bool, fIsLar);
IEM_CIMPL_PROTO_3(iemCImpl_lgdt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_sgdt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_3(iemCImpl_lidt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_sidt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_1(iemCImpl_lldt, uint16_t, uNewLdt);
IEM_CIMPL_PROTO_2(iemCImpl_sldt_reg, uint8_t, iGReg, uint8_t, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_sldt_mem, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_1(iemCImpl_ltr, uint16_t, uNewTr);
IEM_CIMPL_PROTO_2(iemCImpl_str_reg, uint8_t, iGReg, uint8_t, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_str_mem, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Rd_Cd, uint8_t, iGReg, uint8_t, iCrReg);
IEM_CIMPL_PROTO_2(iemCImpl_smsw_reg, uint8_t, iGReg, uint8_t, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_smsw_mem, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_4(iemCImpl_load_CrX, uint8_t, iCrReg, uint64_t, uNewCrX, IEMACCESSCRX, enmAccessCrX, uint8_t, iGReg);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Cd_Rd, uint8_t, iCrReg, uint8_t, iGReg);
IEM_CIMPL_PROTO_2(iemCImpl_lmsw, uint16_t, u16NewMsw, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_0(iemCImpl_clts);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Rd_Dd, uint8_t, iGReg, uint8_t, iDrReg);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Dd_Rd, uint8_t, iDrReg, uint8_t, iGReg);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Rd_Td, uint8_t, iGReg, uint8_t, iTrReg);
IEM_CIMPL_PROTO_2(iemCImpl_mov_Td_Rd, uint8_t, iTrReg, uint8_t, iGReg);
IEM_CIMPL_PROTO_1(iemCImpl_invlpg, RTGCPTR, GCPtrPage);
IEM_CIMPL_PROTO_3(iemCImpl_invpcid, uint8_t, iEffSeg, RTGCPTR, GCPtrInvpcidDesc, uint64_t, uInvpcidType);
IEM_CIMPL_PROTO_0(iemCImpl_invd);
IEM_CIMPL_PROTO_0(iemCImpl_wbinvd);
IEM_CIMPL_PROTO_0(iemCImpl_rsm);
IEM_CIMPL_PROTO_0(iemCImpl_rdtsc);
IEM_CIMPL_PROTO_0(iemCImpl_rdtscp);
IEM_CIMPL_PROTO_0(iemCImpl_rdpmc);
IEM_CIMPL_PROTO_0(iemCImpl_rdmsr);
IEM_CIMPL_PROTO_0(iemCImpl_wrmsr);
IEM_CIMPL_PROTO_3(iemCImpl_in, uint16_t, u16Port, bool, fImm, uint8_t, cbReg);
IEM_CIMPL_PROTO_1(iemCImpl_in_eAX_DX, uint8_t, cbReg);
IEM_CIMPL_PROTO_3(iemCImpl_out, uint16_t, u16Port, bool, fImm, uint8_t, cbReg);
IEM_CIMPL_PROTO_1(iemCImpl_out_DX_eAX, uint8_t, cbReg);
IEM_CIMPL_PROTO_0(iemCImpl_cli);
IEM_CIMPL_PROTO_0(iemCImpl_sti);
IEM_CIMPL_PROTO_0(iemCImpl_hlt);
IEM_CIMPL_PROTO_1(iemCImpl_monitor, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_mwait);
IEM_CIMPL_PROTO_0(iemCImpl_swapgs);
IEM_CIMPL_PROTO_0(iemCImpl_cpuid);
IEM_CIMPL_PROTO_1(iemCImpl_aad, uint8_t, bImm);
IEM_CIMPL_PROTO_1(iemCImpl_aam, uint8_t, bImm);
IEM_CIMPL_PROTO_0(iemCImpl_daa);
IEM_CIMPL_PROTO_0(iemCImpl_das);
IEM_CIMPL_PROTO_0(iemCImpl_aaa);
IEM_CIMPL_PROTO_0(iemCImpl_aas);
IEM_CIMPL_PROTO_3(iemCImpl_bound_16, int16_t, idxArray, int16_t, idxLowerBound, int16_t, idxUpperBound);
IEM_CIMPL_PROTO_3(iemCImpl_bound_32, int32_t, idxArray, int32_t, idxLowerBound, int32_t, idxUpperBound);
IEM_CIMPL_PROTO_0(iemCImpl_xgetbv);
IEM_CIMPL_PROTO_0(iemCImpl_xsetbv);
IEM_CIMPL_PROTO_4(iemCImpl_cmpxchg16b_fallback_rendezvous, PRTUINT128U, pu128Dst, PRTUINT128U, pu128RaxRdx,
                  PRTUINT128U, pu128RbxRcx, uint32_t *, pEFlags);
IEM_CIMPL_PROTO_2(iemCImpl_clflush_clflushopt, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_1(iemCImpl_finit, bool, fCheckXcpts);
IEM_CIMPL_PROTO_3(iemCImpl_fxsave, uint8_t, iEffSeg, RTGCPTR, GCPtrEff, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_fxrstor, uint8_t, iEffSeg, RTGCPTR, GCPtrEff, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_xsave, uint8_t, iEffSeg, RTGCPTR, GCPtrEff, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_3(iemCImpl_xrstor, uint8_t, iEffSeg, RTGCPTR, GCPtrEff, IEMMODE, enmEffOpSize);
IEM_CIMPL_PROTO_2(iemCImpl_stmxcsr, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_2(iemCImpl_vstmxcsr, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_2(iemCImpl_ldmxcsr, uint8_t, iEffSeg, RTGCPTR, GCPtrEff);
IEM_CIMPL_PROTO_3(iemCImpl_fnstenv, IEMMODE, enmEffOpSize, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_3(iemCImpl_fnsave, IEMMODE, enmEffOpSize, uint8_t, iEffSeg, RTGCPTR, GCPtrEffDst);
IEM_CIMPL_PROTO_3(iemCImpl_fldenv, IEMMODE, enmEffOpSize, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_3(iemCImpl_frstor, IEMMODE, enmEffOpSize, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc);
IEM_CIMPL_PROTO_1(iemCImpl_fldcw, uint16_t, u16Fcw);
IEM_CIMPL_PROTO_1(iemCImpl_fxch_underflow, uint8_t, iStReg);
IEM_CIMPL_PROTO_3(iemCImpl_fcomi_fucomi, uint8_t, iStReg, PFNIEMAIMPLFPUR80EFL, pfnAImpl, bool, fPop);
/** @} */

/** @name IEMAllCImplStrInstr.cpp.h
 * @note sed -e '/IEM_CIMPL_DEF_/!d' -e 's/IEM_CIMPL_DEF_/IEM_CIMPL_PROTO_/' -e 's/$/;/' -e 's/RT_CONCAT4(//' \
 *           -e 's/,ADDR_SIZE)/64/g' -e 's/,OP_SIZE,/64/g' -e 's/,OP_rAX,/rax/g' IEMAllCImplStrInstr.cpp.h
 * @{ */
IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op8_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op8_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_al_m16);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_al_m16);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op8_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_al_m16);
IEM_CIMPL_PROTO_1(iemCImpl_lods_al_m16, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op8_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op8_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op8_addr16, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op8_addr16, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op16_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op16_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_ax_m16);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_ax_m16);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op16_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_ax_m16);
IEM_CIMPL_PROTO_1(iemCImpl_lods_ax_m16, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op16_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op16_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op16_addr16, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op16_addr16, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op32_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op32_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_eax_m16);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_eax_m16);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op32_addr16, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_eax_m16);
IEM_CIMPL_PROTO_1(iemCImpl_lods_eax_m16, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op32_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op32_addr16, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op32_addr16, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op32_addr16, uint8_t, iEffSeg, bool, fIoChecked);


IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op8_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op8_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_al_m32);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_al_m32);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op8_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_al_m32);
IEM_CIMPL_PROTO_1(iemCImpl_lods_al_m32, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op8_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op8_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op8_addr32, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op8_addr32, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op16_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op16_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_ax_m32);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_ax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op16_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_ax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_lods_ax_m32, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op16_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op16_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op16_addr32, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op16_addr32, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op32_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op32_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_eax_m32);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_eax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op32_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_eax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_lods_eax_m32, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op32_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op32_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op32_addr32, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op32_addr32, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op64_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op64_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_rax_m32);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_rax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op64_addr32, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_rax_m32);
IEM_CIMPL_PROTO_1(iemCImpl_lods_rax_m32, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op64_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op64_addr32, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op64_addr32, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op64_addr32, uint8_t, iEffSeg, bool, fIoChecked);


IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op8_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op8_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_al_m64);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_al_m64);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op8_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_al_m64);
IEM_CIMPL_PROTO_1(iemCImpl_lods_al_m64, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op8_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op8_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op8_addr64, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op8_addr64, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op16_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op16_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_ax_m64);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_ax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op16_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_ax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_lods_ax_m64, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op16_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op16_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op16_addr64, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op16_addr64, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op32_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op32_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_eax_m64);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_eax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op32_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_eax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_lods_eax_m64, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op32_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op32_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op32_addr64, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op32_addr64, uint8_t, iEffSeg, bool, fIoChecked);

IEM_CIMPL_PROTO_1(iemCImpl_repe_cmps_op64_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_repne_cmps_op64_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_repe_scas_rax_m64);
IEM_CIMPL_PROTO_0(iemCImpl_repne_scas_rax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_rep_movs_op64_addr64, uint8_t, iEffSeg);
IEM_CIMPL_PROTO_0(iemCImpl_stos_rax_m64);
IEM_CIMPL_PROTO_1(iemCImpl_lods_rax_m64, int8_t, iEffSeg);
IEM_CIMPL_PROTO_1(iemCImpl_ins_op64_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_1(iemCImpl_rep_ins_op64_addr64, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_outs_op64_addr64, uint8_t, iEffSeg, bool, fIoChecked);
IEM_CIMPL_PROTO_2(iemCImpl_rep_outs_op64_addr64, uint8_t, iEffSeg, bool, fIoChecked);
/** @} */

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
VBOXSTRICTRC    iemVmxVmexit(PVMCPUCC pVCpu, uint32_t uExitReason, uint64_t u64ExitQual) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstr(PVMCPUCC pVCpu, uint32_t uExitReason, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrNeedsInfo(PVMCPUCC pVCpu, uint32_t uExitReason, VMXINSTRID uInstrId, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitTaskSwitch(PVMCPUCC pVCpu, IEMTASKSWITCH enmTaskSwitch, RTSEL SelNewTss, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitEvent(PVMCPUCC pVCpu, uint8_t uVector, uint32_t fFlags, uint32_t uErrCode, uint64_t uCr2, uint8_t cbInstr)  RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitEventDoubleFault(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitEpt(PVMCPUCC pVCpu, PPGMPTWALK pWalk, uint32_t fAccess, uint32_t fSlatFail, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitPreemptTimer(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMwait(PVMCPUCC pVCpu, bool fMonitorHwArmed, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrIo(PVMCPUCC pVCpu, VMXINSTRID uInstrId, uint16_t u16Port,
                                    bool fImm, uint8_t cbAccess, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrStrIo(PVMCPUCC pVCpu, VMXINSTRID uInstrId, uint16_t u16Port, uint8_t cbAccess,
                                       bool fRep, VMXEXITINSTRINFO ExitInstrInfo, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovDrX(PVMCPUCC pVCpu, VMXINSTRID uInstrId, uint8_t iDrReg, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovToCr8(PVMCPUCC pVCpu, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovFromCr8(PVMCPUCC pVCpu, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovToCr3(PVMCPUCC pVCpu, uint64_t uNewCr3, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovFromCr3(PVMCPUCC pVCpu, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrMovToCr0Cr4(PVMCPUCC pVCpu, uint8_t iCrReg, uint64_t *puNewCrX, uint8_t iGReg, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrClts(PVMCPUCC pVCpu, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrLmsw(PVMCPUCC pVCpu, uint32_t uGuestCr0, uint16_t *pu16NewMsw,
                                      RTGCPTR GCPtrEffDst, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVmexitInstrInvlpg(PVMCPUCC pVCpu, RTGCPTR GCPtrPage, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxApicWriteEmulation(PVMCPUCC pVCpu) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxVirtApicAccessUnused(PVMCPUCC pVCpu, PRTGCPHYS pGCPhysAccess, size_t cbAccess, uint32_t fAccess) RT_NOEXCEPT;
uint32_t        iemVmxVirtApicReadRaw32(PVMCPUCC pVCpu, uint16_t offReg) RT_NOEXCEPT;
void            iemVmxVirtApicWriteRaw32(PVMCPUCC pVCpu, uint16_t offReg, uint32_t uReg) RT_NOEXCEPT;
VBOXSTRICTRC    iemVmxInvvpid(PVMCPUCC pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPTR GCPtrInvvpidDesc,
                              uint64_t u64InvvpidType, PCVMXVEXITINFO pExitInfo) RT_NOEXCEPT;
bool            iemVmxIsRdmsrWrmsrInterceptSet(PCVMCPU pVCpu, uint32_t uExitReason, uint32_t idMsr) RT_NOEXCEPT;
IEM_CIMPL_PROTO_0(iemCImpl_vmxoff);
IEM_CIMPL_PROTO_2(iemCImpl_vmxon, uint8_t, iEffSeg, RTGCPTR, GCPtrVmxon);
IEM_CIMPL_PROTO_0(iemCImpl_vmlaunch);
IEM_CIMPL_PROTO_0(iemCImpl_vmresume);
IEM_CIMPL_PROTO_2(iemCImpl_vmptrld, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs);
IEM_CIMPL_PROTO_2(iemCImpl_vmptrst, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs);
IEM_CIMPL_PROTO_2(iemCImpl_vmclear, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs);
IEM_CIMPL_PROTO_2(iemCImpl_vmwrite_reg, uint64_t, u64Val, uint64_t, u64VmcsField);
IEM_CIMPL_PROTO_3(iemCImpl_vmwrite_mem, uint8_t, iEffSeg, RTGCPTR, GCPtrVal, uint32_t, u64VmcsField);
IEM_CIMPL_PROTO_2(iemCImpl_vmread_reg64, uint64_t *, pu64Dst, uint64_t, u64VmcsField);
IEM_CIMPL_PROTO_2(iemCImpl_vmread_reg32, uint32_t *, pu32Dst, uint32_t, u32VmcsField);
IEM_CIMPL_PROTO_3(iemCImpl_vmread_mem_reg64, uint8_t, iEffSeg, RTGCPTR, GCPtrDst, uint32_t, u64VmcsField);
IEM_CIMPL_PROTO_3(iemCImpl_vmread_mem_reg32, uint8_t, iEffSeg, RTGCPTR, GCPtrDst, uint32_t, u32VmcsField);
IEM_CIMPL_PROTO_3(iemCImpl_invvpid, uint8_t, iEffSeg, RTGCPTR, GCPtrInvvpidDesc, uint64_t, uInvvpidType);
IEM_CIMPL_PROTO_3(iemCImpl_invept, uint8_t, iEffSeg, RTGCPTR, GCPtrInveptDesc, uint64_t, uInveptType);
IEM_CIMPL_PROTO_0(iemCImpl_vmx_pause);
#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
VBOXSTRICTRC    iemSvmVmexit(PVMCPUCC pVCpu, uint64_t uExitCode, uint64_t uExitInfo1, uint64_t uExitInfo2) RT_NOEXCEPT;
VBOXSTRICTRC    iemHandleSvmEventIntercept(PVMCPUCC pVCpu, uint8_t u8Vector, uint32_t fFlags, uint32_t uErr, uint64_t uCr2) RT_NOEXCEPT;
VBOXSTRICTRC    iemSvmHandleIOIntercept(PVMCPUCC pVCpu, uint16_t u16Port, SVMIOIOTYPE enmIoType, uint8_t cbReg,
                                        uint8_t cAddrSizeBits, uint8_t iEffSeg, bool fRep, bool fStrIo, uint8_t cbInstr) RT_NOEXCEPT;
VBOXSTRICTRC    iemSvmHandleMsrIntercept(PVMCPUCC pVCpu, uint32_t idMsr, bool fWrite) RT_NOEXCEPT;
IEM_CIMPL_PROTO_0(iemCImpl_vmrun);
IEM_CIMPL_PROTO_0(iemCImpl_vmload);
IEM_CIMPL_PROTO_0(iemCImpl_vmsave);
IEM_CIMPL_PROTO_0(iemCImpl_clgi);
IEM_CIMPL_PROTO_0(iemCImpl_stgi);
IEM_CIMPL_PROTO_0(iemCImpl_invlpga);
IEM_CIMPL_PROTO_0(iemCImpl_skinit);
IEM_CIMPL_PROTO_0(iemCImpl_svm_pause);
#endif

IEM_CIMPL_PROTO_0(iemCImpl_vmcall);  /* vmx */
IEM_CIMPL_PROTO_0(iemCImpl_vmmcall); /* svm */
IEM_CIMPL_PROTO_1(iemCImpl_Hypercall, uint16_t, uDisOpcode); /* both */


extern const PFNIEMOP g_apfnOneByteMap[256];

/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_IEMInternal_h */

