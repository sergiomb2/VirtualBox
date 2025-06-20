/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver VMSVGA hardware access helpers.
 */

/*
 * Copyright (C) 2016-2024 Oracle and/or its affiliates.
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaHw_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaHw_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "Svga.h"

#include <iprt/asm.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#elif defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
# include <iprt/asm-arm.h>
#else
# error "Port me"
#endif

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
DECLINLINE(RTIOPORT) SVGAPort(PVBOXWDDM_EXT_VMSVGA pSvga, uint16_t u16Offset)
{
    return pSvga->hw.ioportBase + u16Offset;
}
#endif

DECLINLINE(void) SVGAWriteIRQStatus(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32IrqStatus)
{
    if (pSvga->fMMIO)
    {
        volatile uint32_t *pu32 = &pSvga->hw.pu32MMIO[SVGA_REG_IRQ_STATUS];
        ASMAtomicWriteU32(pu32, u32IrqStatus);
        ASMCompilerBarrier();
        return;
    }

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    ASMOutU32(SVGAPort(pSvga, SVGA_IRQSTATUS_PORT), u32IrqStatus);
#endif
}

DECLINLINE(uint32_t) SVGAReadIRQStatus(PVBOXWDDM_EXT_VMSVGA pSvga)
{
    if (pSvga->fMMIO)
    {
        volatile uint32_t *pu32 = &pSvga->hw.pu32MMIO[SVGA_REG_IRQ_STATUS];
        uint32_t u32Value = ASMAtomicReadU32(pu32);
        ASMCompilerBarrier();
        return u32Value;
    }

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    return ASMInU32(SVGAPort(pSvga, SVGA_IRQSTATUS_PORT));
#else
    AssertFailedReturn(0);
#endif
}

DECLINLINE(void) SVGARegWrite(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Offset, uint32_t u32Value)
{
    if (pSvga->fMMIO)
    {
        volatile uint32_t *pu32 = &pSvga->hw.pu32MMIO[u32Offset];
        ASMAtomicWriteU32(pu32, u32Value);
        ASMCompilerBarrier();
        return;
    }

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSvga->HwSpinLock, &OldIrql);

    ASMOutU32(SVGAPort(pSvga, SVGA_INDEX_PORT), u32Offset);
    ASMOutU32(SVGAPort(pSvga, SVGA_VALUE_PORT), u32Value);

    KeReleaseSpinLock(&pSvga->HwSpinLock, OldIrql);
#endif
}

DECLINLINE(uint32_t) SVGARegRead(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Offset)
{
    uint32_t u32Value;

    if (pSvga->fMMIO)
    {
        volatile uint32_t *pu32 = &pSvga->hw.pu32MMIO[u32Offset];
        u32Value = ASMAtomicReadU32(pu32);
        ASMCompilerBarrier();
        return u32Value;
    }

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSvga->HwSpinLock, &OldIrql);

    ASMOutU32(SVGAPort(pSvga, SVGA_INDEX_PORT), u32Offset);
    u32Value = ASMInU32(SVGAPort(pSvga, SVGA_VALUE_PORT));

    KeReleaseSpinLock(&pSvga->HwSpinLock, OldIrql);
    return u32Value;
#else
    AssertFailedReturn(0);
#endif
}

DECLINLINE(uint32_t) SVGADevCapRead(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t idx)
{
    uint32_t u32Value;
    KIRQL OldIrql;

    if (pSvga->fMMIO)
    {
        KeAcquireSpinLock(&pSvga->HwSpinLock, &OldIrql);

        volatile uint32_t *pu32 = &pSvga->hw.pu32MMIO[SVGA_REG_DEV_CAP];
        ASMAtomicWriteU32(pu32, idx);
        ASMCompilerBarrier();
        u32Value = ASMAtomicReadU32(pu32);
        ASMCompilerBarrier();

        KeReleaseSpinLock(&pSvga->HwSpinLock, OldIrql);
        return u32Value;
    }

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    KeAcquireSpinLock(&pSvga->HwSpinLock, &OldIrql);

    ASMOutU32(SVGAPort(pSvga, SVGA_INDEX_PORT), SVGA_REG_DEV_CAP);
    ASMOutU32(SVGAPort(pSvga, SVGA_VALUE_PORT), idx);
    u32Value = ASMInU32(SVGAPort(pSvga, SVGA_VALUE_PORT));

    KeReleaseSpinLock(&pSvga->HwSpinLock, OldIrql);
    return u32Value;
#else
    AssertFailedReturn(0);
#endif
}

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
DECLINLINE(volatile void *) SVGAFifoPtrFromOffset(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Offset)
{
    return (volatile uint8_t *)pSvga->hw.pu32FIFO + u32Offset;
}

DECLINLINE(volatile void *) SVGAFifoPtrFromIndex(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Index)
{
    return pSvga->hw.pu32FIFO + u32Index;
}

DECLINLINE(uint32_t) SVGAFifoRead(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Index)
{
    volatile uint32_t *pu32 = &pSvga->hw.pu32FIFO[u32Index];
    return ASMAtomicReadU32(pu32);
}

DECLINLINE(void) SVGAFifoWrite(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Index, uint32_t u32Value)
{
    volatile uint32_t *pu32 = &pSvga->hw.pu32FIFO[u32Index];
    ASMAtomicWriteU32(pu32, u32Value);
    ASMCompilerBarrier();
}
#endif /* defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86) */

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_SvgaHw_h */
