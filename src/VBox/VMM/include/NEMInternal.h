/* $Id$ */
/** @file
 * NEM - Internal header file.
 */

/*
 * Copyright (C) 2018-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_NEMInternal_h
#define VMM_INCLUDED_SRC_include_NEMInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/cpum.h> /* For CPUMCPUVENDOR. */
#include <VBox/vmm/stam.h>
#include <VBox/vmm/vmapi.h>
#ifdef RT_OS_WINDOWS
#include <iprt/nt/hyperv.h>
#include <iprt/critsect.h>
#elif defined(RT_OS_DARWIN)
# include "VMXInternal.h"
#endif

RT_C_DECLS_BEGIN


/** @defgroup grp_nem_int      Internal
 * @ingroup grp_nem
 * @internal
 * @{
 */

#if defined(VBOX_WITH_PGM_NEM_MODE) && !defined(VBOX_WITH_NATIVE_NEM)
# error "VBOX_WITH_PGM_NEM_MODE requires VBOX_WITH_NATIVE_NEM to be defined"
#endif


#ifdef RT_OS_WINDOWS
/*
 * Windows: Code configuration.
 */
# ifndef VBOX_WITH_PGM_NEM_MODE
#  define NEM_WIN_USE_HYPERCALLS_FOR_PAGES
#endif
//# define NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS   /**< Applies to ring-3 code only. Useful for testing VID API. */
//# define NEM_WIN_USE_OUR_OWN_RUN_API            /**< Applies to ring-3 code only. Useful for testing VID API. */
//# define NEM_WIN_WITH_RING0_RUNLOOP             /**< Enables the ring-0 runloop. */
//# define NEM_WIN_USE_RING0_RUNLOOP_BY_DEFAULT   /**< For quickly testing ring-3 API without messing with CFGM. */
# if defined(NEM_WIN_USE_OUR_OWN_RUN_API) && !defined(NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS)
#  error "NEM_WIN_USE_OUR_OWN_RUN_API requires NEM_WIN_USE_HYPERCALLS_FOR_REGISTERS"
# endif
# if defined(NEM_WIN_USE_OUR_OWN_RUN_API) && !defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES)
#  error "NEM_WIN_USE_OUR_OWN_RUN_API requires NEM_WIN_USE_HYPERCALLS_FOR_PAGES"
# endif
# if defined(NEM_WIN_WITH_RING0_RUNLOOP) && !defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES)
#  error "NEM_WIN_WITH_RING0_RUNLOOP requires NEM_WIN_USE_HYPERCALLS_FOR_PAGES"
# endif
# if defined(VBOX_WITH_PGM_NEM_MODE) && defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES)
#  error "VBOX_WITH_PGM_NEM_MODE cannot be used together with NEM_WIN_USE_HYPERCALLS_FOR_PAGES"
# endif

/**
 * Windows VID I/O control information.
 */
typedef struct NEMWINIOCTL
{
    /** The I/O control function number. */
    uint32_t    uFunction;
    uint32_t    cbInput;
    uint32_t    cbOutput;
} NEMWINIOCTL;

/** @name Windows: Our two-bit physical page state for PGMPAGE
 * @{ */
# define NEM_WIN_PAGE_STATE_NOT_SET     0
# define NEM_WIN_PAGE_STATE_UNMAPPED    1
# define NEM_WIN_PAGE_STATE_READABLE    2
# define NEM_WIN_PAGE_STATE_WRITABLE    3
/** @} */

/** Windows: Checks if a_GCPhys is subject to the limited A20 gate emulation. */
# define NEM_WIN_IS_SUBJECT_TO_A20(a_GCPhys)    ((RTGCPHYS)((a_GCPhys) - _1M) < (RTGCPHYS)_64K)
/** Windows: Checks if a_GCPhys is relevant to the limited A20 gate emulation. */
# define NEM_WIN_IS_RELEVANT_TO_A20(a_GCPhys)    \
    ( ((RTGCPHYS)((a_GCPhys) - _1M) < (RTGCPHYS)_64K) || ((RTGCPHYS)(a_GCPhys) < (RTGCPHYS)_64K) )

/** The CPUMCTX_EXTRN_XXX mask for IEM. */
# define NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM      (  IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_INHIBIT_INT \
                                                  | CPUMCTX_EXTRN_INHIBIT_NMI )
