/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, R0 Device parts.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PDM_DEVICE
#define PDMPCIDEV_INCLUDE_PRIVATE  /* Hack to get pdmpcidevint.h included at the right point. */
#include "PDMInternal.h"
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/apic.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/gvm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/vmcc.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/vmm/gvmm.h>
#include <VBox/sup.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/process.h>
#include <iprt/string.h>

#include "dtrace/VBoxVMM.h"
#include "PDMInline.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
extern DECLEXPORT(const PDMDEVHLPR0)    g_pdmR0DevHlp;
extern DECLEXPORT(const PDMPICHLPR0)    g_pdmR0PicHlp;
extern DECLEXPORT(const PDMIOAPICHLPR0) g_pdmR0IoApicHlp;
extern DECLEXPORT(const PDMPCIHLPR0)    g_pdmR0PciHlp;
extern DECLEXPORT(const PDMHPETHLPR0)   g_pdmR0HpetHlp;
extern DECLEXPORT(const PDMPCIRAWHLPR0) g_pdmR0PciRawHlp;
extern DECLEXPORT(const PDMDRVHLPR0)    g_pdmR0DrvHlp;
RT_C_DECLS_END

/** List of PDMDEVMODREGR0 structures protected by the loader lock. */
static RTLISTANCHOR g_PDMDevModList;


/**
 * Pointer to the ring-0 device registrations for VMMR0.
 */
static const PDMDEVREGR0 *g_apVMM0DevRegs[] =
{
    &g_DeviceAPIC,
};

/**
 * Module device registration record for VMMR0.
 */
static PDMDEVMODREGR0 g_VBoxDDR0ModDevReg =
{
    /* .u32Version = */ PDM_DEVMODREGR0_VERSION,
    /* .cDevRegs = */   RT_ELEMENTS(g_apVMM0DevRegs),
    /* .papDevRegs = */ &g_apVMM0DevRegs[0],
    /* .hMod = */       NULL,
    /* .ListEntry = */  { NULL, NULL },
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static bool pdmR0IsaSetIrq(PGVM pGVM, int iIrq, int iLevel, uint32_t uTagSrc);


/**
 * Initializes the global ring-0 PDM data.
 */
VMMR0_INT_DECL(void) PDMR0Init(void *hMod)
{
    RTListInit(&g_PDMDevModList);
    g_VBoxDDR0ModDevReg.hMod = hMod;
    RTListAppend(&g_PDMDevModList, &g_VBoxDDR0ModDevReg.ListEntry);
}


/**
 * Used by PDMR0CleanupVM to destroy a device instance.
 *
 * This is done during VM cleanup so that we're sure there are no active threads
 * inside the device code.
 *
 * @param   pGVM        The global (ring-0) VM structure.
 * @param   pDevIns     The device instance.
 * @param   idxR0Device The device instance handle.
 */
static int pdmR0DeviceDestroy(PGVM pGVM, PPDMDEVINSR0 pDevIns, uint32_t idxR0Device)
{
    /*
     * Assert sanity.
     */
    Assert(idxR0Device < pGVM->pdmr0.s.cDevInstances);
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    Assert(pDevIns->u32Version == PDM_DEVINSR0_VERSION);
    Assert(pDevIns->Internal.s.idxR0Device == idxR0Device);

    /*
     * Call the final destructor if there is one.
     */
    if (pDevIns->pReg->pfnFinalDestruct)
        pDevIns->pReg->pfnFinalDestruct(pDevIns);
    pDevIns->u32Version = ~PDM_DEVINSR0_VERSION;

    /*
     * Remove the device from the instance table.
     */
    Assert(pGVM->pdmr0.s.apDevInstances[idxR0Device] == pDevIns);
    pGVM->pdmr0.s.apDevInstances[idxR0Device] = NULL;
    if (idxR0Device + 1 == pGVM->pdmr0.s.cDevInstances)
        pGVM->pdmr0.s.cDevInstances = idxR0Device;

    /*
     * Free the ring-3 mapping and instance memory.
     */
    RTR0MEMOBJ hMemObj = pDevIns->Internal.s.hMapObj;
    pDevIns->Internal.s.hMapObj = NIL_RTR0MEMOBJ;
    RTR0MemObjFree(hMemObj, true);

    hMemObj = pDevIns->Internal.s.hMemObj;
    pDevIns->Internal.s.hMemObj = NIL_RTR0MEMOBJ;
    RTR0MemObjFree(hMemObj, true);

    return VINF_SUCCESS;
}


/**
 * Initializes the per-VM data for the PDM.
 *
 * This is called from under the GVMM lock, so it only need to initialize the
 * data so PDMR0CleanupVM and others will work smoothly.
 *
 * @param   pGVM    Pointer to the global VM structure.
 */
VMMR0_INT_DECL(void) PDMR0InitPerVMData(PGVM pGVM)
{
    AssertCompile(sizeof(pGVM->pdm.s) <= sizeof(pGVM->pdm.padding));
    AssertCompile(sizeof(pGVM->pdmr0.s) <= sizeof(pGVM->pdmr0.padding));

    pGVM->pdmr0.s.cDevInstances = 0;
}


/**
 * Cleans up any loose ends before the GVM structure is destroyed.
 */
VMMR0_INT_DECL(void) PDMR0CleanupVM(PGVM pGVM)
{
    uint32_t i = pGVM->pdmr0.s.cDevInstances;
    while (i-- > 0)
    {
        PPDMDEVINSR0 pDevIns = pGVM->pdmr0.s.apDevInstances[i];
        if (pDevIns)
            pdmR0DeviceDestroy(pGVM, pDevIns, i);
    }
}


/** @name Ring-0 Device Helpers
 * @{
 */

/** @interface_method_impl{PDMDEVHLPR0,pfnIoPortSetUpContextEx} */
static DECLCALLBACK(int) pdmR0DevHlp_IoPortSetUpContextEx(PPDMDEVINS pDevIns, IOMIOPORTHANDLE hIoPorts,
                                                          PFNIOMIOPORTOUT pfnOut, PFNIOMIOPORTIN pfnIn,
                                                          PFNIOMIOPORTOUTSTRING pfnOutStr, PFNIOMIOPORTINSTRING pfnInStr,
                                                          void *pvUser)
{
    RT_NOREF(pDevIns, hIoPorts, pfnOut, pfnIn, pfnOutStr, pfnInStr, pvUser);
    return VERR_NOT_IMPLEMENTED;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnMmioSetUpContextEx} */
static DECLCALLBACK(int) pdmR0DevHlp_MmioSetUpContextEx(PPDMDEVINS pDevIns, IOMMMIOHANDLE hRegion, PFNIOMMMIOWRITE pfnWrite,
                                                        PFNIOMMMIOREAD pfnRead, PFNIOMMMIOFILL pfnFill, void *pvUser)
{
    RT_NOREF(pDevIns, hRegion, pfnWrite, pfnRead, pfnFill, pvUser);
    return VERR_NOT_IMPLEMENTED;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPCIPhysRead} */
static DECLCALLBACK(int) pdmR0DevHlp_PCIPhysRead(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, RTGCPHYS GCPhys,
                                                 void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (!pPciDev) /* NULL is an alias for the default PCI device. */
        pPciDev = pDevIns->Internal.s.pHeadPciDevR0;
    AssertReturn(pPciDev, VERR_PDM_NOT_PCI_DEVICE);

#ifndef PDM_DO_NOT_RESPECT_PCI_BM_BIT
    /*
     * Just check the busmaster setting here and forward the request to the generic read helper.
     */
    if (PCIDevIsBusmaster(pPciDev))
    { /* likely */ }
    else
    {
        Log(("pdmRCDevHlp_PCIPhysRead: caller=%p/%d: returns %Rrc - Not bus master! GCPhys=%RGp cbRead=%#zx\n",
             pDevIns, pDevIns->iInstance, VERR_PDM_NOT_PCI_BUS_MASTER, GCPhys, cbRead));
        memset(pvBuf, 0xff, cbRead);
        return VERR_PDM_NOT_PCI_BUS_MASTER;
    }
#endif

    return pDevIns->pHlpR0->pfnPhysRead(pDevIns, GCPhys, pvBuf, cbRead);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPCIPhysWrite} */
static DECLCALLBACK(int) pdmR0DevHlp_PCIPhysWrite(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, RTGCPHYS GCPhys,
                                                  const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (!pPciDev) /* NULL is an alias for the default PCI device. */
        pPciDev = pDevIns->Internal.s.pHeadPciDevR0;
    AssertReturn(pPciDev, VERR_PDM_NOT_PCI_DEVICE);

#ifndef PDM_DO_NOT_RESPECT_PCI_BM_BIT
    /*
     * Just check the busmaster setting here and forward the request to the generic read helper.
     */
    if (PCIDevIsBusmaster(pPciDev))
    { /* likely */ }
    else
    {
        Log(("pdmRCDevHlp_PCIPhysWrite: caller=%p/%d: returns %Rrc - Not bus master! GCPhys=%RGp cbWrite=%#zx\n",
             pDevIns, pDevIns->iInstance, VERR_PDM_NOT_PCI_BUS_MASTER, GCPhys, cbWrite));
        return VERR_PDM_NOT_PCI_BUS_MASTER;
    }
#endif

    return pDevIns->pHlpR0->pfnPhysWrite(pDevIns, GCPhys, pvBuf, cbWrite);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPCISetIrq} */
static DECLCALLBACK(void) pdmR0DevHlp_PCISetIrq(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (!pPciDev) /* NULL is an alias for the default PCI device. */
        pPciDev = pDevIns->Internal.s.pHeadPciDevR0;
    AssertReturnVoid(pPciDev);
    LogFlow(("pdmR0DevHlp_PCISetIrq: caller=%p/%d: pPciDev=%p:{%#x} iIrq=%d iLevel=%d\n",
             pDevIns, pDevIns->iInstance, pPciDev, pPciDev->uDevFn, iIrq, iLevel));
    PGVM         pGVM    = pDevIns->Internal.s.pGVM;
    PPDMPCIBUS   pPciBus = pPciDev->Int.s.pPdmBusR0;

    pdmLock(pGVM);
    uint32_t uTagSrc;
    if (iLevel & PDM_IRQ_LEVEL_HIGH)
    {
        pDevIns->Internal.s.pIntR3R0->uLastIrqTag = uTagSrc = pdmCalcIrqTag(pGVM, pDevIns->Internal.s.pInsR3R0->idTracing);
        if (iLevel == PDM_IRQ_LEVEL_HIGH)
            VBOXVMM_PDM_IRQ_HIGH(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
        else
            VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    }
    else
        uTagSrc = pDevIns->Internal.s.pIntR3R0->uLastIrqTag;

    if (    pPciBus
        &&  pPciBus->pDevInsR0)
    {
        pPciBus->pfnSetIrqR0(pPciBus->pDevInsR0, pPciDev, iIrq, iLevel, uTagSrc);

        pdmUnlock(pGVM);

        if (iLevel == PDM_IRQ_LEVEL_LOW)
            VBOXVMM_PDM_IRQ_LOW(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    }
    else
    {
        pdmUnlock(pGVM);

        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pGVM->pdm.s.pDevHlpQueueR0);
        AssertReturnVoid(pTask);

        pTask->enmOp = PDMDEVHLPTASKOP_PCI_SET_IRQ;
        pTask->pDevInsR3 = PDMDEVINS_2_R3PTR(pDevIns);
        pTask->u.PciSetIRQ.iIrq = iIrq;
        pTask->u.PciSetIRQ.iLevel = iLevel;
        pTask->u.PciSetIRQ.uTagSrc = uTagSrc;
        pTask->u.PciSetIRQ.pPciDevR3 = MMHyperR0ToR3(pGVM, pPciDev);

        PDMQueueInsertEx(pGVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
    }

    LogFlow(("pdmR0DevHlp_PCISetIrq: caller=%p/%d: returns void; uTagSrc=%#x\n", pDevIns, pDevIns->iInstance, uTagSrc));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnISASetIrq} */
static DECLCALLBACK(void) pdmR0DevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_ISASetIrq: caller=%p/%d: iIrq=%d iLevel=%d\n", pDevIns, pDevIns->iInstance, iIrq, iLevel));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    pdmLock(pGVM);
    uint32_t uTagSrc;
    if (iLevel & PDM_IRQ_LEVEL_HIGH)
    {
        pDevIns->Internal.s.pIntR3R0->uLastIrqTag = uTagSrc = pdmCalcIrqTag(pGVM, pDevIns->Internal.s.pInsR3R0->idTracing);
        if (iLevel == PDM_IRQ_LEVEL_HIGH)
            VBOXVMM_PDM_IRQ_HIGH(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
        else
            VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    }
    else
        uTagSrc = pDevIns->Internal.s.pIntR3R0->uLastIrqTag;

    bool fRc = pdmR0IsaSetIrq(pGVM, iIrq, iLevel, uTagSrc);

    if (iLevel == PDM_IRQ_LEVEL_LOW && fRc)
        VBOXVMM_PDM_IRQ_LOW(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));
    pdmUnlock(pGVM);
    LogFlow(("pdmR0DevHlp_ISASetIrq: caller=%p/%d: returns void; uTagSrc=%#x\n", pDevIns, pDevIns->iInstance, uTagSrc));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnIoApicSendMsi} */
static DECLCALLBACK(void) pdmR0DevHlp_IoApicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t uValue)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_IoApicSendMsi: caller=%p/%d: GCPhys=%RGp uValue=%#x\n", pDevIns, pDevIns->iInstance, GCPhys, uValue));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    uint32_t uTagSrc;
    pDevIns->Internal.s.pIntR3R0->uLastIrqTag = uTagSrc = pdmCalcIrqTag(pGVM, pDevIns->Internal.s.pInsR3R0->idTracing);
    VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pGVM), RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc));

    if (pGVM->pdm.s.IoApic.pDevInsR0)
        pGVM->pdm.s.IoApic.pfnSendMsiR0(pGVM->pdm.s.IoApic.pDevInsR0, GCPhys, uValue, uTagSrc);
    else
        AssertFatalMsgFailed(("Lazy bastards!"));

    LogFlow(("pdmR0DevHlp_IoApicSendMsi: caller=%p/%d: returns void; uTagSrc=%#x\n", pDevIns, pDevIns->iInstance, uTagSrc));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPhysRead} */
