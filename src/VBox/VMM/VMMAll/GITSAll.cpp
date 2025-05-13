/* $Id$ */
/** @file
 * GITS - GIC Interrupt Translation Service (ITS) - All Contexts.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_GIC
#include "GICInternal.h"

#include <VBox/log.h>
#include <VBox/gic.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/vm.h>        /* pVM->cCpus */
#include <iprt/errcore.h>       /* VINF_SUCCESS */
#include <iprt/string.h>        /* RT_ZERO */
#include <iprt/mem.h>           /* RTMemAllocZ, RTMemFree */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current GITS saved state version. */
#define GITS_SAVED_STATE_VERSION                        1

/** GITS diagnostic enum description expansion.
 * The below construct ensures typos in the input to this macro are caught
 * during compile time. */
#define GITSDIAG_DESC(a_Name)                       RT_CONCAT(kGitsDiag_, a_Name) < kGitsDiag_End ? RT_STR(a_Name) : "Ignored"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** GITS diagnostics description for members in GITSDIAG. */
static const char *const g_apszGitsDiagDesc[] =
{
    /* No error. */
    GITSDIAG_DESC(None),

    /* Command queue: basic operation errors. */
    GITSDIAG_DESC(CmdQueue_Basic_Unknown_Cmd),
    GITSDIAG_DESC(CmdQueue_Basic_Invalid_PhysAddr),

    /* Command queue: INVALL. */
    GITSDIAG_DESC(CmdQueue_Cmd_Invall_Cte_Unmapped),
    GITSDIAG_DESC(CmdQueue_Cmd_Invall_Icid_Invalid),

    /* Command: MAPV. */
    GITSDIAG_DESC(CmdQueue_Cmd_Mapc_Icid_Invalid),

    /* Command: MAPD. */
    GITSDIAG_DESC(CmdQueue_Cmd_Mapd_Size_Invalid),

    /* Command: MAPI. */
    GITSDIAG_DESC(CmdQueue_Cmd_Mapi_DevId_Unmapped),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapi_Dte_Rd_Failed),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapi_EventId_Invalid),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapi_IcId_Invalid),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapi_Ite_Wr_Failed),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapi_Lpi_Invalid),

    /* Command: MAPTI. */
    GITSDIAG_DESC(CmdQueue_Cmd_Mapti_DevId_Unmapped),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapti_Dte_Rd_Failed),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapti_EventId_Invalid),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapti_IcId_Invalid),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapti_Ite_Wr_Failed),
    GITSDIAG_DESC(CmdQueue_Cmd_Mapti_Lpi_Invalid),

    /* kGitsDiag_End */
};
AssertCompile(RT_ELEMENTS(g_apszGitsDiagDesc) == kGitsDiag_End);
#undef GITSDIAG_DESC


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

DECL_HIDDEN_CALLBACK(const char *) gitsGetCtrlRegDescription(uint16_t offReg)
{
    if (GIC_IS_REG_IN_RANGE(offReg, GITS_CTRL_REG_BASER_OFF_FIRST, GITS_CTRL_REG_BASER_RANGE_SIZE))
        return "GITS_BASER<n>";
    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:    return "GITS_CTLR";
        case GITS_CTRL_REG_IIDR_OFF:    return "GITS_IIDR";
        case GITS_CTRL_REG_TYPER_OFF:   return "GITS_TYPER";
        case GITS_CTRL_REG_MPAMIDR_OFF: return "GITS_MPAMIDR";
        case GITS_CTRL_REG_PARTIDR_OFF: return "GITS_PARTIDR";
        case GITS_CTRL_REG_MPIDR_OFF:   return "GITS_MPIDR";
        case GITS_CTRL_REG_STATUSR_OFF: return "GITS_STATUSR";
        case GITS_CTRL_REG_UMSIR_OFF:   return "GITS_UMSIR";
        case GITS_CTRL_REG_CBASER_OFF:  return "GITS_CBASER";
        case GITS_CTRL_REG_CWRITER_OFF: return "GITS_CWRITER";
        case GITS_CTRL_REG_CREADR_OFF:  return "GITS_CREADR";
        default:
            return "<UNKNOWN>";
    }
}


DECL_HIDDEN_CALLBACK(const char *) gitsGetTranslationRegDescription(uint16_t offReg)
{
    switch (offReg)
    {
        case GITS_TRANSLATION_REG_TRANSLATER:   return "GITS_TRANSLATER";
        default:
            return "<UNKNOWN>";
    }
}


static const char *gitsGetCommandName(uint8_t uCmdId)
{
    switch (uCmdId)
    {
        case GITS_CMD_ID_CLEAR:     return "CLEAR";
        case GITS_CMD_ID_DISCARD:   return "DISCARD";
        case GITS_CMD_ID_INT:       return "INT";
        case GITS_CMD_ID_INV:       return "INV";
        case GITS_CMD_ID_INVALL:    return "INVALL";
        case GITS_CMD_ID_INVDB:     return "INVDB";
        case GITS_CMD_ID_MAPC:      return "MAPC";
        case GITS_CMD_ID_MAPD:      return "MAPD";
        case GITS_CMD_ID_MAPI:      return "MAPI";
        case GITS_CMD_ID_MAPTI:     return "MAPTI";
        case GITS_CMD_ID_MOVALL:    return "MOVALL";
        case GITS_CMD_ID_MOVI:      return "MOVI";
        case GITS_CMD_ID_SYNC:      return "SYNC";
        case GITS_CMD_ID_VINVALL:   return "VINVALL";
        case GITS_CMD_ID_VMAPI:     return "VMAPI";
        case GITS_CMD_ID_VMAPP:     return "VMAPP";
        case GITS_CMD_ID_VMAPTI:    return "VMAPTI";
        case GITS_CMD_ID_VMOVI:     return "VMOVI";
        case GITS_CMD_ID_VMOVP:     return "VMOVP";
        case GITS_CMD_ID_VSGI:      return "VSGI";
        case GITS_CMD_ID_VSYNC:     return "VSYNC";
        default:
            return "<UNKNOWN>";
    }
}


DECL_FORCE_INLINE(const char *) gitsGetDiagDescription(GITSDIAG enmDiag)
{
    if (enmDiag < RT_ELEMENTS(g_apszGitsDiagDesc))
        return g_apszGitsDiagDesc[enmDiag];
    return "<Unknown>";
}