/** The CPUMCTX_EXTRN_XXX mask for IEM when raising exceptions. */
# define NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM_XCPT (IEM_CPUMCTX_EXTRN_XCPT_MASK | NEM_WIN_CPUMCTX_EXTRN_MASK_FOR_IEM)

/** @name Windows: Interrupt window flags (NEM_WIN_INTW_F_XXX).
 * @{  */
# define NEM_WIN_INTW_F_NMI             UINT8_C(0x01)
# define NEM_WIN_INTW_F_REGULAR         UINT8_C(0x02)
# define NEM_WIN_INTW_F_PRIO_MASK       UINT8_C(0x3c)
# define NEM_WIN_INTW_F_PRIO_SHIFT      2
/** @} */

#endif /* RT_OS_WINDOWS */


#ifdef RT_OS_DARWIN
/** vCPU ID declaration to avoid dragging in HV headers here. */
typedef unsigned hv_vcpuid_t;
/** The HV VM memory space ID (ASID). */
typedef unsigned hv_vm_space_t;


/** @name Darwin: Our two-bit physical page state for PGMPAGE
 * @{ */
# define NEM_DARWIN_PAGE_STATE_NOT_SET     0
# define NEM_DARWIN_PAGE_STATE_UNMAPPED    1
# define NEM_DARWIN_PAGE_STATE_READABLE    2
# define NEM_DARWIN_PAGE_STATE_WRITABLE    3
/** @} */

/** The CPUMCTX_EXTRN_XXX mask for IEM. */
# define NEM_DARWIN_CPUMCTX_EXTRN_MASK_FOR_IEM      (  IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_INHIBIT_INT \
                                                     | CPUMCTX_EXTRN_INHIBIT_NMI )
/** The CPUMCTX_EXTRN_XXX mask for IEM when raising exceptions. */
# define NEM_DARWIN_CPUMCTX_EXTRN_MASK_FOR_IEM_XCPT (IEM_CPUMCTX_EXTRN_XCPT_MASK | NEM_DARWIN_CPUMCTX_EXTRN_MASK_FOR_IEM)

#endif


/** Trick to make slickedit see the static functions in the template. */
#ifndef IN_SLICKEDIT
# define NEM_TMPL_STATIC static
#else
# define NEM_TMPL_STATIC
#endif


/**
 * Generic NEM exit type enumeration for use with EMHistoryAddExit.
 *
 * On windows we've got two different set of exit types and they are both jumping
 * around the place value wise, so EM can use their values.
 *
 * @note We only have exit types for exits not covered by EM here.
 */
typedef enum NEMEXITTYPE
{
    NEMEXITTYPE_INVALID = 0,

    /* Common: */
    NEMEXITTYPE_INTTERRUPT_WINDOW,
    NEMEXITTYPE_HALT,

    /* Windows: */
    NEMEXITTYPE_UNRECOVERABLE_EXCEPTION,
    NEMEXITTYPE_INVALID_VP_REGISTER_VALUE,
    NEMEXITTYPE_XCPT_UD,
    NEMEXITTYPE_XCPT_DB,
    NEMEXITTYPE_XCPT_BP,
    NEMEXITTYPE_CANCELED,
    NEMEXITTYPE_MEMORY_ACCESS,

    /* Linux: */
    NEMEXITTYPE_INTERNAL_ERROR_EMULATION,
    NEMEXITTYPE_INTERNAL_ERROR_FATAL,
    NEMEXITTYPE_INTERRUPTED,
    NEMEXITTYPE_FAILED_ENTRY,

    /* End of valid types. */
    NEMEXITTYPE_END
} NEMEXITTYPE;


/**
 * NEM VM Instance data.
 */