static DECLCALLBACK(int) pdmR0DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PhysRead: caller=%p/%d: GCPhys=%RGp pvBuf=%p cbRead=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbRead));

    VBOXSTRICTRC rcStrict = PGMPhysRead(pDevIns->Internal.s.pGVM, GCPhys, pvBuf, cbRead, PGMACCESSORIGIN_DEVICE);
    AssertMsg(rcStrict == VINF_SUCCESS, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict))); /** @todo track down the users for this bugger. */

    Log(("pdmR0DevHlp_PhysRead: caller=%p/%d: returns %Rrc\n", pDevIns, pDevIns->iInstance, VBOXSTRICTRC_VAL(rcStrict) ));
    return VBOXSTRICTRC_VAL(rcStrict);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnPhysWrite} */
static DECLCALLBACK(int) pdmR0DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_PhysWrite: caller=%p/%d: GCPhys=%RGp pvBuf=%p cbWrite=%#x\n",
             pDevIns, pDevIns->iInstance, GCPhys, pvBuf, cbWrite));

    VBOXSTRICTRC rcStrict = PGMPhysWrite(pDevIns->Internal.s.pGVM, GCPhys, pvBuf, cbWrite, PGMACCESSORIGIN_DEVICE);
    AssertMsg(rcStrict == VINF_SUCCESS, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict))); /** @todo track down the users for this bugger. */

    Log(("pdmR0DevHlp_PhysWrite: caller=%p/%d: returns %Rrc\n", pDevIns, pDevIns->iInstance, VBOXSTRICTRC_VAL(rcStrict) ));
    return VBOXSTRICTRC_VAL(rcStrict);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnA20IsEnabled} */
static DECLCALLBACK(bool) pdmR0DevHlp_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_A20IsEnabled: caller=%p/%d:\n", pDevIns, pDevIns->iInstance));

    bool fEnabled = PGMPhysIsA20Enabled(VMMGetCpu(pDevIns->Internal.s.pGVM));

    Log(("pdmR0DevHlp_A20IsEnabled: caller=%p/%d: returns %RTbool\n", pDevIns, pDevIns->iInstance, fEnabled));
    return fEnabled;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMState} */
static DECLCALLBACK(VMSTATE) pdmR0DevHlp_VMState(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);

    VMSTATE enmVMState = pDevIns->Internal.s.pGVM->enmVMState;

    LogFlow(("pdmR0DevHlp_VMState: caller=%p/%d: returns %d\n", pDevIns, pDevIns->iInstance, enmVMState));
    return enmVMState;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMSetError} */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pGVM, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMSetErrorV} */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pGVM, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMSetRuntimeError} */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list va;
    va_start(va, pszFormat);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pGVM, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
    return rc;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnVMSetRuntimeErrorV} */
static DECLCALLBACK(int) pdmR0DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pGVM, fFlags, pszErrorId, pszFormat, va);
    return rc;
}



/** @interface_method_impl{PDMDEVHLPR0,pfnGetVM} */
static DECLCALLBACK(PVMCC)  pdmR0DevHlp_GetVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_GetVM: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return pDevIns->Internal.s.pGVM;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnGetVMCPU} */
static DECLCALLBACK(PVMCPUCC) pdmR0DevHlp_GetVMCPU(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_GetVMCPU: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return VMMGetCpu(pDevIns->Internal.s.pGVM);
}


/** @interface_method_impl{PDMDEVHLPRC,pfnGetCurrentCpuId} */
static DECLCALLBACK(VMCPUID) pdmR0DevHlp_GetCurrentCpuId(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VMCPUID idCpu = VMMGetCpuId(pDevIns->Internal.s.pGVM);
    LogFlow(("pdmR0DevHlp_GetCurrentCpuId: caller='%p'/%d for CPU %u\n", pDevIns, pDevIns->iInstance, idCpu));
    return idCpu;
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerToPtr} */
static DECLCALLBACK(PTMTIMERR0) pdmR0DevHlp_TimerToPtr(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RT_NOREF(pDevIns);
    return (PTMTIMERR0)MMHyperR3ToCC(pDevIns->Internal.s.pGVM, hTimer);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerFromMicro} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerFromMicro(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cMicroSecs)
{
    return TMTimerFromMicro(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cMicroSecs);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerFromMilli} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerFromMilli(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cMilliSecs)
{
    return TMTimerFromMilli(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cMilliSecs);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerFromNano} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerFromNano(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cNanoSecs)
{
    return TMTimerFromNano(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cNanoSecs);
}