static RTGCPHYS gitsGetBaseRegPhysAddr(uint64_t uGitsBaseReg)
{
    /* Mask for physical address bits [47:12]. */
    static uint64_t const s_auPhysAddrLoMasks[] =
    {
        UINT64_C(0x0000fffffffff000), /*  4K bits[47:12] */
        UINT64_C(0x0000ffffffffc000), /* 16K bits[47:14] */
        UINT64_C(0x0000ffffffff0000), /* 64K bits[47:16] */
        UINT64_C(0x0000ffffffff0000)  /* 64K bits[47:16] */
    };

    /* Mask for physical address bits [51:48]. */
    static uint64_t const s_auPhysAddrHiMasks[] =
    {
        UINT64_C(0x0),                /*  4K bits[51:48] = 0 */
        UINT64_C(0x0),                /* 16K bits[51:48] = 0 */
        UINT64_C(0x000000000000f000), /* 64K bits[51:48] = bits[15:12] */
        UINT64_C(0x000000000000f000)  /* 64K bits[51:48] = bits[15:12] */
    };
    AssertCompile(RT_ELEMENTS(s_auPhysAddrLoMasks) == RT_ELEMENTS(s_auPhysAddrHiMasks));

    uint8_t const idxPageSize = RT_BF_GET(uGitsBaseReg, GITS_BF_CTRL_REG_BASER_PAGESIZE);
    Assert(idxPageSize < RT_ELEMENTS(s_auPhysAddrLoMasks));
    RTGCPHYS const GCPhys =  (uGitsBaseReg & s_auPhysAddrLoMasks[idxPageSize])
                          | ((uGitsBaseReg & s_auPhysAddrHiMasks[idxPageSize]) << (48 - 12));
    return GCPhys;
}


static void gitsCmdQueueSetError(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, GITSDIAG enmDiag, bool fStallQueue)
{
    Log4Func(("enmDiag=%#RX32 (%s) fStallQueue=%RTbool\n", enmDiag, gitsGetDiagDescription(enmDiag)));

    GIC_CRIT_SECT_ENTER(pDevIns);

    /* Record the error and stall the queue. */
    pGitsDev->enmDiag = enmDiag;
    pGitsDev->cCmdQueueErrors++;
    if (fStallQueue)
        pGitsDev->uCmdReadReg |= GITS_BF_CTRL_REG_CREADR_STALLED_MASK;

    GIC_CRIT_SECT_LEAVE(pDevIns);

    /* Since we don't support SEIs, so there should be nothing more to do here. */
    Assert(!RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_SEIS));
}


DECL_FORCE_INLINE(bool) gitsCmdQueueIsEmptyEx(PCGITSDEV pGitsDev, uint32_t *poffRead, uint32_t *poffWrite)
{
    *poffRead  = pGitsDev->uCmdReadReg  & GITS_BF_CTRL_REG_CREADR_OFFSET_MASK;
    *poffWrite = pGitsDev->uCmdWriteReg & GITS_BF_CTRL_REG_CWRITER_OFFSET_MASK;
    return *poffRead == *poffWrite;
}


DECL_FORCE_INLINE(bool) gitsCmdQueueIsEmpty(PCGITSDEV pGitsDev)
{
    uint32_t offRead;
    uint32_t offWrite;
    return gitsCmdQueueIsEmptyEx(pGitsDev, &offRead, &offWrite);
}


DECL_FORCE_INLINE(bool) gitsCmdQueueCanProcessRequests(PCGITSDEV pGitsDev)
{
    if (    (pGitsDev->uTypeReg.u    & GITS_BF_CTRL_REG_CTLR_ENABLED_MASK)
        &&  (pGitsDev->uCmdBaseReg.u & GITS_BF_CTRL_REG_CBASER_VALID_MASK)
        && !(pGitsDev->uCmdReadReg   & GITS_BF_CTRL_REG_CREADR_STALLED_MASK))
        return true;
    return false;
}


static void gitsCmdQueueThreadWakeUpIfNeeded(PPDMDEVINS pDevIns, PGITSDEV pGitsDev)
{
    Log4Func(("\n"));
    Assert(GIC_CRIT_SECT_IS_OWNER(pDevIns));
    if (    gitsCmdQueueCanProcessRequests(pGitsDev)
        && !gitsCmdQueueIsEmpty(pGitsDev))
    {
        Log4Func(("Waking up command-queue thread\n"));
        int const rc = PDMDevHlpSUPSemEventSignal(pDevIns, pGitsDev->hEvtCmdQueue);
        AssertRC(rc);
    }
}


DECL_HIDDEN_CALLBACK(uint64_t) gitsMmioReadCtrl(PCGITSDEV pGitsDev, uint16_t offReg, unsigned cb)
{
    Assert(cb == 4 || cb == 8);
    Assert(!(offReg & 3));
    RT_NOREF(cb);

    /*
     * GITS_BASER<n>.
     */
    uint64_t uReg;
    if (GIC_IS_REG_IN_RANGE(offReg, GITS_CTRL_REG_BASER_OFF_FIRST, GITS_CTRL_REG_BASER_RANGE_SIZE))
    {
        uint16_t const cbReg  = sizeof(uint64_t);
        uint16_t const idxReg = (offReg - GITS_CTRL_REG_BASER_OFF_FIRST) / cbReg;
        uReg = pGitsDev->aItsTableRegs[idxReg].u >> ((offReg & 7) << 3 /* to bits */);
        return uReg;
    }

    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:
            Assert(cb == 4);
            uReg = pGitsDev->uCtrlReg;
            break;

        case GITS_CTRL_REG_PIDR2_OFF:
            Assert(cb == 4);
            Assert(pGitsDev->uArchRev <= GITS_CTRL_REG_PIDR2_ARCHREV_GICV4);
            uReg = RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_DES_1,   GIC_JEDEC_JEP10_DES_1(GIC_JEDEC_JEP106_IDENTIFICATION_CODE))
                 | RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_JEDEC,   1)
                 | RT_BF_MAKE(GITS_BF_CTRL_REG_PIDR2_ARCHREV, pGitsDev->uArchRev);
            break;

        case GITS_CTRL_REG_IIDR_OFF:
            Assert(cb == 4);
            uReg = RT_BF_MAKE(GITS_BF_CTRL_REG_IIDR_IMPL_ID_CODE,   GIC_JEDEC_JEP106_IDENTIFICATION_CODE)
                 | RT_BF_MAKE(GITS_BF_CTRL_REG_IIDR_IMPL_CONT_CODE, GIC_JEDEC_JEP106_CONTINUATION_CODE);
            break;

        case GITS_CTRL_REG_TYPER_OFF:
        case GITS_CTRL_REG_TYPER_OFF + 4:
            uReg = pGitsDev->uTypeReg.u >> ((offReg & 7) << 3 /* to bits */);
            break;

        case GITS_CTRL_REG_CBASER_OFF:
            uReg = pGitsDev->uCmdBaseReg.u;
            break;

        case GITS_CTRL_REG_CBASER_OFF + 4:
            Assert(cb == 4);
            uReg = pGitsDev->uCmdBaseReg.s.Hi;
            break;

        case GITS_CTRL_REG_CREADR_OFF:
            uReg = pGitsDev->uCmdReadReg;
            break;

        case GITS_CTRL_REG_CREADR_OFF + 4:
            uReg = 0;   /* Upper 32-bits are reserved, MBZ. */
            break;

        case GITS_CTRL_REG_CWRITER_OFF:
            uReg = pGitsDev->uCmdWriteReg;
            break;

        case GITS_CTRL_REG_CWRITER_OFF + 4:
            uReg = 0;   /* Upper 32-bits are reserved, MBZ. */
            break;

        default:
            AssertReleaseMsgFailed(("offReg=%#x (%s)\n", offReg, gitsGetCtrlRegDescription(offReg)));
            uReg = 0;
            break;
    }

    Log4Func(("offReg=%#RX16 (%s) uReg=%#RX64 [%u-bit]\n", offReg, gitsGetCtrlRegDescription(offReg), uReg, cb << 3));
    return uReg;
}