typedef struct NEM
{
    /** NEM_MAGIC. */
    uint32_t                    u32Magic;

    /** Set if enabled. */
    bool                        fEnabled;
    /** Set if long mode guests are allowed. */
    bool                        fAllow64BitGuests;

#if defined(RT_OS_LINUX)
    /** The '/dev/kvm' file descriptor.   */
    int32_t                     fdKvm;
    /** The KVM_CREATE_VM file descriptor. */
    int32_t                     fdVm;

    /** KVM_GET_VCPU_MMAP_SIZE. */
    uint32_t                    cbVCpuMmap;
    /** KVM_CAP_NR_MEMSLOTS. */
    uint32_t                    cMaxMemSlots;
    /** KVM_CAP_X86_ROBUST_SINGLESTEP. */
    bool                        fRobustSingleStep;

    /** Hint where there might be a free slot. */
    uint16_t                    idPrevSlot;
    /** Memory slot ID allocation bitmap. */
    uint64_t                    bmSlotIds[_32K / 8 / sizeof(uint64_t)];

#elif defined(RT_OS_WINDOWS)
    /** Set if we've created the EMTs. */
    bool                        fCreatedEmts : 1;
    /** WHvRunVpExitReasonX64Cpuid is supported. */
    bool                        fExtendedMsrExit : 1;
    /** WHvRunVpExitReasonX64MsrAccess is supported. */
    bool                        fExtendedCpuIdExit : 1;
    /** WHvRunVpExitReasonException is supported. */
    bool                        fExtendedXcptExit : 1;
# ifndef VBOX_WITH_PGM_NEM_MODE
    /** Set if we're using the ring-0 API to do the work. */
    bool                        fUseRing0Runloop : 1;
# endif
# ifdef NEM_WIN_WITH_A20
    /** Set if we've started more than one CPU and cannot mess with A20. */
    bool                        fA20Fixed : 1;
    /** Set if A20 is enabled. */
    bool                        fA20Enabled : 1;
# endif
    /** The reported CPU vendor.   */
    CPUMCPUVENDOR               enmCpuVendor;
    /** Cache line flush size as a power of two. */
    uint8_t                     cCacheLineFlushShift;
    /** The result of WHvCapabilityCodeProcessorFeatures. */
    union
    {
        /** 64-bit view. */
        uint64_t                u64;
# ifdef _WINHVAPIDEFS_H_
        /** Interpreed features. */
        WHV_PROCESSOR_FEATURES  u;
# endif
    } uCpuFeatures;

    /** The partition handle. */
# ifdef _WINHVAPIDEFS_H_
    WHV_PARTITION_HANDLE
# else
    RTHCUINTPTR
# endif
                                hPartition;
    /** The device handle for the partition, for use with Vid APIs or direct I/O
     * controls. */
    RTR3PTR                     hPartitionDevice;
    /** The Hyper-V partition ID.   */
    uint64_t                    idHvPartition;

    /** Number of currently mapped pages. */
    uint32_t volatile           cMappedPages;
#  ifndef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    /** Max number of pages we dare map at once. */
    uint32_t                    cMaxMappedPages;
#  endif
    STAMCOUNTER                 StatMapPage;
    STAMCOUNTER                 StatUnmapPage;
#  ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    STAMCOUNTER                 StatRemapPage;
    STAMCOUNTER                 StatRemapPageFailed;
#  elif !defined(VBOX_WITH_PGM_NEM_MODE)
    STAMCOUNTER                 StatUnmapAllPages;
#  endif
    STAMCOUNTER                 StatMapPageFailed;
    STAMCOUNTER                 StatUnmapPageFailed;
#  ifdef VBOX_WITH_PGM_NEM_MODE
    STAMPROFILE                 StatProfMapGpaRange;
    STAMPROFILE                 StatProfUnmapGpaRange;
#  endif
#  ifndef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    STAMPROFILE                 StatProfMapGpaRangePage;
    STAMPROFILE                 StatProfUnmapGpaRangePage;
#  endif

#  ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    /** Info about the VidGetHvPartitionId I/O control interface. */
    NEMWINIOCTL                 IoCtlGetHvPartitionId;
    /** Info about the VidGetPartitionProperty I/O control interface. */
    NEMWINIOCTL                 IoCtlGetPartitionProperty;
#  endif
#  ifdef NEM_WIN_WITH_RING0_RUNLOOP
    /** Info about the VidStartVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlStartVirtualProcessor;
    /** Info about the VidStopVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlStopVirtualProcessor;
    /** Info about the VidStopVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlMessageSlotHandleAndGetNext;
#  endif

    /** Statistics updated by NEMR0UpdateStatistics. */
    struct
    {
        uint64_t                cPagesAvailable;
        uint64_t                cPagesInUse;
    } R0Stats;

#elif defined(RT_OS_DARWIN)
    /** Set if we've created the EMTs. */
    bool                        fCreatedEmts : 1;
    /** Set if hv_vm_create() was called successfully. */
    bool                        fCreatedVm   : 1;
    /** Set if hv_vm_space_create() was called successfully. */
    bool                        fCreatedAsid : 1;
    /** The ASID for this VM (only valid if fCreatedAsid is true). */
    hv_vm_space_t               uVmAsid;
    STAMCOUNTER                 StatMapPage;
    STAMCOUNTER                 StatUnmapPage;
    STAMCOUNTER                 StatMapPageFailed;
    STAMCOUNTER                 StatUnmapPageFailed;
#endif /* RT_OS_WINDOWS */
} NEM;
/** Pointer to NEM VM instance data. */
typedef NEM *PNEM;