/** @interface_method_impl{PDMDEVHLPR0,pfnTimerGet} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerGet(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerGet(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerGetFreq} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerGetFreq(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerGetFreq(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerGetNano} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TimerGetNano(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerGetNano(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerIsActive} */
static DECLCALLBACK(bool) pdmR0DevHlp_TimerIsActive(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerIsActive(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerIsLockOwner} */
static DECLCALLBACK(bool) pdmR0DevHlp_TimerIsLockOwner(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerIsLockOwner(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerLock} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerLock(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, int rcBusy)
{
    return TMTimerLock(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), rcBusy);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSet} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSet(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t uExpire)
{
    return TMTimerSet(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), uExpire);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetFrequencyHint} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetFrequencyHint(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint32_t uHz)
{
    return TMTimerSetFrequencyHint(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), uHz);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetMicro} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetMicro(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cMicrosToNext)
{
    return TMTimerSetMicro(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cMicrosToNext);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetMillies} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetMillies(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cMilliesToNext)
{
    return TMTimerSetMillies(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cMilliesToNext);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetNano} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetNano(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cNanosToNext)
{
    return TMTimerSetNano(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cNanosToNext);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerSetRelative} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerSetRelative(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer, uint64_t cTicksToNext, uint64_t *pu64Now)
{
    return TMTimerSetRelative(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer), cTicksToNext, pu64Now);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerStop} */
static DECLCALLBACK(int) pdmR0DevHlp_TimerStop(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    return TMTimerStop(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTimerUnlock} */
static DECLCALLBACK(void) pdmR0DevHlp_TimerUnlock(PPDMDEVINS pDevIns, TMTIMERHANDLE hTimer)
{
    TMTimerUnlock(pdmR0DevHlp_TimerToPtr(pDevIns, hTimer));
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTMTimeVirtGet} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TMTimeVirtGet(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_TMTimeVirtGet: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return TMVirtualGet(pDevIns->Internal.s.pGVM);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTMTimeVirtGetFreq} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TMTimeVirtGetFreq(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_TMTimeVirtGetFreq: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return TMVirtualGetFreq(pDevIns->Internal.s.pGVM);
}


/** @interface_method_impl{PDMDEVHLPR0,pfnTMTimeVirtGetNano} */
static DECLCALLBACK(uint64_t) pdmR0DevHlp_TMTimeVirtGetNano(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR0DevHlp_TMTimeVirtGetNano: caller='%p'/%d\n", pDevIns, pDevIns->iInstance));
    return TMVirtualToNano(pDevIns->Internal.s.pGVM, TMVirtualGet(pDevIns->Internal.s.pGVM));
}

/** @interface_method_impl{PDMDEVHLPR0,pfnCritSectGetNop} */
static DECLCALLBACK(PPDMCRITSECT) pdmR0DevHlp_CritSectGetNop(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    PPDMCRITSECT pCritSect = &pGVM->pdm.s.NopCritSect;
    LogFlow(("pdmR0DevHlp_CritSectGetNop: caller='%s'/%d: return %p\n", pDevIns->pReg->szName, pDevIns->iInstance, pCritSect));
    return pCritSect;
}

/** @interface_method_impl{PDMDEVHLPR0,pfnSetDeviceCritSect} */
static DECLCALLBACK(int) pdmR0DevHlp_SetDeviceCritSect(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect)
{
    /*
     * Validate input.
     *
     * Note! We only allow the automatically created default critical section
     *       to be replaced by this API.
     */
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertPtrReturn(pCritSect, VERR_INVALID_POINTER);
    LogFlow(("pdmR0DevHlp_SetDeviceCritSect: caller='%s'/%d: pCritSect=%p (%s)\n",
             pDevIns->pReg->szName, pDevIns->iInstance, pCritSect, pCritSect->s.pszName));
    AssertReturn(PDMCritSectIsInitialized(pCritSect), VERR_INVALID_PARAMETER);
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    AssertReturn(pCritSect->s.pVMR0 == pGVM, VERR_INVALID_PARAMETER);

    VM_ASSERT_EMT(pGVM);
    VM_ASSERT_STATE_RETURN(pGVM, VMSTATE_CREATING, VERR_WRONG_ORDER);

    /*
     * Check that ring-3 has already done this, then effect the change.
     */
    AssertReturn(pDevIns->pDevInsForR3R0->Internal.s.fIntFlags & PDMDEVINSINT_FLAGS_CHANGED_CRITSECT, VERR_WRONG_ORDER);
    pDevIns->pCritSectRoR0 = pCritSect;

    LogFlow(("pdmR0DevHlp_SetDeviceCritSect: caller='%s'/%d: returns %Rrc\n", pDevIns->pReg->szName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}

/** @interface_method_impl{PDMDEVHLPR0,pfnDBGFTraceBuf} */
static DECLCALLBACK(RTTRACEBUF) pdmR0DevHlp_DBGFTraceBuf(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    RTTRACEBUF hTraceBuf = pDevIns->Internal.s.pGVM->hTraceBufR0;
    LogFlow(("pdmR3DevHlp_DBGFTraceBuf: caller='%p'/%d: returns %p\n", pDevIns, pDevIns->iInstance, hTraceBuf));
    return hTraceBuf;
}


/**
 * The Ring-0 Device Helper Callbacks.
 */
extern DECLEXPORT(const PDMDEVHLPR0) g_pdmR0DevHlp =
{
    PDM_DEVHLPR0_VERSION,
    pdmR0DevHlp_IoPortSetUpContextEx,
    pdmR0DevHlp_MmioSetUpContextEx,
    pdmR0DevHlp_PCIPhysRead,
    pdmR0DevHlp_PCIPhysWrite,
    pdmR0DevHlp_PCISetIrq,
    pdmR0DevHlp_ISASetIrq,
    pdmR0DevHlp_IoApicSendMsi,
    pdmR0DevHlp_PhysRead,
    pdmR0DevHlp_PhysWrite,
    pdmR0DevHlp_A20IsEnabled,
    pdmR0DevHlp_VMState,
    pdmR0DevHlp_VMSetError,
    pdmR0DevHlp_VMSetErrorV,
    pdmR0DevHlp_VMSetRuntimeError,
    pdmR0DevHlp_VMSetRuntimeErrorV,
    pdmR0DevHlp_GetVM,
    pdmR0DevHlp_GetVMCPU,
    pdmR0DevHlp_GetCurrentCpuId,
    pdmR0DevHlp_TimerToPtr,
    pdmR0DevHlp_TimerFromMicro,
    pdmR0DevHlp_TimerFromMilli,
    pdmR0DevHlp_TimerFromNano,
    pdmR0DevHlp_TimerGet,
    pdmR0DevHlp_TimerGetFreq,
    pdmR0DevHlp_TimerGetNano,
    pdmR0DevHlp_TimerIsActive,
    pdmR0DevHlp_TimerIsLockOwner,
    pdmR0DevHlp_TimerLock,
    pdmR0DevHlp_TimerSet,
    pdmR0DevHlp_TimerSetFrequencyHint,
    pdmR0DevHlp_TimerSetMicro,
    pdmR0DevHlp_TimerSetMillies,
    pdmR0DevHlp_TimerSetNano,
    pdmR0DevHlp_TimerSetRelative,
    pdmR0DevHlp_TimerStop,
    pdmR0DevHlp_TimerUnlock,
    pdmR0DevHlp_TMTimeVirtGet,
    pdmR0DevHlp_TMTimeVirtGetFreq,
    pdmR0DevHlp_TMTimeVirtGetNano,
    pdmR0DevHlp_CritSectGetNop,
    pdmR0DevHlp_SetDeviceCritSect,
    PDMCritSectEnter,
    PDMCritSectEnterDebug,
    PDMCritSectTryEnter,
    PDMCritSectTryEnterDebug,
    PDMCritSectLeave,
    PDMCritSectIsOwner,
    PDMCritSectIsInitialized,
    PDMCritSectHasWaiters,
    PDMCritSectGetRecursion,
    pdmR0DevHlp_DBGFTraceBuf,
    NULL /*pfnReserved1*/,
    NULL /*pfnReserved2*/,
    NULL /*pfnReserved3*/,
    NULL /*pfnReserved4*/,
    NULL /*pfnReserved5*/,
    NULL /*pfnReserved6*/,
    NULL /*pfnReserved7*/,
    NULL /*pfnReserved8*/,
    NULL /*pfnReserved9*/,
    NULL /*pfnReserved10*/,
    PDM_DEVHLPR0_VERSION
};

/** @} */




/** @name PIC Ring-0 Helpers
 * @{
 */

/** @interface_method_impl{PDMPICHLPR0,pfnSetInterruptFF} */
static DECLCALLBACK(void) pdmR0PicHlp_SetInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM     pGVM  = (PGVM)pDevIns->Internal.s.pGVM;
    PVMCPUCC pVCpu = &pGVM->aCpus[0];     /* for PIC we always deliver to CPU 0, MP use APIC */
    /** @todo r=ramshankar: Propagating rcRZ and make all callers handle it? */
    APICLocalInterrupt(pVCpu, 0 /* u8Pin */, 1 /* u8Level */, VINF_SUCCESS /* rcRZ */);
}


/** @interface_method_impl{PDMPICHLPR0,pfnClearInterruptFF} */
static DECLCALLBACK(void) pdmR0PicHlp_ClearInterruptFF(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM     pGVM  = (PGVM)pDevIns->Internal.s.pGVM;
    PVMCPUCC pVCpu = &pGVM->aCpus[0];     /* for PIC we always deliver to CPU 0, MP use APIC */
    /** @todo r=ramshankar: Propagating rcRZ and make all callers handle it? */
    APICLocalInterrupt(pVCpu, 0 /* u8Pin */, 0 /* u8Level */, VINF_SUCCESS /* rcRZ */);
}


/** @interface_method_impl{PDMPICHLPR0,pfnLock} */
static DECLCALLBACK(int) pdmR0PicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pGVM, rc);
}