DECL_HIDDEN_CALLBACK(uint64_t) gitsMmioReadTranslate(PCGITSDEV pGitsDev, uint16_t offReg, unsigned cb)
{
    Assert(cb == 8 || cb == 4);
    Assert(!(offReg & 3));
    RT_NOREF(pGitsDev, cb);

    uint64_t uReg = 0;
    AssertReleaseMsgFailed(("offReg=%#x (%s) uReg=%#RX64 [%u-bit]\n", offReg, gitsGetTranslationRegDescription(offReg), uReg, cb << 3));
    return uReg;
}


DECL_HIDDEN_CALLBACK(void) gitsMmioWriteCtrl(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, uint16_t offReg, uint64_t uValue, unsigned cb)
{
    Assert(cb == 8 || cb == 4);
    Assert(!(offReg & 3));
    Log4Func(("offReg=%u uValue=%#RX64 cb=%u\n", offReg, uValue, cb));

    /*
     * GITS_BASER<n>.
     */
    if (GIC_IS_REG_IN_RANGE(offReg, GITS_CTRL_REG_BASER_OFF_FIRST, GITS_CTRL_REG_BASER_RANGE_SIZE))
    {
        uint16_t const cbReg   = sizeof(uint64_t);
        uint16_t const idxReg  = (offReg - GITS_CTRL_REG_BASER_OFF_FIRST) / cbReg;
        uint64_t const fRwMask = GITS_CTRL_REG_BASER_RW_MASK;
        if (!(offReg & 7))
        {
            if (cb == 8)
                GIC_SET_REG_U64_FULL(pGitsDev->aItsTableRegs[idxReg].u, uValue, fRwMask);
            else
                GIC_SET_REG_U64_LO(pGitsDev->aItsTableRegs[idxReg].s.Lo, uValue, fRwMask);
        }
        else
        {
            Assert(cb == 4);
            GIC_SET_REG_U64_HI(pGitsDev->aItsTableRegs[idxReg].s.Hi, uValue, fRwMask);
        }
        /** @todo Clear ITS caches when GITS_BASER<n>.Valid = 0. */
        return;
    }

    switch (offReg)
    {
        case GITS_CTRL_REG_CTLR_OFF:
            Assert(cb == 4);
            Assert(!(pGitsDev->uTypeReg.u & GITS_BF_CTRL_REG_TYPER_UMSI_IRQ_MASK));
            GIC_SET_REG_U32(pGitsDev->uCtrlReg, uValue, GITS_BF_CTRL_REG_CTLR_RW_MASK);
            if (RT_BF_GET(uValue, GITS_BF_CTRL_REG_CTLR_ENABLED))
                pGitsDev->uCtrlReg &= ~GITS_BF_CTRL_REG_CTLR_QUIESCENT_MASK;
            else
            {
                pGitsDev->uCtrlReg |=  GITS_BF_CTRL_REG_CTLR_QUIESCENT_MASK;
                /** @todo Clear ITS caches. */
            }
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        case GITS_CTRL_REG_CBASER_OFF:
            if (cb == 8)
                GIC_SET_REG_U64_FULL(pGitsDev->uCmdBaseReg.u, uValue, GITS_CTRL_REG_CBASER_RW_MASK);
            else
                GIC_SET_REG_U64_LO(pGitsDev->uCmdBaseReg.s.Lo, uValue, GITS_CTRL_REG_CBASER_RW_MASK);
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        case GITS_CTRL_REG_CBASER_OFF + 4:
            Assert(cb == 4);
            GIC_SET_REG_U64_HI(pGitsDev->uCmdBaseReg.s.Hi, uValue, GITS_CTRL_REG_CBASER_RW_MASK);
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        case GITS_CTRL_REG_CWRITER_OFF:
            GIC_SET_REG_U32(pGitsDev->uCmdWriteReg, uValue, GITS_CTRL_REG_CWRITER_RW_MASK);
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        case GITS_CTRL_REG_CWRITER_OFF + 4:
            /* Upper 32-bits are all reserved, ignore write. Fedora 40 arm64 guests (and probably others) do this. */
            Assert(uValue == 0);
            gitsCmdQueueThreadWakeUpIfNeeded(pDevIns, pGitsDev);
            break;

        default:
            AssertReleaseMsgFailed(("offReg=%#x (%s) uValue=%#RX32\n", offReg, gitsGetCtrlRegDescription(offReg), uValue));
            break;
    }

    Log4Func(("offReg=%#RX16 (%s) uValue=%#RX32 [%u-bit]\n", offReg, gitsGetCtrlRegDescription(offReg), uValue, cb << 3));
}


DECL_HIDDEN_CALLBACK(void) gitsMmioWriteTranslate(PGITSDEV pGitsDev, uint16_t offReg, uint64_t uValue, unsigned cb)
{
    RT_NOREF(pGitsDev);
    Assert(cb == 8 || cb == 4);
    Assert(!(offReg & 3));
    Log4Func(("offReg=%u uValue=%#RX64 cb=%u\n", offReg, uValue, cb));
    /** @todo Call gitsSetLpi for GITS_TRANSLATER register offset write. */
    AssertReleaseMsgFailed(("offReg=%#x uValue=%#RX64 [%u-bit]\n", offReg, uValue, cb << 3));
}