/** NEM::u32Magic value. */
#define NEM_MAGIC               UINT32_C(0x004d454e)
/** NEM::u32Magic value after termination. */
#define NEM_MAGIC_DEAD          UINT32_C(0xdead1111)


/**
 * NEM VMCPU Instance data.
 */
typedef struct NEMCPU
{
    /** NEMCPU_MAGIC. */
    uint32_t                    u32Magic;
    /** Whether \#UD needs to be intercepted and presented to GIM. */
    bool                        fGIMTrapXcptUD : 1;
    /** Whether \#GP needs to be intercept for mesa driver workaround. */
    bool                        fTrapXcptGpForLovelyMesaDrv: 1;

#if defined(RT_OS_LINUX)
    uint8_t                     abPadding[3];
    /** The KVM VCpu file descriptor. */
    int32_t                     fdVCpu;
    /** Pointer to the KVM_RUN data exchange region. */
    R3PTRTYPE(struct kvm_run *) pRun;
    /** The MSR_IA32_APICBASE value known to KVM. */
    uint64_t                    uKvmApicBase;

    /** @name Statistics
     * @{ */
    STAMCOUNTER                 StatExitTotal;
    STAMCOUNTER                 StatExitIo;
    STAMCOUNTER                 StatExitMmio;
    STAMCOUNTER                 StatExitSetTpr;
    STAMCOUNTER                 StatExitTprAccess;
    STAMCOUNTER                 StatExitRdMsr;
    STAMCOUNTER                 StatExitWrMsr;
    STAMCOUNTER                 StatExitIrqWindowOpen;
    STAMCOUNTER                 StatExitHalt;
    STAMCOUNTER                 StatExitIntr;
    STAMCOUNTER                 StatExitHypercall;
    STAMCOUNTER                 StatExitDebug;
    STAMCOUNTER                 StatExitBusLock;
    STAMCOUNTER                 StatExitInternalErrorEmulation;
    STAMCOUNTER                 StatExitInternalErrorFatal;
# if 0
    STAMCOUNTER                 StatExitCpuId;
    STAMCOUNTER                 StatExitUnrecoverable;
    STAMCOUNTER                 StatGetMsgTimeout;
    STAMCOUNTER                 StatStopCpuSuccess;
    STAMCOUNTER                 StatStopCpuPending;
    STAMCOUNTER                 StatStopCpuPendingAlerts;
    STAMCOUNTER                 StatStopCpuPendingOdd;
    STAMCOUNTER                 StatCancelChangedState;
    STAMCOUNTER                 StatCancelAlertedThread;
# endif
    STAMCOUNTER                 StatBreakOnCancel;
    STAMCOUNTER                 StatBreakOnFFPre;
    STAMCOUNTER                 StatBreakOnFFPost;
    STAMCOUNTER                 StatBreakOnStatus;
    STAMCOUNTER                 StatFlushExitOnReturn;
    STAMCOUNTER                 StatFlushExitOnReturn1Loop;
    STAMCOUNTER                 StatFlushExitOnReturn2Loops;
    STAMCOUNTER                 StatFlushExitOnReturn3Loops;
    STAMCOUNTER                 StatFlushExitOnReturn4PlusLoops;
    STAMCOUNTER                 StatImportOnDemand;
    STAMCOUNTER                 StatImportOnReturn;
    STAMCOUNTER                 StatImportOnReturnSkipped;
    STAMCOUNTER                 StatImportPendingInterrupt;
    STAMCOUNTER                 StatExportPendingInterrupt;
    STAMCOUNTER                 StatQueryCpuTick;
    /** @} */


#elif defined(RT_OS_WINDOWS)
    /** The current state of the interrupt windows (NEM_WIN_INTW_F_XXX). */
    uint8_t                     fCurrentInterruptWindows;
    /** The desired state of the interrupt windows (NEM_WIN_INTW_F_XXX). */
    uint8_t                     fDesiredInterruptWindows;
    /** Last copy of HV_X64_VP_EXECUTION_STATE::InterruptShadow. */
    bool                        fLastInterruptShadow : 1;
# ifdef NEM_WIN_WITH_RING0_RUNLOOP
    /** Pending VINF_NEM_FLUSH_TLB. */
    int32_t                     rcPending;
# else
    uint32_t                    uPadding;
# endif
    /** The VID_MSHAGN_F_XXX flags.
     * Either VID_MSHAGN_F_HANDLE_MESSAGE | VID_MSHAGN_F_GET_NEXT_MESSAGE or zero. */
    uint32_t                    fHandleAndGetFlags;
    /** What VidMessageSlotMap returns and is used for passing exit info. */
    RTR3PTR                     pvMsgSlotMapping;
    /** The windows thread handle. */
    RTR3PTR                     hNativeThreadHandle;
    /** Parameters for making Hyper-V hypercalls. */
    union
    {
        uint8_t                 ab[64];
        /** Arguments for NEMR0MapPages (HvCallMapGpaPages). */
        struct
        {
            RTGCPHYS            GCPhysSrc;
            RTGCPHYS            GCPhysDst; /**< Same as GCPhysSrc except maybe when the A20 gate is disabled. */
            uint32_t            cPages;
            HV_MAP_GPA_FLAGS    fFlags;
        }                       MapPages;
        /** Arguments for NEMR0UnmapPages (HvCallUnmapGpaPages). */
        struct
        {
            RTGCPHYS            GCPhys;
            uint32_t            cPages;
        }                       UnmapPages;
        /** Result from NEMR0QueryCpuTick. */
        struct
        {
            uint64_t            cTicks;
            uint32_t            uAux;
        }                       QueryCpuTick;
        /** Input and output for NEMR0DoExperiment. */
        struct
        {
            uint32_t            uItem;
            bool                fSuccess;
            uint64_t            uStatus;
            uint64_t            uLoValue;
            uint64_t            uHiValue;
        }                       Experiment;
    } Hypercall;
    /** I/O control buffer, we always use this for I/O controls. */
    union
    {
        uint8_t                 ab[64];
        HV_PARTITION_ID         idPartition;
        HV_VP_INDEX             idCpu;
        struct
        {
            uint64_t            enmProperty;
            uint64_t            uValue;
        } GetProp;
# ifdef VID_MSHAGN_F_GET_NEXT_MESSAGE
        VID_IOCTL_INPUT_MESSAGE_SLOT_HANDLE_AND_GET_NEXT MsgSlotHandleAndGetNext;
# endif
    } uIoCtlBuf;

    /** @name Statistics
     * @{ */
    STAMCOUNTER                 StatExitPortIo;
    STAMCOUNTER                 StatExitMemUnmapped;
    STAMCOUNTER                 StatExitMemIntercept;
    STAMCOUNTER                 StatExitHalt;
    STAMCOUNTER                 StatExitInterruptWindow;
    STAMCOUNTER                 StatExitCpuId;
    STAMCOUNTER                 StatExitMsr;
    STAMCOUNTER                 StatExitException;
    STAMCOUNTER                 StatExitExceptionBp;
    STAMCOUNTER                 StatExitExceptionDb;
    STAMCOUNTER                 StatExitExceptionGp;
    STAMCOUNTER                 StatExitExceptionGpMesa;
    STAMCOUNTER                 StatExitExceptionUd;
    STAMCOUNTER                 StatExitExceptionUdHandled;
    STAMCOUNTER                 StatExitUnrecoverable;
    STAMCOUNTER                 StatGetMsgTimeout;
    STAMCOUNTER                 StatStopCpuSuccess;
    STAMCOUNTER                 StatStopCpuPending;
    STAMCOUNTER                 StatStopCpuPendingAlerts;
    STAMCOUNTER                 StatStopCpuPendingOdd;
    STAMCOUNTER                 StatCancelChangedState;
    STAMCOUNTER                 StatCancelAlertedThread;
    STAMCOUNTER                 StatBreakOnCancel;
    STAMCOUNTER                 StatBreakOnFFPre;
    STAMCOUNTER                 StatBreakOnFFPost;
    STAMCOUNTER                 StatBreakOnStatus;
    STAMCOUNTER                 StatImportOnDemand;
    STAMCOUNTER                 StatImportOnReturn;
    STAMCOUNTER                 StatImportOnReturnSkipped;
    STAMCOUNTER                 StatQueryCpuTick;
    /** @} */

#elif defined(RT_OS_DARWIN)
    /** The vCPU handle associated with the EMT executing this vCPU. */
    hv_vcpuid_t                 hVCpuId;

    /** @name State shared with the VT-x code.
     * @{ */
    /** Whether we should use the debug loop because of single stepping or special
     *  debug breakpoints / events are armed. */
    bool                        fUseDebugLoop;
    /** Whether we're executing a single instruction. */
    bool                        fSingleInstruction;

    bool                        afAlignment0[2];

    /** An additional error code used for some gurus. */
    uint32_t                    u32HMError;
    /** The last exit-to-ring-3 reason. */
    int32_t                     rcLastExitToR3;
    /** CPU-context changed flags (see HM_CHANGED_xxx). */
    uint64_t                    fCtxChanged;

    /** The guest VMCS information. */
    VMXVMCSINFO                 VmcsInfo;

    /** VT-x data.   */
    struct HMCPUVMX
    {
        /** @name Guest information.
         * @{ */
        /** Guest VMCS information shared with ring-3. */
        VMXVMCSINFOSHARED           VmcsInfo;
        /** Nested-guest VMCS information shared with ring-3. */
        VMXVMCSINFOSHARED           VmcsInfoNstGst;
        /** Whether the nested-guest VMCS was the last current VMCS (shadow copy for ring-3).
         * @see HMR0PERVCPU::vmx.fSwitchedToNstGstVmcs  */
        bool                        fSwitchedToNstGstVmcsCopyForRing3;
        /** Whether the static guest VMCS controls has been merged with the
         *  nested-guest VMCS controls. */
        bool                        fMergedNstGstCtls;
        /** Whether the nested-guest VMCS has been copied to the shadow VMCS. */
        bool                        fCopiedNstGstToShadowVmcs;
        /** Whether flushing the TLB is required due to switching to/from the
         *  nested-guest. */
        bool                        fSwitchedNstGstFlushTlb;
        /** Alignment. */
        bool                        afAlignment0[4];
        /** Cached guest APIC-base MSR for identifying when to map the APIC-access page. */
        uint64_t                    u64GstMsrApicBase;
        /** @} */

        /** @name Error reporting and diagnostics.
         * @{ */
        /** VT-x error-reporting (mainly for ring-3 propagation). */
        struct
        {
            RTCPUID                 idCurrentCpu;
            RTCPUID                 idEnteredCpu;
            RTHCPHYS                HCPhysCurrentVmcs;
            uint32_t                u32VmcsRev;
            uint32_t                u32InstrError;
            uint32_t                u32ExitReason;
            uint32_t                u32GuestIntrState;
        } LastError;
        /** @} */
    } vmx;

    /** Event injection state. */
    HMEVENT                     Event;

    /** Current shadow paging mode for updating CR4.
     * @todo move later (@bugref{9217}).  */
    PGMMODE                     enmShadowMode;
    uint32_t                    u32TemporaryPadding;

    /** The PAE PDPEs used with Nested Paging (only valid when
     *  VMCPU_FF_HM_UPDATE_PAE_PDPES is set). */
    X86PDPE                     aPdpes[4];
    /** Pointer to the VMX statistics. */
    PVMXSTATISTICS              pVmxStats;

    /** @name Statistics
     * @{ */
    STAMCOUNTER                 StatExitAll;
    STAMCOUNTER                 StatBreakOnCancel;
    STAMCOUNTER                 StatBreakOnFFPre;
    STAMCOUNTER                 StatBreakOnFFPost;
    STAMCOUNTER                 StatBreakOnStatus;
    STAMCOUNTER                 StatImportOnDemand;
    STAMCOUNTER                 StatImportOnReturn;
    STAMCOUNTER                 StatImportOnReturnSkipped;
    STAMCOUNTER                 StatQueryCpuTick;
#ifdef VBOX_WITH_STATISTICS
    STAMPROFILEADV              StatProfGstStateImport;
    STAMPROFILEADV              StatProfGstStateExport;
#endif
    /** @} */

    /** @} */
#endif /* RT_OS_DARWIN */
} NEMCPU;
/** Pointer to NEM VMCPU instance data. */
typedef NEMCPU *PNEMCPU;