/** @interface_method_impl{PDMPICHLPR0,pfnUnlock} */
static DECLCALLBACK(void) pdmR0PicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pGVM);
}


/**
 * The Ring-0 PIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMPICHLPR0) g_pdmR0PicHlp =
{
    PDM_PICHLPR0_VERSION,
    pdmR0PicHlp_SetInterruptFF,
    pdmR0PicHlp_ClearInterruptFF,
    pdmR0PicHlp_Lock,
    pdmR0PicHlp_Unlock,
    PDM_PICHLPR0_VERSION
};

/** @} */


/** @name I/O APIC Ring-0 Helpers
 * @{
 */

/** @interface_method_impl{PDMIOAPICHLPR0,pfnApicBusDeliver} */
static DECLCALLBACK(int) pdmR0IoApicHlp_ApicBusDeliver(PPDMDEVINS pDevIns, uint8_t u8Dest, uint8_t u8DestMode,
                                                       uint8_t u8DeliveryMode, uint8_t uVector, uint8_t u8Polarity,
                                                       uint8_t u8TriggerMode, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    LogFlow(("pdmR0IoApicHlp_ApicBusDeliver: caller=%p/%d: u8Dest=%RX8 u8DestMode=%RX8 u8DeliveryMode=%RX8 uVector=%RX8 u8Polarity=%RX8 u8TriggerMode=%RX8 uTagSrc=%#x\n",
             pDevIns, pDevIns->iInstance, u8Dest, u8DestMode, u8DeliveryMode, uVector, u8Polarity, u8TriggerMode, uTagSrc));
    return APICBusDeliver(pGVM, u8Dest, u8DestMode, u8DeliveryMode, uVector, u8Polarity, u8TriggerMode, uTagSrc);
}


/** @interface_method_impl{PDMIOAPICHLPR0,pfnLock} */
static DECLCALLBACK(int) pdmR0IoApicHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pGVM, rc);
}


/** @interface_method_impl{PDMIOAPICHLPR0,pfnUnlock} */
static DECLCALLBACK(void) pdmR0IoApicHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pGVM);
}


/**
 * The Ring-0 I/O APIC Helper Callbacks.
 */
extern DECLEXPORT(const PDMIOAPICHLPR0) g_pdmR0IoApicHlp =
{
    PDM_IOAPICHLPR0_VERSION,
    pdmR0IoApicHlp_ApicBusDeliver,
    pdmR0IoApicHlp_Lock,
    pdmR0IoApicHlp_Unlock,
    PDM_IOAPICHLPR0_VERSION
};

/** @} */




/** @name PCI Bus Ring-0 Helpers
 * @{
 */

/** @interface_method_impl{PDMPCIHLPR0,pfnIsaSetIrq} */
static DECLCALLBACK(void) pdmR0PciHlp_IsaSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IsaSetIrq: iIrq=%d iLevel=%d uTagSrc=%#x\n", iIrq, iLevel, uTagSrc));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    pdmLock(pGVM);
    pdmR0IsaSetIrq(pGVM, iIrq, iLevel, uTagSrc);
    pdmUnlock(pGVM);
}


/** @interface_method_impl{PDMPCIHLPR0,pfnIoApicSetIrq} */
static DECLCALLBACK(void) pdmR0PciHlp_IoApicSetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IoApicSetIrq: iIrq=%d iLevel=%d uTagSrc=%#x\n", iIrq, iLevel, uTagSrc));
    PGVM pGVM = pDevIns->Internal.s.pGVM;

    if (pGVM->pdm.s.IoApic.pDevInsR0)
        pGVM->pdm.s.IoApic.pfnSetIrqR0(pGVM->pdm.s.IoApic.pDevInsR0, iIrq, iLevel, uTagSrc);
    else if (pGVM->pdm.s.IoApic.pDevInsR3)
    {
        /* queue for ring-3 execution. */
        PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pGVM->pdm.s.pDevHlpQueueR0);
        if (pTask)
        {
            pTask->enmOp = PDMDEVHLPTASKOP_IOAPIC_SET_IRQ;
            pTask->pDevInsR3 = NIL_RTR3PTR; /* not required */
            pTask->u.IoApicSetIRQ.iIrq = iIrq;
            pTask->u.IoApicSetIRQ.iLevel = iLevel;
            pTask->u.IoApicSetIRQ.uTagSrc = uTagSrc;

            PDMQueueInsertEx(pGVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
        }
        else
            AssertMsgFailed(("We're out of devhlp queue items!!!\n"));
    }
}


/** @interface_method_impl{PDMPCIHLPR0,pfnIoApicSendMsi} */
static DECLCALLBACK(void) pdmR0PciHlp_IoApicSendMsi(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t uValue, uint32_t uTagSrc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    Log4(("pdmR0PciHlp_IoApicSendMsi: GCPhys=%p uValue=%d uTagSrc=%#x\n", GCPhys, uValue, uTagSrc));
    PGVM pGVM = pDevIns->Internal.s.pGVM;
    if (pGVM->pdm.s.IoApic.pDevInsR0)
        pGVM->pdm.s.IoApic.pfnSendMsiR0(pGVM->pdm.s.IoApic.pDevInsR0, GCPhys, uValue, uTagSrc);
    else
        AssertFatalMsgFailed(("Lazy bastards!"));
}


/** @interface_method_impl{PDMPCIHLPR0,pfnLock} */
static DECLCALLBACK(int) pdmR0PciHlp_Lock(PPDMDEVINS pDevIns, int rc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    return pdmLockEx(pDevIns->Internal.s.pGVM, rc);
}


/** @interface_method_impl{PDMPCIHLPR0,pfnUnlock} */
static DECLCALLBACK(void) pdmR0PciHlp_Unlock(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    pdmUnlock(pDevIns->Internal.s.pGVM);
}


/**
 * The Ring-0 PCI Bus Helper Callbacks.
 */
extern DECLEXPORT(const PDMPCIHLPR0) g_pdmR0PciHlp =
{
    PDM_PCIHLPR0_VERSION,
    pdmR0PciHlp_IsaSetIrq,
    pdmR0PciHlp_IoApicSetIrq,
    pdmR0PciHlp_IoApicSendMsi,
    pdmR0PciHlp_Lock,
    pdmR0PciHlp_Unlock,
    PDM_PCIHLPR0_VERSION, /* the end */
};

/** @} */




/** @name HPET Ring-0 Helpers
 * @{
 */
/* none */

/**
 * The Ring-0 HPET Helper Callbacks.
 */
extern DECLEXPORT(const PDMHPETHLPR0) g_pdmR0HpetHlp =
{
    PDM_HPETHLPR0_VERSION,
    PDM_HPETHLPR0_VERSION, /* the end */
};

/** @} */


/** @name Raw PCI Ring-0 Helpers
 * @{
 */
/* none */

/**
 * The Ring-0 PCI raw Helper Callbacks.
 */
extern DECLEXPORT(const PDMPCIRAWHLPR0) g_pdmR0PciRawHlp =
{
    PDM_PCIRAWHLPR0_VERSION,
    PDM_PCIRAWHLPR0_VERSION, /* the end */
};

/** @} */


/** @name Ring-0 Context Driver Helpers
 * @{
 */

/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetError} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetError(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDrvIns->Internal.s.pVMR0, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetErrorV} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetErrorV(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    int rc2 = VMSetErrorV(pDrvIns->Internal.s.pVMR0, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetRuntimeError} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetRuntimeError(PPDMDRVINS pDrvIns, uint32_t fFlags, const char *pszErrorId,
                                                       const char *pszFormat, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    va_list va;
    va_start(va, pszFormat);
    int rc = VMSetRuntimeErrorV(pDrvIns->Internal.s.pVMR0, fFlags, pszErrorId, pszFormat, va);
    va_end(va);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnVMSetRuntimeErrorV} */
static DECLCALLBACK(int) pdmR0DrvHlp_VMSetRuntimeErrorV(PPDMDRVINS pDrvIns, uint32_t fFlags, const char *pszErrorId,
                                                        const char *pszFormat, va_list va)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    int rc = VMSetRuntimeErrorV(pDrvIns->Internal.s.pVMR0, fFlags, pszErrorId, pszFormat, va);
    return rc;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnAssertEMT} */