DECL_HIDDEN_CALLBACK(void) gitsInit(PGITSDEV pGitsDev)
{
    Log4Func(("\n"));

    /* GITS_CTLR.*/
    pGitsDev->uCtrlReg = RT_BF_MAKE(GITS_BF_CTRL_REG_CTLR_QUIESCENT, 1);

    /* GITS_TYPER. */
    pGitsDev->uTypeReg.u = RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_PHYSICAL,  1)     /* Physical LPIs supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VIRTUAL,   0) */  /* Virtual LPIs not supported. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CCT,       0)     /* Collections in memory not supported. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_ITT_ENTRY_SIZE, sizeof(GITSITE) - 1) /* ITE size in bytes minus 1. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_ID_BITS,   31)    /* 32-bit event IDs. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_DEV_BITS,  31)    /* 32-bit device IDs. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_SEIS,      0) */  /* Locally generated errors not recommended. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_PTA,       0) */  /* Target is VCPU ID not address. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_HCC,       255)   /* Collection count. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CID_BITS,  0)     /* Collections in memory not supported. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_CIL,       0)     /* Collections in memory not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VMOVP,     0) */  /* VMOVP not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_MPAM,      0) */  /* MPAM no supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VSGI,      0) */  /* VSGI not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_VMAPP,     0) */  /* VMAPP not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_SVPET,     0) */  /* SVPET not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_NID,       0) */  /* NID (doorbell) not supported. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_UMSI,      0) */  /** @todo Reporting receipt of unmapped MSIs. */
                       /*| RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_UMSI_IRQ,  0) */  /** @todo Generating interrupt on unmapped MSI. */
                         | RT_BF_MAKE(GITS_BF_CTRL_REG_TYPER_INV,       1);    /* ITS caches invalidated when clearing
                                                                                  GITS_CTLR.Enabled and GITS_BASER<n>.Valid. */
    Assert(RT_ELEMENTS(pGitsDev->aCtes) >= RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_HCC));

    /* GITS_BASER<n>. */
    RT_ZERO(pGitsDev->aItsTableRegs);
    pGitsDev->aItsTableRegs[0].u = RT_BF_MAKE(GITS_BF_CTRL_REG_BASER_ENTRY_SIZE, sizeof(GITSDTE) - 1)
                                 | RT_BF_MAKE(GITS_BF_CTRL_REG_BASER_TYPE,       GITS_BASER_TYPE_DEVICES);

    /* GITS_CBASER, GITS_CREADR, GITS_CWRITER. */
    pGitsDev->uCmdBaseReg.u = 0;
    pGitsDev->uCmdReadReg   = 0;
    pGitsDev->uCmdWriteReg  = 0;

    /* Collection Table. */
    for (unsigned i = 0; i < RT_ELEMENTS(pGitsDev->aCtes); i++)
        pGitsDev->aCtes[i].idTargetCpu = NIL_VMCPUID;

    /* Misc. stuff. */
    pGitsDev->cCmdQueueErrors = 0;
}


#ifdef IN_RING3
DECL_HIDDEN_CALLBACK(void) gitsR3DbgInfo(PCGITSDEV pGitsDev, PCDBGFINFOHLP pHlp)
{
    pHlp->pfnPrintf(pHlp, "GIC ITS:\n");

    /* Basic info, GITS_CTLR and GITS_TYPER. */
    {
        uint32_t const uCtrlReg = pGitsDev->uCtrlReg;
        GITSDIAG const enmDiag  = pGitsDev->enmDiag;
        pHlp->pfnPrintf(pHlp, "  uArchRev           = %u\n",          pGitsDev->uArchRev);
        pHlp->pfnPrintf(pHlp, "  Cmd queue errors   = %RU64\n",       pGitsDev->cCmdQueueErrors);
        pHlp->pfnPrintf(pHlp, "  Last error         = %#RX32 (%s)\n", enmDiag, gitsGetDiagDescription(enmDiag));
        pHlp->pfnPrintf(pHlp, "  GITS_CTLR          = %#RX32\n",      uCtrlReg);
        pHlp->pfnPrintf(pHlp, "    Enabled            = %RTbool\n",   RT_BF_GET(uCtrlReg, GITS_BF_CTRL_REG_CTLR_ENABLED));
        pHlp->pfnPrintf(pHlp, "    UMSI IRQ           = %RTbool\n",   RT_BF_GET(uCtrlReg, GITS_BF_CTRL_REG_CTLR_UMSI_IRQ));
        pHlp->pfnPrintf(pHlp, "    Quiescent          = %RTbool\n",   RT_BF_GET(uCtrlReg, GITS_BF_CTRL_REG_CTLR_QUIESCENT));
    }

    /* GITS_BASER<n>. */
    for (unsigned i = 0; i < RT_ELEMENTS(pGitsDev->aItsTableRegs); i++)
    {
        static uint32_t const s_acbPageSize[] = { _4K, _16K, _64K, _64K };
        static const char* const s_apszType[] = { "UnImpl", "Devices", "vPEs", "Intr Collections" };
        uint64_t const uReg    = pGitsDev->aItsTableRegs[i].u;
        bool const     fValid  = RT_BOOL(RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_VALID));
        uint8_t const  idxType = RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_TYPE);
        if (   fValid
            || idxType != GITS_BASER_TYPE_UNIMPL)
        {
            uint16_t const uSize       = RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_SIZE);
            uint16_t const cPages      = uSize > 0 ? uSize + 1 : 0;
            uint8_t const  idxPageSize = RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_PAGESIZE);
            uint64_t const cbItsTable  = cPages * s_acbPageSize[idxPageSize];
            uint8_t const  uEntrySize  = RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_ENTRY_SIZE);
            bool const     fIndirect   = RT_BOOL(RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_INDIRECT));
            const char *pszType        = s_apszType[idxType];
            pHlp->pfnPrintf(pHlp, "  GITS_BASER[%u]      = %#RX64\n", i, uReg);
            pHlp->pfnPrintf(pHlp, "    Size               = %#x (pages=%u total=%.Rhcb)\n", uSize, cPages, cbItsTable);
            pHlp->pfnPrintf(pHlp, "    Page size          = %#x (%.Rhcb)\n", idxPageSize, s_acbPageSize[idxPageSize]);
            pHlp->pfnPrintf(pHlp, "    Shareability       = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_SHAREABILITY));
            pHlp->pfnPrintf(pHlp, "    Phys addr          = %#RX64 (addr=%#RX64)\n", uReg & GITS_BF_CTRL_REG_BASER_PHYS_ADDR_MASK,
                                                                                     gitsGetBaseRegPhysAddr(uReg));
            pHlp->pfnPrintf(pHlp, "    Entry size         = %#x (%u bytes)\n", uEntrySize, uEntrySize > 0 ? uEntrySize + 1 : 0);
            pHlp->pfnPrintf(pHlp, "    Outer cache        = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_OUTER_CACHE));
            pHlp->pfnPrintf(pHlp, "    Type               = %#x (%s)\n", idxType, pszType);
            pHlp->pfnPrintf(pHlp, "    Inner cache        = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_BASER_INNER_CACHE));
            pHlp->pfnPrintf(pHlp, "    Indirect           = %RTbool\n",  fIndirect);
            pHlp->pfnPrintf(pHlp, "    Valid              = %RTbool\n",  fValid);
        }
    }

    /* GITS_CBASER. */
    {
        uint64_t const uReg   = pGitsDev->uCmdBaseReg.u;
        uint8_t const  uSize  = RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_SIZE);
        uint16_t const cPages = uSize > 0 ? uSize + 1 : 0;
        pHlp->pfnPrintf(pHlp, "  GITS_CBASER        = %#RX64\n", uReg);
        pHlp->pfnPrintf(pHlp, "    Size               = %#x (pages=%u total=%.Rhcb)\n", uSize, cPages, _4K * cPages);
        pHlp->pfnPrintf(pHlp, "    Shareability       = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_SHAREABILITY));
        pHlp->pfnPrintf(pHlp, "    Phys addr          = %#RX64\n",   uReg & GITS_BF_CTRL_REG_CBASER_PHYS_ADDR_MASK);
        pHlp->pfnPrintf(pHlp, "    Outer cache        = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_OUTER_CACHE));
        pHlp->pfnPrintf(pHlp, "    Inner cache        = %#x\n",      RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_INNER_CACHE));
        pHlp->pfnPrintf(pHlp, "    Valid              = %RTbool\n",  RT_BF_GET(uReg, GITS_BF_CTRL_REG_CBASER_VALID));
    }

    /* GITS_CREADR. */
    {
        uint32_t const uReg = pGitsDev->uCmdReadReg;
        pHlp->pfnPrintf(pHlp, "  GITS_CREADR        = 0x%05RX32 (stalled=%RTbool offset=%RU32)\n", uReg,
                        RT_BF_GET(uReg, GITS_BF_CTRL_REG_CREADR_STALLED), uReg & GITS_BF_CTRL_REG_CREADR_OFFSET_MASK);
    }

    /* GITS_CWRITER. */
    {
        uint32_t const uReg = pGitsDev->uCmdWriteReg;
        pHlp->pfnPrintf(pHlp, "  GITS_CWRITER       = 0x%05RX32 (  retry=%RTbool offset=%RU32)\n", uReg,
                        RT_BF_GET(uReg, GITS_BF_CTRL_REG_CWRITER_RETRY), uReg & GITS_BF_CTRL_REG_CWRITER_OFFSET_MASK);
    }

    /* Interrupt Collection Table. */
    {
        pHlp->pfnPrintf(pHlp, "  Collection Table:\n");
        bool fHasValidCtes = false;
        for (unsigned i = 0; i < RT_ELEMENTS(pGitsDev->aCtes); i++)
        {
            VMCPUID const idTargetCpu = pGitsDev->aCtes[i].idTargetCpu;
            if (idTargetCpu != NIL_VMCPUID)
            {
                pHlp->pfnPrintf(pHlp, "    [%3u] = %RU32\n", i, idTargetCpu);
                fHasValidCtes = true;
            }
        }
        if (!fHasValidCtes)
            pHlp->pfnPrintf(pHlp, "    Empty (no valid entries)\n");
    }
}


