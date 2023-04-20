/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GICv3).
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VMM_INCLUDED_SRC_include_GICInternal_h
#define VMM_INCLUDED_SRC_include_GICInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/gic.h>
#include <VBox/vmm/pdmdev.h>


/** @defgroup grp_gic_int       Internal
 * @ingroup grp_gic
 * @internal
 * @{
 */

#define VMCPU_TO_GICCPU(a_pVCpu)             (&(a_pVCpu)->gic.s)
#define VM_TO_GIC(a_pVM)                     (&(a_pVM)->gic.s)
#define VM_TO_GICDEV(a_pVM)                  CTX_SUFF(VM_TO_GIC(a_pVM)->pGicDev)
#ifdef IN_RING3
# define VMCPU_TO_DEVINS(a_pVCpu)           ((a_pVCpu)->pVMR3->gic.s.pDevInsR3)
#elif defined(IN_RING0)
# error "Not implemented!"
#endif

/**
 * GIC PDM instance data (per-VM).
 */
typedef struct GICDEV
{
    /** The distributor MMIO handle. */
    IOMMMIOHANDLE               hMmioDist;
    /** The redistributor MMIO handle. */
    IOMMMIOHANDLE               hMmioReDist;
} GICDEV;
/** Pointer to a GIC device. */
typedef GICDEV *PGICDEV;
/** Pointer to a const GIC device. */
typedef GICDEV const *PCGICDEV;


/**
 * GIC VM Instance data.
 */
typedef struct GIC
{
    /** The ring-3 device instance. */
    PPDMDEVINSR3                pDevInsR3;
} GIC;
/** Pointer to GIC VM instance data. */
typedef GIC *PGIC;
/** Pointer to const GIC VM instance data. */
typedef GIC const *PCGIC;
AssertCompileSizeAlignment(GIC, 8);

/**
 * GIC VMCPU Instance data.
 */
typedef struct GICCPU
{
    /** @name Log Max counters
     * @{ */
    uint32_t                    cLogMaxAccessError;
    uint32_t                    cLogMaxSetApicBaseAddr;
    uint32_t                    cLogMaxGetApicBaseAddr;
    uint32_t                    uAlignment4;
    /** @} */

    /** @name APIC statistics.
     * @{ */
#ifdef VBOX_WITH_STATISTICS
    /** Number of MMIO reads in R3. */
    STAMCOUNTER                 StatMmioReadR3;
    /** Number of MMIO writes in R3. */
    STAMCOUNTER                 StatMmioWriteR3;
    /** Number of MSR reads in R3. */
    STAMCOUNTER                 StatSysRegReadR3;
    /** Number of MSR writes in R3. */
    STAMCOUNTER                 StatSysRegWriteR3;

# if 0 /* No R0 for now. */
    /** Number of MMIO reads in RZ. */
    STAMCOUNTER                 StatMmioReadRZ;
    /** Number of MMIO writes in RZ. */
    STAMCOUNTER                 StatMmioWriteRZ;
    /** Number of MSR reads in RZ. */
    STAMCOUNTER                 StatSysRegReadRZ;
    /** Number of MSR writes in RZ. */
    STAMCOUNTER                 StatSysRegWriteRZ;
# endif
#endif
    /** @} */
} GICCPU;
/** Pointer to GIC VMCPU instance data. */
typedef GICCPU *PGICCPU;
/** Pointer to a const GIC VMCPU instance data. */
typedef GICCPU const *PCGICCPU;

DECL_HIDDEN_CALLBACK(VBOXSTRICTRC)      gicDistMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb);
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC)      gicDistMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb);

DECL_HIDDEN_CALLBACK(VBOXSTRICTRC)      gicReDistMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb);
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC)      gicReDistMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb);

DECLHIDDEN(void)                        gicResetCpu(PVMCPUCC pVCpu);

DECLCALLBACK(int)                       gicR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg);
DECLCALLBACK(int)                       gicR3Destruct(PPDMDEVINS pDevIns);
DECLCALLBACK(void)                      gicR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta);
DECLCALLBACK(void)                      gicR3Reset(PPDMDEVINS pDevIns);

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_GICInternal_h */