static DECLCALLBACK(bool) pdmR0DrvHlp_AssertEMT(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    if (VM_IS_EMT(pDrvIns->Internal.s.pVMR0))
        return true;

    RTAssertMsg1Weak("AssertEMT", iLine, pszFile, pszFunction);
    RTAssertPanic();
    return false;
}


/** @interface_method_impl{PDMDRVHLPR0,pfnAssertOther} */
static DECLCALLBACK(bool) pdmR0DrvHlp_AssertOther(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    if (!VM_IS_EMT(pDrvIns->Internal.s.pVMR0))
        return true;

    RTAssertMsg1Weak("AssertOther", iLine, pszFile, pszFunction);
    RTAssertPanic();
    return false;
}


/**
 * The Ring-0 Context Driver Helper Callbacks.
 */
extern DECLEXPORT(const PDMDRVHLPR0) g_pdmR0DrvHlp =
{
    PDM_DRVHLPRC_VERSION,
    pdmR0DrvHlp_VMSetError,
    pdmR0DrvHlp_VMSetErrorV,
    pdmR0DrvHlp_VMSetRuntimeError,
    pdmR0DrvHlp_VMSetRuntimeErrorV,
    pdmR0DrvHlp_AssertEMT,
    pdmR0DrvHlp_AssertOther,
    PDM_DRVHLPRC_VERSION
};

/** @} */




/**
 * Sets an irq on the PIC and I/O APIC.
 *
 * @returns true if delivered, false if postponed.
 * @param   pGVM        The global (ring-0) VM structure.
 * @param   iIrq        The irq.
 * @param   iLevel      The new level.
 * @param   uTagSrc     The IRQ tag and source.
 *
 * @remarks The caller holds the PDM lock.
 */
static bool pdmR0IsaSetIrq(PGVM pGVM, int iIrq, int iLevel, uint32_t uTagSrc)
{
    if (RT_LIKELY(    (   pGVM->pdm.s.IoApic.pDevInsR0
                       || !pGVM->pdm.s.IoApic.pDevInsR3)
                  &&  (   pGVM->pdm.s.Pic.pDevInsR0
                       || !pGVM->pdm.s.Pic.pDevInsR3)))
    {
        if (pGVM->pdm.s.Pic.pDevInsR0)
            pGVM->pdm.s.Pic.pfnSetIrqR0(pGVM->pdm.s.Pic.pDevInsR0, iIrq, iLevel, uTagSrc);
        if (pGVM->pdm.s.IoApic.pDevInsR0)
            pGVM->pdm.s.IoApic.pfnSetIrqR0(pGVM->pdm.s.IoApic.pDevInsR0, iIrq, iLevel, uTagSrc);
        return true;
    }

    /* queue for ring-3 execution. */
    PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)PDMQueueAlloc(pGVM->pdm.s.pDevHlpQueueR0);
    AssertReturn(pTask, false);

    pTask->enmOp = PDMDEVHLPTASKOP_ISA_SET_IRQ;
    pTask->pDevInsR3 = NIL_RTR3PTR; /* not required */
    pTask->u.IsaSetIRQ.iIrq = iIrq;
    pTask->u.IsaSetIRQ.iLevel = iLevel;
    pTask->u.IsaSetIRQ.uTagSrc = uTagSrc;

    PDMQueueInsertEx(pGVM->pdm.s.pDevHlpQueueR0, &pTask->Core, 0);
    return false;
}


/**
 * PDMDevHlpCallR0 helper.
 *
 * @returns See PFNPDMDEVREQHANDLERR0.
 * @param   pGVM    The global (ring-0) VM structure. (For validation.)
 * @param   pReq    Pointer to the request buffer.
 */
VMMR0_INT_DECL(int) PDMR0DeviceCallReqHandler(PGVM pGVM, PPDMDEVICECALLREQHANDLERREQ pReq)
{
    /*
     * Validate input and make the call.
     */
    int rc = GVMMR0ValidateGVM(pGVM);
    if (RT_SUCCESS(rc))
    {
        AssertPtrReturn(pReq, VERR_INVALID_POINTER);
        AssertMsgReturn(pReq->Hdr.cbReq == sizeof(*pReq), ("%#x != %#x\n", pReq->Hdr.cbReq, sizeof(*pReq)), VERR_INVALID_PARAMETER);

        PPDMDEVINS pDevIns = pReq->pDevInsR0;
        AssertPtrReturn(pDevIns, VERR_INVALID_POINTER);
        AssertReturn(pDevIns->Internal.s.pGVM == pGVM, VERR_INVALID_PARAMETER);

        PFNPDMDEVREQHANDLERR0 pfnReqHandlerR0 = pReq->pfnReqHandlerR0;
        AssertPtrReturn(pfnReqHandlerR0, VERR_INVALID_POINTER);

        rc = pfnReqHandlerR0(pDevIns, pReq->uOperation, pReq->u64Arg);
    }
    return rc;
}


/**
 * Worker for PDMR0DeviceCreate that does the actual instantiation.
 *
 * Allocates a memory object and devides it up as follows:
 * @verbatim
 *   ----------------------
 *   ring-0 devins
 *   ----------------------
 *   ring-0 instance data
 *   ----------------------
 *   page alignment padding
 *   ----------------------
 *   ring-3 devins
 *   ----------------------
 *   ring-3 instance data
 *   ----------------------
 *  [page alignment padding] -
 *  [----------------------]  \
 *  [raw-mode devins       ]   - Optional, only when raw-mode is enabled.
 *  [----------------------]  /
 *  [raw-mode instance data] -
 *   ----------------------
 *   shared instance data
 *   ----------------------
 *   default crit section
 *   ----------------------
 * @endverbatim
 *
 * @returns VBox status code.
 * @param   pGVM            The global (ring-0) VM structure.
 * @param   pDevReg         The device registration structure.
 * @param   iInstance       The device instance number.
 * @param   cbInstanceR3    The size of the ring-3 instance data.
 * @param   cbInstanceRC    The size of the raw-mode instance data.
 * @param   hMod            The module implementing the device.  On success, the
 * @param   RCPtrMapping    The raw-mode context mapping address, NIL_RTGCPTR if
 *                          not to include raw-mode.
 * @param   ppDevInsR3      Where to return the ring-3 device instance address.
 * @thread  EMT(0)
 */