#if 0
static void gitsR3DteCacheAdd(PGITSDEV pGitsDev, uint32_t uDevId, RTGCPHYS GCPhysItt, uint8_t cDevIdBits, PGITSDTE pDte)
{
    pDte->fValid    = 1;
    pDte->afPadding = 0;
    pDte->uDevId    = uDevId;
    pDte->GCPhysItt = GCPhysItt;
    pDte->cbItt     = RT_BIT_32(cDevIdBits) - 1;

    unsigned idxFree = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(pGitsDev->aDtes); i++)
    {
        PCGITSDTE pCurDte = &pGitsDev->aDtes[i];
        if (!pCurDte->fValid)
        {
            idxFree = i;
            break;
        }
    }
    memcpy(&pGitsDev->aDtes[idxFree], pDte, sizeof(*pDte));
}


static void gitsR3DteCacheRemove(PGITSDEV pGitsDev, uint32_t uDevId)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pGitsDev->aDtes); i++)
    {
        PGITSDTE pCurDte = &pGitsDev->aDtes[i];
        if (pCurDte->uDevId == uDevId)
        {
            RT_ZERO(*pCurDte);
            return;
        }
    }
}
#endif


static int gitsR3DteGetAddr(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, uint32_t uDevId, PRTGCPHYS pGCPhysDte)
{
    uint64_t const uBaseReg       = pGitsDev->aItsTableRegs[0].u;
    bool const     fIndirect      = RT_BF_GET(uBaseReg, GITS_BF_CTRL_REG_BASER_INDIRECT);
    RTGCPHYS       GCPhysDevTable = gitsGetBaseRegPhysAddr(uBaseReg);
    if (!fIndirect)
    {
        *pGCPhysDte = GCPhysDevTable + uDevId * sizeof(GITSDTE);
        return VINF_SUCCESS;
    }

    RTGCPHYS offDte = 0;
    static uint32_t const s_acbPageSizes[]    = { _4K, _16K, _64K, _64K };
    static uint64_t const s_auPhysAddrMasks[] =
    {
        UINT64_C(0x000ffffffffff000), /*  4K bits[51:12] */
        UINT64_C(0x000fffffffffc000), /* 16K bits[51:14] */
        UINT64_C(0x000fffffffff0000), /* 64K bits[51:16] */
        UINT64_C(0x000fffffffff0000)  /* 64K bits[51:16] */
    };

    uint8_t const  idxPageSize = RT_BF_GET(uBaseReg, GITS_BF_CTRL_REG_BASER_PAGESIZE);
    uint32_t const cbPage      = s_acbPageSizes[idxPageSize];

    /* Read the the level 1 table device-table entry. */
    uint32_t const cLevel1Entries = cbPage / GITS_ITE_INDIRECT_LVL1_SIZE;
    RTGCPHYS const offLevel1Dte   = (uDevId % cLevel1Entries) * GITS_ITE_INDIRECT_LVL1_SIZE;
    uint64_t       uLevel1Dte     = 0;
    int rc = PDMDevHlpPhysReadMeta(pDevIns, GCPhysDevTable + offLevel1Dte, &uLevel1Dte, sizeof(uLevel1Dte));
    if (RT_SUCCESS(rc))
    {
        /* Check if the entry is valid. */
        bool const fValid = RT_BF_GET(uLevel1Dte, GITS_BF_ITE_INDIRECT_LVL1_4K_VALID);
        if (fValid)
        {
            /* Compute the physical address of the device-table entry from the level 1 entry. */
            uint32_t const cEntries = cbPage / sizeof(GITSDTE);
            GCPhysDevTable          = uLevel1Dte & s_auPhysAddrMasks[idxPageSize];
            offDte                  = (uDevId % cEntries) * sizeof(GITSDTE);

            *pGCPhysDte = GCPhysDevTable + offDte;
            return VINF_SUCCESS;
        }
        rc = VERR_NOT_FOUND;
    }

    /* Something went wrong (usually shouldn't happen but could be faulty/misbehaving guest). */
    *pGCPhysDte = NIL_RTGCPHYS;
    return rc;
}