/** NEMCPU::u32Magic value. */
#define NEMCPU_MAGIC            UINT32_C(0x4d454e20)
/** NEMCPU::u32Magic value after termination. */
#define NEMCPU_MAGIC_DEAD       UINT32_C(0xdead2222)


#ifdef IN_RING0
# ifdef RT_OS_WINDOWS
/**
 * Windows: Hypercall input/ouput page info.
 */
typedef struct NEMR0HYPERCALLDATA
{
    /** Host physical address of the hypercall input/output page. */
    RTHCPHYS                    HCPhysPage;
    /** Pointer to the hypercall input/output page. */
    uint8_t                    *pbPage;
    /** Handle to the memory object of the hypercall input/output page. */
    RTR0MEMOBJ                  hMemObj;
} NEMR0HYPERCALLDATA;
/** Pointer to a Windows hypercall input/output page info. */
typedef NEMR0HYPERCALLDATA *PNEMR0HYPERCALLDATA;
# endif /* RT_OS_WINDOWS */

/**
 * NEM GVMCPU instance data.
 */
typedef struct NEMR0PERVCPU
{
# if defined(RT_OS_WINDOWS) && defined(NEM_WIN_USE_HYPERCALLS_FOR_PAGES)
    /** Hypercall input/ouput page. */
    NEMR0HYPERCALLDATA          HypercallData;
    /** Delta to add to convert a ring-0 pointer to a ring-3 one.   */
    uintptr_t                   offRing3ConversionDelta;
# else
    uint32_t                    uDummy;
# endif
} NEMR0PERVCPU;