static int pdmR0DeviceCreateWorker(PGVM pGVM, PCPDMDEVREGR0 pDevReg, uint32_t iInstance, uint32_t cbInstanceR3,
                                   uint32_t cbInstanceRC, RTRGPTR RCPtrMapping, void *hMod, PPDMDEVINSR3 *ppDevInsR3)
{
    /*
     * Check that the instance number isn't a duplicate.
     */
    for (size_t i = 0; i < pGVM->pdmr0.s.cDevInstances; i++)
    {
        PPDMDEVINS pCur = pGVM->pdmr0.s.apDevInstances[i];
        AssertLogRelReturn(!pCur || pCur->pReg != pDevReg || pCur->iInstance != iInstance, VERR_DUPLICATE);
    }

    /*
     * Figure out how much memory we need and allocate it.
     */
    uint32_t const cbRing0  = RT_ALIGN_32(RT_UOFFSETOF(PDMDEVINSR0, achInstanceData) + pDevReg->cbInstanceCC, PAGE_SIZE);
    uint32_t const cbRing3  = RT_ALIGN_32(RT_UOFFSETOF(PDMDEVINSR3, achInstanceData) + cbInstanceR3,
                                          RCPtrMapping != NIL_RTRGPTR ? PAGE_SIZE : 64);
    uint32_t const cbRC     = RCPtrMapping != NIL_RTRGPTR ? 0
                            : RT_ALIGN_32(RT_UOFFSETOF(PDMDEVINSRC, achInstanceData) + cbInstanceRC, 64);
    uint32_t const cbShared = RT_ALIGN_32(pDevReg->cbInstanceShared, 64);
    uint32_t const cbTotal  = RT_ALIGN_32(cbRing0 + cbRing3 + cbRC + cbShared + sizeof(PDMCRITSECT), PAGE_SIZE);

    RTR0MEMOBJ hMemObj;
    int rc = RTR0MemObjAllocPage(&hMemObj, cbTotal, false /*fExecutable*/);
    if (RT_FAILURE(rc))
        return rc;
    RT_BZERO(RTR0MemObjAddress(hMemObj), cbTotal);

    /* Map it. */
    RTR0MEMOBJ hMapObj;
    rc = RTR0MemObjMapUserEx(&hMapObj, hMemObj, (RTR3PTR)-1, 0, RTMEM_PROT_READ | RTMEM_PROT_WRITE, RTR0ProcHandleSelf(),
                             cbRing0, cbTotal - cbRing0);
    if (RT_SUCCESS(rc))
    {
        PPDMDEVINSR0        pDevIns   = (PPDMDEVINSR0)RTR0MemObjAddress(hMemObj);
        struct PDMDEVINSR3 *pDevInsR3 = (struct PDMDEVINSR3 *)((uint8_t *)pDevIns + cbRing0);

        /*
         * Initialize the ring-0 instance.
         */
        pDevIns->u32Version             = PDM_DEVINSR0_VERSION;
        pDevIns->iInstance              = iInstance;
        pDevIns->pHlpR0                 = &g_pdmR0DevHlp;
        pDevIns->pvInstanceDataR0       = (uint8_t *)pDevIns + cbRing0 + cbRing3 + cbRC;
        pDevIns->pvInstanceDataForR0    = &pDevIns->achInstanceData[0];
        pDevIns->pCritSectRoR0          = (PPDMCRITSECT)(  (uint8_t *)pDevIns->pvInstanceDataR0
                                                         + RT_ALIGN_32(pDevReg->cbInstanceShared, 64));
        pDevIns->pReg                   = pDevReg;
        pDevIns->pDevInsForR3           = RTR0MemObjAddressR3(hMapObj);
        pDevIns->pDevInsForR3R0         = pDevInsR3;
        pDevIns->pvInstanceDataForR3R0  = &pDevInsR3->achInstanceData[0];
        pDevIns->Internal.s.pGVM        = pGVM;
        pDevIns->Internal.s.pRegR0      = pDevReg;
        pDevIns->Internal.s.hMod        = hMod;
        pDevIns->Internal.s.hMemObj     = hMemObj;
        pDevIns->Internal.s.hMapObj     = hMapObj;
        pDevIns->Internal.s.pInsR3R0    = pDevInsR3;
        pDevIns->Internal.s.pIntR3R0    = &pDevInsR3->Internal.s;

        /*
         * Initialize the ring-3 instance data as much as we can.
         */
        pDevInsR3->u32Version           = PDM_DEVINSR3_VERSION;
        pDevInsR3->iInstance            = iInstance;
        pDevInsR3->cbRing3              = cbTotal - cbRing0;
        pDevInsR3->fR0Enabled           = true;
        pDevInsR3->fRCEnabled           = RCPtrMapping != NIL_RTRGPTR;
        pDevInsR3->pvInstanceDataR3     = pDevIns->pDevInsForR3 + cbRing3 + cbRC;
        pDevInsR3->pvInstanceDataForR3  = pDevIns->pDevInsForR3 + RT_UOFFSETOF(PDMDEVINSR3, achInstanceData);
        pDevInsR3->pCritSectRoR3        = pDevIns->pDevInsForR3 + cbRing3 + cbRC + cbShared;
        pDevInsR3->pDevInsR0RemoveMe    = pDevIns;
        pDevInsR3->pvInstanceDataR0     = pDevIns->pvInstanceDataR0;
        pDevInsR3->pvInstanceDataRC     = RCPtrMapping == NIL_RTRGPTR
                                        ? NIL_RTRGPTR : pDevIns->pDevInsForRC + RT_UOFFSETOF(PDMDEVINSRC, achInstanceData);
        pDevInsR3->pDevInsForRC         = pDevIns->pDevInsForRC;
        pDevInsR3->pDevInsForRCR3       = pDevIns->pDevInsForR3 + cbRing3;
        pDevInsR3->pDevInsForRCR3       = pDevInsR3->pDevInsForRCR3 + RT_UOFFSETOF(PDMDEVINSRC, achInstanceData);

        pDevInsR3->Internal.s.pVMR3     = pGVM->pVMR3;
        pDevInsR3->Internal.s.fIntFlags = RCPtrMapping == NIL_RTRGPTR ? PDMDEVINSINT_FLAGS_R0_ENABLED
                                        : PDMDEVINSINT_FLAGS_R0_ENABLED | PDMDEVINSINT_FLAGS_RC_ENABLED;

        /*
         * Initialize the raw-mode instance data as much as possible.
         */
        if (RCPtrMapping != NIL_RTRGPTR)
        {
            struct PDMDEVINSRC *pDevInsRC = RCPtrMapping == NIL_RTRGPTR ? NULL
                                          : (struct PDMDEVINSRC *)((uint8_t *)pDevIns + cbRing0 + cbRing3);

            pDevIns->pDevInsForRC           = RCPtrMapping;
            pDevIns->pDevInsForRCR0         = pDevInsRC;
            pDevIns->pvInstanceDataForRCR0  = &pDevInsRC->achInstanceData[0];

            pDevInsRC->u32Version           = PDM_DEVINSRC_VERSION;
            pDevInsRC->iInstance            = iInstance;
            pDevInsRC->pvInstanceDataRC     = pDevIns->pDevInsForRC + cbRC;
            pDevInsRC->pvInstanceDataForRC  = pDevIns->pDevInsForRC + RT_UOFFSETOF(PDMDEVINSRC, achInstanceData);
            pDevInsRC->pCritSectRoRC        = pDevIns->pDevInsForRC + cbRC + cbShared;
            pDevInsRC->Internal.s.pVMRC     = pGVM->pVMRC;
        }

        /*
         * Add to the device instance array and set its handle value.
         */
        AssertCompile(sizeof(pGVM->pdmr0.padding) == sizeof(pGVM->pdmr0));
        uint32_t idxR0Device = pGVM->pdmr0.s.cDevInstances;
        if (idxR0Device < RT_ELEMENTS(pGVM->pdmr0.s.apDevInstances))
        {
            pGVM->pdmr0.s.apDevInstances[idxR0Device] = pDevIns;
            pGVM->pdmr0.s.cDevInstances = idxR0Device + 1;
            pDevIns->Internal.s.idxR0Device   = idxR0Device;
            pDevInsR3->Internal.s.idxR0Device = idxR0Device;

            /*
             * Call the early constructor if present.
             */
            if (pDevReg->pfnEarlyConstruct)
                rc = pDevReg->pfnEarlyConstruct(pDevIns);
            if (RT_SUCCESS(rc))
            {
                /*
                 * We're done.
                 */
                *ppDevInsR3 = RTR0MemObjAddressR3(hMapObj);
                return rc;
            }

            /*
             * Bail out.
             */
            if (pDevIns->pReg->pfnFinalDestruct)
                pDevIns->pReg->pfnFinalDestruct(pDevIns);

            pGVM->pdmr0.s.apDevInstances[idxR0Device] = NULL;
            Assert(pGVM->pdmr0.s.cDevInstances == idxR0Device + 1);
            pGVM->pdmr0.s.cDevInstances = idxR0Device;
        }

        RTR0MemObjFree(hMapObj, true);
    }
    RTR0MemObjFree(hMemObj, true);
    return rc;
}