static int gitsR3DteRead(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, uint32_t uDevId, GITSDTE *puDte)
{
    RTGCPHYS GCPhysDte;
    int const rc = gitsR3DteGetAddr(pDevIns, pGitsDev, uDevId, &GCPhysDte);
    if (RT_SUCCESS(rc))
        return PDMDevHlpPhysReadMeta(pDevIns, GCPhysDte, (void *)puDte, sizeof(*puDte));
    AssertMsgFailed(("Failed to get device-table entry address for device ID %#RX32 rc=%Rrc\n", uDevId, rc));
    return rc;
}


static int gitsR3DteWrite(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, uint32_t uDevId, GITSDTE uDte)
{
    RTGCPHYS GCPhysDte;
    int const rc = gitsR3DteGetAddr(pDevIns, pGitsDev, uDevId, &GCPhysDte);
    if (RT_SUCCESS(rc))
        return PDMDevHlpPhysWriteMeta(pDevIns, GCPhysDte, (const void *)&uDte, sizeof(uDte));
    AssertMsgFailed(("Failed to get device-table entry address for device ID %#RX32 rc=%Rrc\n", uDevId, rc));
    return rc;
}


static int gitsR3IteRead(PPDMDEVINS pDevIns, GITSDTE uDte, uint32_t uEventId, GITSITE *puIte)
{
    RTGCPHYS const GCPhysIntrTable = uDte & GITS_BF_DTE_ITT_ADDR_MASK;
    RTGCPHYS const GCPhysIte       = GCPhysIntrTable + uEventId * sizeof(GITSITE);
    return PDMDevHlpPhysReadMeta(pDevIns, GCPhysIte, (void *)puIte, sizeof(*puIte));
}


static int gitsR3IteWrite(PPDMDEVINS pDevIns, GITSDTE uDte, uint32_t uEventId, GITSITE uIte)
{
    RTGCPHYS const GCPhysIntrTable = uDte & GITS_BF_DTE_ITT_ADDR_MASK;
    RTGCPHYS const GCPhysIte       = GCPhysIntrTable + uEventId * sizeof(GITSITE);
    return PDMDevHlpPhysWriteMeta(pDevIns, GCPhysIte, (const void *)&uIte, sizeof(uIte));
}


static void gitsR3CmdMapIntr(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, uint32_t uDevId, uint32_t uEventId, uint16_t uIntId,
                             uint16_t uIcId, bool fMapti)
{
#define GITS_CMD_QUEUE_SET_ERR_RET(a_enmDiagSuffix) \
    do \
    { \
        gitsCmdQueueSetError(pDevIns, pGitsDev, \
                             fMapti ? kGitsDiag_CmdQueue_Cmd_ ## Mapti_ ## a_enmDiagSuffix \
                                    : kGitsDiag_CmdQueue_Cmd_ ## Mapi_  ## a_enmDiagSuffix, false /* fStall */); \
        return; \
    } while (0)

    /* We support 32-bits of device ID and hence it cannot be out of range (asserted below). */
    Assert(sizeof(uDevId) * 8 >= RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_DEV_BITS) + 1);

    /* Validate ICID. */
    if (uIcId < RT_ELEMENTS(pGitsDev->aCtes))
    { /* likely */ }
    else
        GITS_CMD_QUEUE_SET_ERR_RET(IcId_Invalid);

    /* Validate LPI INTID. */
    if (gicDistIsLpiValid(pDevIns, uIntId))
    { /* likely */ }
    else
        GITS_CMD_QUEUE_SET_ERR_RET(Lpi_Invalid);

    /* Read the device-table entry. */
    GITSDTE uDte = 0;
    int rc = gitsR3DteRead(pDevIns, pGitsDev, uDevId, &uDte);
    if (RT_SUCCESS(rc))
    { /* likely */ }
    else
        GITS_CMD_QUEUE_SET_ERR_RET(Dte_Rd_Failed);

    /* Check that the device ID mapping is valid. */
    bool const fValid = RT_BF_GET(uDte, GITS_BF_DTE_VALID);
    if (fValid)
    { /* likely */ }
    else
        GITS_CMD_QUEUE_SET_ERR_RET(DevId_Unmapped);

    /* Check that the event ID (which is the index) is within range. */
    uint32_t const cEntries = RT_BIT_32(RT_BF_GET(uDte, GITS_BF_DTE_ITT_ADDR) + 1);
    if (uEventId < cEntries)
    {
        /* Write the interrupt-translation entry mapping event ID with INTID and ICID. */
        GITSITE const uIte = RT_BF_MAKE(GITS_BF_ITE_ICID,    uIcId)
                           | RT_BF_MAKE(GITS_BF_ITE_INTID,   uIntId)
                           | RT_BF_MAKE(GITS_BF_ITE_IS_PHYS, 1)
                           | RT_BF_MAKE(GITS_BF_ITE_VALID,   1);
        rc = gitsR3IteWrite(pDevIns, uDte, uEventId, uIte);
        if (RT_SUCCESS(rc))
            return;

        GITS_CMD_QUEUE_SET_ERR_RET(Ite_Wr_Failed);
    }
    else
        GITS_CMD_QUEUE_SET_ERR_RET(EventId_Invalid);

#undef GITS_CMD_QUEUE_SET_ERR_RET
}