/**
 * NEM GVM instance data.
 */
typedef struct NEMR0PERVM
{
# ifdef RT_OS_WINDOWS
#  ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    /** The partition ID. */
    uint64_t                    idHvPartition;
    /** I/O control context. */
    PSUPR0IOCTLCTX              pIoCtlCtx;
    /** Info about the VidGetHvPartitionId I/O control interface. */
    NEMWINIOCTL                 IoCtlGetHvPartitionId;
    /** Info about the VidGetPartitionProperty I/O control interface. */
    NEMWINIOCTL                 IoCtlGetPartitionProperty;
#  endif
#  ifdef NEM_WIN_WITH_RING0_RUNLOOP
    /** Info about the VidStartVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlStartVirtualProcessor;
    /** Info about the VidStopVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlStopVirtualProcessor;
    /** Info about the VidStopVirtualProcessor I/O control interface. */
    NEMWINIOCTL                 IoCtlMessageSlotHandleAndGetNext;
    /** Whether we may use the ring-0 runloop or not. */
    bool                        fMayUseRing0Runloop;
#  endif

#  ifdef NEM_WIN_USE_HYPERCALLS_FOR_PAGES
    /** Hypercall input/ouput page for non-EMT. */
    NEMR0HYPERCALLDATA          HypercallData;
    /** Critical section protecting use of HypercallData. */
    RTCRITSECT                  HypercallDataCritSect;
#  endif

# else
    uint32_t                    uDummy;
# endif
} NEMR0PERVM;