/**
 * Used by ring-3 PDM to create a device instance that operates both in ring-3
 * and ring-0.
 *
 * Creates an instance of a device (for both ring-3 and ring-0, and optionally
 * raw-mode context).
 *
 * @returns VBox status code.
 * @param   pGVM    The global (ring-0) VM structure.
 * @param   pReq    Pointer to the request buffer.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) PDMR0DeviceCreateReqHandler(PGVM pGVM, PPDMDEVICECREATEREQ pReq)
{
    LogFlow(("PDMR0DeviceCreateReqHandler: %s in %s\n", pReq->szDevName, pReq->szModName));

    /*
     * Validate the request.
     */
    AssertReturn(pReq->Hdr.cbReq == sizeof(*pReq), VERR_INVALID_PARAMETER);
    pReq->pDevInsR3 = NIL_RTR3PTR;

    int rc = GVMMR0ValidateGVMandEMT(pGVM, 0);
    AssertRCReturn(rc, rc);

    AssertReturn(pReq->fFlags           != 0, VERR_INVALID_FLAGS);
    AssertReturn(pReq->fClass           != 0, VERR_WRONG_TYPE);
    AssertReturn(pReq->uSharedVersion   != 0, VERR_INVALID_PARAMETER);
    AssertReturn(pReq->cbInstanceShared != 0, VERR_INVALID_PARAMETER);
    size_t const cchDevName = RTStrNLen(pReq->szDevName, sizeof(pReq->szDevName));
    AssertReturn(cchDevName < sizeof(pReq->szDevName), VERR_NO_STRING_TERMINATOR);
    AssertReturn(cchDevName > 0, VERR_EMPTY_STRING);
    AssertReturn(cchDevName < RT_SIZEOFMEMB(PDMDEVREG, szName), VERR_NOT_FOUND);

    size_t const cchModName = RTStrNLen(pReq->szModName, sizeof(pReq->szModName));
    AssertReturn(cchModName < sizeof(pReq->szModName), VERR_NO_STRING_TERMINATOR);
    AssertReturn(cchModName > 0, VERR_EMPTY_STRING);
    AssertReturn(pReq->cbInstanceR3 <= _2M, VERR_OUT_OF_RANGE);
    AssertReturn(pReq->cbInstanceRC <= _512K, VERR_OUT_OF_RANGE);
    AssertReturn(pReq->iInstance < 1024, VERR_OUT_OF_RANGE);

    /*
     * Reference the module.
     */
    void *hMod = NULL;
    rc = SUPR0LdrModByName(pGVM->pSession, pReq->szModName, &hMod);
    if (RT_FAILURE(rc))
    {
        LogRel(("PDMR0DeviceCreateReqHandler: SUPR0LdrModByName(,%s,) failed: %Rrc\n", pReq->szModName, rc));
        return rc;
    }

    /*
     * Look for the the module and the device registration structure.
     */
    int rcLock = SUPR0LdrLock(pGVM->pSession);
    AssertRC(rc);

    rc = VERR_NOT_FOUND;
    PPDMDEVMODREGR0 pMod;
    RTListForEach(&g_PDMDevModList, pMod, PDMDEVMODREGR0, ListEntry)
    {
        if (pMod->hMod == hMod)
        {
            /*
             * Found the module. We can drop the loader lock now before we
             * search the devices it registers.
             */
            if (RT_SUCCESS(rcLock))
            {
                rcLock = SUPR0LdrUnlock(pGVM->pSession);
                AssertRC(rcLock);
            }
            rcLock = VERR_ALREADY_RESET;

            PCPDMDEVREGR0 *papDevRegs = pMod->papDevRegs;
            size_t         i          = pMod->cDevRegs;
            while (i-- > 0)
            {
                PCPDMDEVREGR0 pDevReg = papDevRegs[i];
                LogFlow(("PDMR0DeviceCreateReqHandler: candidate #%u: %s %#x\n", i, pReq->szDevName, pDevReg->u32Version));
                if (   PDM_VERSION_ARE_COMPATIBLE(pDevReg->u32Version, PDM_DEVREGR0_VERSION)
                    && pDevReg->szName[cchDevName] == '\0'
                    && memcmp(pDevReg->szName, pReq->szDevName, cchDevName) == 0)
                {

                    /*
                     * Found the device, now check whether it matches the ring-3 registration.
                     */
                    if (   pReq->uSharedVersion   == pDevReg->uSharedVersion
                        && pReq->cbInstanceShared == pDevReg->cbInstanceShared
                        && pReq->cbInstanceRC     == pDevReg->cbInstanceRC
                        && pReq->fFlags           == pDevReg->fFlags
                        && pReq->fClass           == pDevReg->fClass
                        && pReq->cMaxInstances    == pDevReg->cMaxInstances)
                    {
                        rc = pdmR0DeviceCreateWorker(pGVM, pDevReg, pReq->iInstance, pReq->fRCEnabled,
                                                     pReq->cbInstanceR3, pReq->cbInstanceRC, hMod,
                                                     &pReq->pDevInsR3);
                        if (RT_SUCCESS(rc))
                            hMod = NULL; /* keep the module reference */
                    }
                    else
                    {
                        LogRel(("PDMR0DeviceCreate: Ring-3 does not match ring-0 device registration (%s):\n"
                                "    uSharedVersion: %#x vs %#x\n"
                                "  cbInstanceShared: %#x vs %#x\n"
                                "      cbInstanceRC: %#x vs %#x\n"
                                "            fFlags: %#x vs %#x\n"
                                "            fClass: %#x vs %#x\n"
                                "     cMaxInstances: %#x vs %#x\n",
                                pReq->szDevName,
                                pReq->uSharedVersion,   pDevReg->uSharedVersion,
                                pReq->cbInstanceShared, pDevReg->cbInstanceShared,
                                pReq->cbInstanceRC,     pDevReg->cbInstanceRC,
                                pReq->fFlags,           pDevReg->fFlags,
                                pReq->fClass,           pDevReg->fClass,
                                pReq->cMaxInstances,    pDevReg->cMaxInstances));
                        rc = VERR_INCOMPATIBLE_CONFIG;
                    }
                }
            }
            break;
        }
    }

    if (RT_SUCCESS_NP(rcLock))
    {
        rcLock = SUPR0LdrUnlock(pGVM->pSession);
        AssertRC(rcLock);
    }
    SUPR0LdrModRelease(pGVM->pSession, hMod);
    return rc;
}


/**
 * Used by ring-3 PDM to call standard ring-0 device methods.
 *
 * @returns VBox status code.
 * @param   pGVM    The global (ring-0) VM structure.
 * @param   pReq    Pointer to the request buffer.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) PDMR0DeviceGenCallReqHandler(PGVM pGVM, PPDMDEVICEGENCALLREQ pReq)
{
    /*
     * Validate the request.
     */
    AssertReturn(pReq->Hdr.cbReq == sizeof(*pReq), VERR_INVALID_PARAMETER);

    int rc = GVMMR0ValidateGVMandEMT(pGVM, 0);
    AssertRCReturn(rc, rc);

    AssertReturn(pReq->idxR0Device < pGVM->pdmr0.s.cDevInstances, VERR_INVALID_HANDLE);
    PPDMDEVINSR0 pDevIns = pGVM->pdmr0.s.apDevInstances[pReq->idxR0Device];
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    AssertReturn(pDevIns->pDevInsForR3 == pReq->pDevInsR3, VERR_INVALID_HANDLE);

    /*
     * Make the call.
     */
    rc = VINF_SUCCESS /*VINF_NOT_IMPLEMENTED*/;
    switch (pReq->enmCall)
    {
        case PDMDEVICEGENCALL_CONSTRUCT:
            AssertMsgBreakStmt(pGVM->enmVMState < VMSTATE_CREATED, ("enmVMState=%d\n", pGVM->enmVMState), rc = VERR_INVALID_STATE);
            if (pDevIns->pReg->pfnConstruct)
                rc = pDevIns->pReg->pfnConstruct(pDevIns);
            break;

        case PDMDEVICEGENCALL_DESTRUCT:
            AssertMsgBreakStmt(pGVM->enmVMState < VMSTATE_CREATED || pGVM->enmVMState >= VMSTATE_DESTROYING,
                               ("enmVMState=%d\n", pGVM->enmVMState), rc = VERR_INVALID_STATE);
            if (pDevIns->pReg->pfnDestruct)
            {
                pDevIns->pReg->pfnDestruct(pDevIns);
                rc = VINF_SUCCESS;
            }
            break;

        default:
            AssertMsgFailed(("enmCall=%d\n", pReq->enmCall));
            rc = VERR_INVALID_FUNCTION;
            break;
    }

    return rc;
}


/**
 * Legacy device mode compatiblity.
 *
 * @returns VBox status code.
 * @param   pGVM    The global (ring-0) VM structure.
 * @param   pReq    Pointer to the request buffer.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) PDMR0DeviceCompatSetCritSectReqHandler(PGVM pGVM, PPDMDEVICECOMPATSETCRITSECTREQ pReq)
{
    /*
     * Validate the request.
     */
    AssertReturn(pReq->Hdr.cbReq == sizeof(*pReq), VERR_INVALID_PARAMETER);

    int rc = GVMMR0ValidateGVMandEMT(pGVM, 0);
    AssertRCReturn(rc, rc);

    AssertReturn(pReq->idxR0Device < pGVM->pdmr0.s.cDevInstances, VERR_INVALID_HANDLE);
    PPDMDEVINSR0 pDevIns = pGVM->pdmr0.s.apDevInstances[pReq->idxR0Device];
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    AssertReturn(pDevIns->pDevInsForR3 == pReq->pDevInsR3, VERR_INVALID_HANDLE);

    AssertReturn(pGVM->enmVMState == VMSTATE_CREATING, VERR_INVALID_STATE);

    /*
     * The critical section address can be in a few different places:
     *      1. shared data.
     *      2. nop section.
     *      3. pdm critsect.
     */
    PPDMCRITSECT pCritSect;
    if (pReq->pCritSectR3 == pGVM->pVMR3 + RT_UOFFSETOF(VM, pdm.s.NopCritSect))
    {
        pCritSect = &pGVM->pdm.s.NopCritSect;
        Log(("PDMR0DeviceCompatSetCritSectReqHandler: Nop - %p %#x\n", pCritSect, pCritSect->s.Core.u32Magic));
    }
    else if (pReq->pCritSectR3 == pGVM->pVMR3 + RT_UOFFSETOF(VM, pdm.s.CritSect))
    {
        pCritSect = &pGVM->pdm.s.CritSect;
        Log(("PDMR0DeviceCompatSetCritSectReqHandler: PDM - %p %#x\n", pCritSect, pCritSect->s.Core.u32Magic));
    }
    else
    {
        size_t offCritSect = pReq->pCritSectR3 - pDevIns->pDevInsForR3R0->pvInstanceDataR3;
        AssertLogRelMsgReturn(   offCritSect                       <  pDevIns->pReg->cbInstanceShared
                              && offCritSect + sizeof(PDMCRITSECT) <= pDevIns->pReg->cbInstanceShared,
                              ("offCritSect=%p pCritSectR3=%p cbInstanceShared=%#x (%s)\n",
                               offCritSect, pReq->pCritSectR3, pDevIns->pReg->cbInstanceShared, pDevIns->pReg->szName),
                              VERR_INVALID_POINTER);
        pCritSect = (PPDMCRITSECT)((uint8_t *)pDevIns->pvInstanceDataR0 + offCritSect);
        Log(("PDMR0DeviceCompatSetCritSectReqHandler: custom - %#x/%p %#x\n", offCritSect, pCritSect, pCritSect->s.Core.u32Magic));
    }
    AssertLogRelMsgReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC,
                          ("cs=%p magic=%#x dev=%s\n", pCritSect, pCritSect->s.Core.u32Magic, pDevIns->pReg->szName),
                          VERR_INVALID_MAGIC);

    /*
     * Make the update.
     */
    pDevIns->pCritSectRoR0 = pCritSect;

    return VINF_SUCCESS;
}