DECL_HIDDEN_CALLBACK(int) gitsR3CmdQueueProcess(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, void *pvBuf, uint32_t cbBuf)
{
    Log4Func(("cbBuf=%RU32\n", cbBuf));

    /* Hold the critical section as we could be accessing the device state simultaneously with MMIO accesses. */
    GIC_CRIT_SECT_ENTER(pDevIns);

    if (gitsCmdQueueCanProcessRequests(pGitsDev))
    {
        uint32_t   offRead;
        uint32_t   offWrite;
        bool const fIsEmpty = gitsCmdQueueIsEmptyEx(pGitsDev, &offRead, &offWrite);
        if (!fIsEmpty)
        {
            uint32_t const cCmdQueuePages = RT_BF_GET(pGitsDev->uCmdBaseReg.u, GITS_BF_CTRL_REG_CBASER_SIZE) + 1;
            uint32_t const cbCmdQueue     = cCmdQueuePages << GITS_CMD_QUEUE_PAGE_SHIFT;
            AssertRelease(cbCmdQueue <= cbBuf); /** @todo Paranoia; make this a debug assert later. */

            /*
             * Read all the commands from guest memory into our command queue buffer.
             */
            int      rc;
            uint32_t cbCmds;
            RTGCPHYS const GCPhysCmds = pGitsDev->uCmdBaseReg.u & GITS_BF_CTRL_REG_CBASER_PHYS_ADDR_MASK;

            /* Leave the critical section while reading (a potentially large number of) commands from guest memory. */
            GIC_CRIT_SECT_LEAVE(pDevIns);

            if (offWrite > offRead)
            {
                /* The write offset has not wrapped around, read them in one go. */
                cbCmds = offWrite - offRead;
                Assert(cbCmds <= cbBuf);
                rc = PDMDevHlpPhysReadMeta(pDevIns, GCPhysCmds + offRead, pvBuf, cbCmds);
            }
            else
            {
                /* The write offset has wrapped around, read till end of buffer followed by wrapped-around data. */
                uint32_t const cbForward = cbCmdQueue - offRead;
                uint32_t const cbWrapped = offWrite;
                Assert(cbForward + cbWrapped <= cbBuf);
                rc = PDMDevHlpPhysReadMeta(pDevIns, GCPhysCmds + offRead, pvBuf, cbForward);
                if (   RT_SUCCESS(rc)
                    && cbWrapped > 0)
                    rc = PDMDevHlpPhysReadMeta(pDevIns, GCPhysCmds, (void *)((uintptr_t)pvBuf + cbForward), cbWrapped);
                cbCmds = cbForward + cbWrapped;
            }

            /*
             * Process the commands in the buffer.
             */
            if (RT_SUCCESS(rc))
            {
                /* Indicate to the guest we've fetched all commands. */
                GIC_CRIT_SECT_ENTER(pDevIns);
                pGitsDev->uCmdReadReg   = offWrite;
                pGitsDev->uCmdWriteReg &= ~GITS_BF_CTRL_REG_CWRITER_RETRY_MASK;

                /* Don't hold the critical section while processing commands. */
                GIC_CRIT_SECT_LEAVE(pDevIns);

                uint32_t const cCmds = cbCmds / sizeof(GITSCMD);
                for (uint32_t idxCmd = 0; idxCmd < cCmds; idxCmd++)
                {
                    PCGITSCMD pCmd = (PCGITSCMD)((uintptr_t)pvBuf + (idxCmd * sizeof(GITSCMD)));
                    uint8_t const uCmdId = pCmd->common.uCmdId;
                    switch (uCmdId)
                    {
                        case GITS_CMD_ID_MAPC:
                        {
                            /* Map interrupt collection with a target CPU ID. */
                            uint64_t const uDw2 = pCmd->au64[2].u;
                            uint8_t  const fValid       = RT_BF_GET(uDw2, GITS_BF_CMD_MAPC_DW2_VALID);
                            uint16_t const uTargetCpuId = RT_BF_GET(uDw2, GITS_BF_CMD_MAPC_DW2_RDBASE);
                            uint16_t const uIcId        = RT_BF_GET(uDw2, GITS_BF_CMD_MAPC_DW2_IC_ID);

                            if (RT_LIKELY(uIcId < RT_ELEMENTS(pGitsDev->aCtes)))
                            {
                                GIC_CRIT_SECT_ENTER(pDevIns);
                                Assert(!RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_PTA));
                                pGitsDev->aCtes[uIcId].idTargetCpu = fValid ? uTargetCpuId : NIL_VMCPUID;
                                GIC_CRIT_SECT_LEAVE(pDevIns);
                            }
                            else
                                gitsCmdQueueSetError(pDevIns, pGitsDev, kGitsDiag_CmdQueue_Cmd_Mapc_Icid_Invalid,
                                                     false /* fStall */);
                            STAM_COUNTER_INC(&pGitsDev->StatCmdMapc);
                            break;
                        }

                        case GITS_CMD_ID_MAPD:
                        {
                            /* Map device ID to an interrupt translation table. */
                            uint32_t const uDevId     = RT_BF_GET(pCmd->au64[0].u, GITS_BF_CMD_MAPD_DW0_DEV_ID);
                            uint8_t const  cDevIdBits = RT_BF_GET(pCmd->au64[1].u, GITS_BF_CMD_MAPD_DW1_SIZE);
                            bool const     fValid     = RT_BF_GET(pCmd->au64[2].u, GITS_BF_CMD_MAPD_DW2_VALID);
                            RTGCPHYS const GCPhysItt  = pCmd->au64[2].u & GITS_BF_CMD_MAPD_DW2_ITT_ADDR_MASK;
                            if (fValid)
                            {
                                /* We support 32-bits of device ID and hence it cannot be out of range (asserted below). */
                                Assert(sizeof(uDevId) * 8 >= RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_DEV_BITS) + 1);

                                /* Check that size is within the supported event ID range. */
                                uint8_t const cEventIdBits = RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_ID_BITS) + 1;
                                if (cDevIdBits <= cEventIdBits)
                                {
                                    GITSDTE const uDte = RT_BF_MAKE(GITS_BF_DTE_VALID,     1)
                                                       | RT_BF_MAKE(GITS_BF_DTE_ITT_RANGE, cDevIdBits)
                                                       | (GCPhysItt & GITS_BF_DTE_ITT_ADDR_MASK);

                                    GIC_CRIT_SECT_ENTER(pDevIns);
                                    rc = gitsR3DteWrite(pDevIns, pGitsDev, uDevId, uDte);
                                    /** @todo Add Device ID to internal cache. */
                                    GIC_CRIT_SECT_LEAVE(pDevIns);
                                    AssertRC(rc);
                                }
                                else
                                    gitsCmdQueueSetError(pDevIns, pGitsDev, kGitsDiag_CmdQueue_Cmd_Mapd_Size_Invalid,
                                                         false /* fStall */);
                            }
                            else
                            {
                                uint64_t const uDte = 0;
                                GIC_CRIT_SECT_ENTER(pDevIns);
                                rc = gitsR3DteWrite(pDevIns, pGitsDev, uDevId, uDte);
                                GIC_CRIT_SECT_LEAVE(pDevIns);
                                /** @todo Remove Device ID from internal cache. */
                                AssertRC(rc);
                            }
                            STAM_COUNTER_INC(&pGitsDev->StatCmdMapd);
                            break;
                        }

                        case GITS_CMD_ID_MAPTI:
                        {
                            /* Map device ID and event ID to corresponding ITE with ICID and the INTID. */
                            uint16_t const uIcId    = RT_BF_GET(pCmd->au64[2].u, GITS_BF_CMD_MAPTI_DW2_IC_ID);
                            uint32_t const uDevId   = RT_BF_GET(pCmd->au64[0].u, GITS_BF_CMD_MAPTI_DW0_DEV_ID);
                            uint32_t const uEventId = RT_BF_GET(pCmd->au64[1].u, GITS_BF_CMD_MAPTI_DW1_EVENT_ID);
                            uint32_t const uIntId   = RT_BF_GET(pCmd->au64[1].u, GITS_BF_CMD_MAPTI_DW1_PHYS_INTID);

                            GIC_CRIT_SECT_ENTER(pDevIns);
                            gitsR3CmdMapIntr(pDevIns, pGitsDev, uDevId, uEventId, uIntId, uIcId, true /* fMapti */);
                            GIC_CRIT_SECT_LEAVE(pDevIns);
                            STAM_COUNTER_INC(&pGitsDev->StatCmdMapti);
                            break;
                        }

                        case GITS_CMD_ID_MAPI:
                        {
                            /* Map device ID and event ID to corresponding ITE with ICID and the INTID same as the event ID. */
                            uint16_t const uIcId    = RT_BF_GET(pCmd->au64[2].u, GITS_BF_CMD_MAPTI_DW2_IC_ID);
                            uint32_t const uDevId   = RT_BF_GET(pCmd->au64[0].u, GITS_BF_CMD_MAPTI_DW0_DEV_ID);
                            uint32_t const uEventId = RT_BF_GET(pCmd->au64[1].u, GITS_BF_CMD_MAPTI_DW1_EVENT_ID);
                            uint32_t const uIntId   = uEventId;

                            GIC_CRIT_SECT_ENTER(pDevIns);
                            gitsR3CmdMapIntr(pDevIns, pGitsDev, uDevId, uEventId, uIntId, uIcId, false /* fMapti */);
                            GIC_CRIT_SECT_LEAVE(pDevIns);
                            STAM_COUNTER_INC(&pGitsDev->StatCmdMapi);
                            break;
                        }

                        case GITS_CMD_ID_INV:
                        {
                            /* Reading the table is likely to take the same time as reading just one entry. */
                            gicDistReadLpiConfigTableFromMem(pDevIns);
                            break;
                        }

                        case GITS_CMD_ID_SYNC:
                            /* Nothing to do since all previous commands have committed their changes to device state. */
                            STAM_COUNTER_INC(&pGitsDev->StatCmdSync);
                            break;

                        case GITS_CMD_ID_INVALL:
                        {
                            /* Reading the table is likely to take the same time as reading just one entry. */
                            uint64_t const uDw2  = pCmd->au64[2].u;
                            uint16_t const uIcId = RT_BF_GET(uDw2, GITS_BF_CMD_INVALL_DW2_IC_ID);
                            PCVMCC         pVM   = PDMDevHlpGetVM(pDevIns);
                            if (uIcId < RT_ELEMENTS(pGitsDev->aCtes))
                            {
                                if (pGitsDev->aCtes[uIcId].idTargetCpu < pVM->cCpus)
                                    gicDistReadLpiConfigTableFromMem(pDevIns);
                                else
                                    gitsCmdQueueSetError(pDevIns, pGitsDev, kGitsDiag_CmdQueue_Cmd_Invall_Cte_Unmapped,
                                                         false /* fStall */);
                            }
                            else
                                gitsCmdQueueSetError(pDevIns, pGitsDev, kGitsDiag_CmdQueue_Cmd_Invall_Icid_Invalid,
                                                     false /* fStall */);
                            STAM_COUNTER_INC(&pGitsDev->StatCmdInvall);
                            break;
                        }

                        default:
                        {
                            /* Record an internal error but do NOT stall queue as we have already advanced the read offset. */
                            gitsCmdQueueSetError(pDevIns, pGitsDev, kGitsDiag_CmdQueue_Basic_Unknown_Cmd, false /* fStall */);
                            AssertReleaseMsgFailed(("Cmd=%#x (%s) idxCmd=%u cCmds=%u offRead=%#RX32 offWrite=%#RX32\n",
                                                    uCmdId, gitsGetCommandName(uCmdId), idxCmd, cCmds, offRead, offWrite));
                            break;
                        }
                    }
                }
                return VINF_SUCCESS;
            }

            /* Failed to read command queue from the physical address specified by the guest, stall queue and retry later. */
            gitsCmdQueueSetError(pDevIns, pGitsDev, kGitsDiag_CmdQueue_Basic_Invalid_PhysAddr, true /* fStall */);
            return VINF_TRY_AGAIN;
        }
    }

    GIC_CRIT_SECT_LEAVE(pDevIns);
    return VINF_SUCCESS;
}
#endif /* IN_RING3 */