#endif /* IN_RING*/


#ifdef IN_RING3
int     nemR3NativeInit(PVM pVM, bool fFallback, bool fForced);
int     nemR3NativeInitAfterCPUM(PVM pVM);
int     nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
int     nemR3NativeTerm(PVM pVM);
void    nemR3NativeReset(PVM pVM);
void    nemR3NativeResetCpu(PVMCPU pVCpu, bool fInitIpi);
VBOXSTRICTRC    nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu);
bool            nemR3NativeCanExecuteGuest(PVM pVM, PVMCPU pVCpu);
bool            nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable);
void            nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags);
#endif

void    nemHCNativeNotifyHandlerPhysicalRegister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb);
void    nemHCNativeNotifyHandlerPhysicalModify(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                               RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM);
int     nemHCNativeNotifyPhysPageAllocated(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                           PGMPAGETYPE enmType, uint8_t *pu2State);


#ifdef RT_OS_WINDOWS
/** Maximum number of pages we can map in a single NEMR0MapPages call. */
# define NEM_MAX_MAP_PAGES      ((PAGE_SIZE - RT_UOFFSETOF(HV_INPUT_MAP_GPA_PAGES, PageList)) / sizeof(HV_SPA_PAGE_NUMBER))
/** Maximum number of pages we can unmap in a single NEMR0UnmapPages call. */
# define NEM_MAX_UNMAP_PAGES    4095

#endif
/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_NEMInternal_h */