/**
 * Legacy device mode compatiblity.
 *
 * @returns VBox status code.
 * @param   pGVM    The global (ring-0) VM structure.
 * @param   pReq    Pointer to the request buffer.
 * @thread  EMT(0)
 */
VMMR0_INT_DECL(int) PDMR0DeviceCompatRegPciDevReqHandler(PGVM pGVM, PPDMDEVICECOMPATREGPCIDEVREQ pReq)
{
    /*
     * Validate the request.
     */
    AssertReturn(pReq->Hdr.cbReq == sizeof(*pReq), VERR_INVALID_PARAMETER);

    int rc = GVMMR0ValidateGVMandEMT(pGVM, 0);
    AssertRCReturn(rc, rc);

    AssertReturn(pReq->idxR0Device < pGVM->pdmr0.s.cDevInstances, VERR_INVALID_HANDLE);
    PPDMDEVINSR0 pDevIns = pGVM->pdmr0.s.apDevInstances[pReq->idxR0Device];
    AssertPtrReturn(pDevIns, VERR_INVALID_HANDLE);
    AssertReturn(pDevIns->pDevInsForR3 == pReq->pDevInsR3, VERR_INVALID_HANDLE);

    AssertReturn(pGVM->enmVMState == VMSTATE_CREATING, VERR_INVALID_STATE);

    /*
     * The address must be within the shared instance data.
     */
    size_t offPciDev = pReq->pPciDevR3 - pDevIns->pDevInsForR3R0->pvInstanceDataR3;
    AssertLogRelMsgReturn(   offPciDev                     <  pDevIns->pReg->cbInstanceShared
                          && offPciDev + sizeof(PDMPCIDEV) <= pDevIns->pReg->cbInstanceShared,
                          ("offPciDev=%p pPciDevR3=%p cbInstanceShared=%#x (%s)\n",
                           offPciDev, pReq->pPciDevR3, pDevIns->pReg->cbInstanceShared, pDevIns->pReg->szName),
                          VERR_INVALID_POINTER);
    PPDMPCIDEV pPciDev = (PPDMPCIDEV)((uint8_t *)pDevIns->pvInstanceDataR0 + offPciDev);
    AssertReturn(pPciDev->Int.s.pDevInsR3 == pReq->pDevInsR3, VERR_MISMATCH);

    /*
     * Append the pci device to the list.
     */
    PPDMPCIDEV pPciDevPrev = pDevIns->Internal.s.pHeadPciDevR0;
    if (!pPciDevPrev)
        pDevIns->Internal.s.pHeadPciDevR0 = pPciDev;
    else
    {
        while (pPciDevPrev->Int.s.pNextR0)
            pPciDevPrev = pPciDevPrev->Int.s.pNextR0;
        pPciDevPrev->Int.s.pNextR0 = pPciDev;
    }
    pPciDev->Int.s.pNextR0 = NULL;

    return VINF_SUCCESS;
}


/**
 * Registers the device implementations living in a module.
 *
 * This should normally only be called during ModuleInit().  The should be a
 * call to PDMR0DeviceDeregisterModule from the ModuleTerm() function to undo
 * the effects of this call.
 *
 * @returns VBox status code.
 * @param   hMod            The module handle of the module being registered.
 * @param   pModReg         The module registration structure.  This will be
 *                          used directly so it must live as long as the module
 *                          and be writable.
 *
 * @note    Caller must own the loader lock!
 */
VMMR0DECL(int) PDMR0DeviceRegisterModule(void *hMod, PPDMDEVMODREGR0 pModReg)
{
    /*
     * Validate the input.
     */
    AssertPtrReturn(hMod, VERR_INVALID_HANDLE);
    Assert(SUPR0LdrIsLockOwnerByMod(hMod, true));

    AssertPtrReturn(pModReg, VERR_INVALID_POINTER);
    AssertLogRelMsgReturn(PDM_VERSION_ARE_COMPATIBLE(pModReg->u32Version, PDM_DEVMODREGR0_VERSION),
                          ("pModReg->u32Version=%#x vs %#x\n", pModReg->u32Version, PDM_DEVMODREGR0_VERSION),
                          VERR_VERSION_MISMATCH);
    AssertLogRelMsgReturn(pModReg->cDevRegs <= 256 && pModReg->cDevRegs > 0, ("cDevRegs=%u\n", pModReg->cDevRegs),
                          VERR_OUT_OF_RANGE);
    AssertLogRelMsgReturn(pModReg->hMod == NULL, ("hMod=%p\n", pModReg->hMod), VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(pModReg->ListEntry.pNext == NULL, ("pNext=%p\n", pModReg->ListEntry.pNext), VERR_INVALID_PARAMETER);
    AssertLogRelMsgReturn(pModReg->ListEntry.pPrev == NULL, ("pPrev=%p\n", pModReg->ListEntry.pPrev), VERR_INVALID_PARAMETER);

    for (size_t i = 0; i < pModReg->cDevRegs; i++)
    {
        PCPDMDEVREGR0 pDevReg = pModReg->papDevRegs[i];
        AssertLogRelMsgReturn(RT_VALID_PTR(pDevReg), ("[%u]: %p\n", i, pDevReg), VERR_INVALID_POINTER);
        AssertLogRelMsgReturn(PDM_VERSION_ARE_COMPATIBLE(pDevReg->u32Version, PDM_DEVREGR0_VERSION),
                              ("pDevReg->u32Version=%#x vs %#x\n", pModReg->u32Version, PDM_DEVREGR0_VERSION), VERR_VERSION_MISMATCH);
        AssertLogRelMsgReturn(RT_VALID_PTR(pDevReg->pszDescription), ("[%u]: %p\n", i, pDevReg->pszDescription), VERR_INVALID_POINTER);
        AssertLogRelMsgReturn(pDevReg->uReserved0   == 0, ("[%u]: %#x\n", i, pDevReg->uReserved0),    VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->uReserved1   == 0, ("[%u]: %#x\n", i, pDevReg->uReserved1),    VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->fClass       != 0, ("[%u]: %#x\n", i, pDevReg->fClass),        VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->fFlags       != 0, ("[%u]: %#x\n", i, pDevReg->fFlags),        VERR_INVALID_PARAMETER);
        AssertLogRelMsgReturn(pDevReg->cMaxInstances > 0, ("[%u]: %#x\n", i, pDevReg->cMaxInstances), VERR_INVALID_PARAMETER);

        /* The name must be printable ascii and correctly terminated. */
        for (size_t off = 0; off < RT_ELEMENTS(pDevReg->szName); off++)
        {
            char ch = pDevReg->szName[off];
            AssertLogRelMsgReturn(RT_C_IS_PRINT(ch) || (ch == '\0' && off > 0),
                                  ("[%u]: off=%u  szName: %.*Rhxs\n", i, off, sizeof(pDevReg->szName), &pDevReg->szName[0]),
                                  VERR_INVALID_NAME);
            if (ch == '\0')
                break;
        }
    }

    /*
     * Add it, assuming we're being called at ModuleInit/ModuleTerm time only, or
     * that the caller has already taken the loader lock.
     */
    pModReg->hMod = hMod;
    RTListAppend(&g_PDMDevModList, &pModReg->ListEntry);

    return VINF_SUCCESS;
}


/**
 * Deregisters the device implementations living in a module.
 *
 * This should normally only be called during ModuleTerm().
 *
 * @returns VBox status code.
 * @param   hMod            The module handle of the module being registered.
 * @param   pModReg         The module registration structure.  This will be
 *                          used directly so it must live as long as the module
 *                          and be writable.
 *
 * @note    Caller must own the loader lock!
 */
VMMR0DECL(int) PDMR0DeviceDeregisterModule(void *hMod, PPDMDEVMODREGR0 pModReg)
{
    /*
     * Validate the input.
     */
    AssertPtrReturn(hMod, VERR_INVALID_HANDLE);
    Assert(SUPR0LdrIsLockOwnerByMod(hMod, true));

    AssertPtrReturn(pModReg, VERR_INVALID_POINTER);
    AssertLogRelMsgReturn(PDM_VERSION_ARE_COMPATIBLE(pModReg->u32Version, PDM_DEVMODREGR0_VERSION),
                          ("pModReg->u32Version=%#x vs %#x\n", pModReg->u32Version, PDM_DEVMODREGR0_VERSION),
                          VERR_VERSION_MISMATCH);
    AssertLogRelMsgReturn(pModReg->hMod == hMod || pModReg->hMod == NULL, ("pModReg->hMod=%p vs %p\n", pModReg->hMod, hMod),
                          VERR_INVALID_PARAMETER);

    /*
     * Unlink the registration record and return it to virgin conditions.  Ignore
     * the call if not registered.
     */
    if (pModReg->hMod)
    {
        pModReg->hMod = NULL;
        RTListNodeRemove(&pModReg->ListEntry);
        pModReg->ListEntry.pNext = NULL;
        pModReg->ListEntry.pPrev = NULL;
        return VINF_SUCCESS;
    }
    return VWRN_NOT_FOUND;
}