DECL_HIDDEN_CALLBACK(int) gitsSetLpi(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, uint32_t uDevId, uint32_t uEventId, bool fAsserted)
{
    /* We support 32-bits of device ID and hence it cannot be out of range (asserted below). */
    Assert(sizeof(uDevId) * 8 >= RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_DEV_BITS) + 1);

    /** @todo Error recording. */

    GIC_CRIT_SECT_ENTER(pDevIns);

    bool const fEnabled = RT_BF_GET(pGitsDev->uCtrlReg, GITS_BF_CTRL_REG_CTLR_ENABLED);
    if (fEnabled)
    {
        /* Read the DTE */
        GITSDTE uDte;
        int rc = gitsR3DteRead(pDevIns, pGitsDev, uDevId, &uDte);
        if (RT_SUCCESS(rc))
        {
            /* Check the DTE is mapped (valid). */
            bool const fValid = RT_BF_GET(uDte, GITS_BF_DTE_VALID);
            if (fValid)
            {
                /* Check that the event ID (which is the index) is within range. */
                uint32_t const cEntries = RT_BIT_32(RT_BF_GET(uDte, GITS_BF_DTE_ITT_ADDR) + 1);
                if (uEventId < cEntries)
                {
                    /* Read the interrupt-translation entry. */
                    GITSITE uIte = 0;
                    rc = gitsR3IteRead(pDevIns, uDte, uEventId, &uIte);
                    if (RT_SUCCESS(rc))
                    {
                        /* Check the interrupt ID is within range. */
                        uint16_t const uIntId = RT_BF_GET(uIte, GITS_BF_ITE_INTID);
                        uint16_t const uIcId  = RT_BF_GET(uIte, GITS_BF_ITE_ICID);
                        bool const fIsLpiValid = gicDistIsLpiValid(pDevIns, uIntId);
                        if (fIsLpiValid)
                        {
                            /* Check the interrupt collection ID is valid. */
                            if (uIcId < RT_ELEMENTS(pGitsDev->aCtes))
                            {
                                Assert(!RT_BF_GET(pGitsDev->uTypeReg.u, GITS_BF_CTRL_REG_TYPER_PTA));
                                PCVMCC         pVM  = PDMDevHlpGetVM(pDevIns);
                                VMCPUID const idCpu = pGitsDev->aCtes[uIcId].idTargetCpu;

                                /* Check that the target CPU is valid. */
                                if (idCpu < pVM->cCpus)
                                {
                                    /* Set or clear the LPI pending state in the redistributor. */
                                    PVMCPUCC pVCpu = pVM->CTX_SUFF(apCpus)[idCpu];
                                    gicReDistSetLpi(pDevIns, pVCpu, uIntId, fAsserted);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    GIC_CRIT_SECT_LEAVE(pDevIns);
    return VINF_SUCCESS;
}


#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

