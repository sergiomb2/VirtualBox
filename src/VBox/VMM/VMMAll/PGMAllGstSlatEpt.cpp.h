/* $Id$ */
/** @file
 * VBox - Page Manager, Guest EPT SLAT - All context code.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#if PGM_GST_TYPE == PGM_TYPE_EPT
DECLINLINE(bool) PGM_GST_SLAT_NAME_EPT(WalkIsPermValid)(PCVMCPUCC pVCpu, uint64_t uEntry)
{
    if (!(uEntry & VMX_BF_EPT_PT_READ_MASK))
    {
        if (uEntry & VMX_BF_EPT_PT_WRITE_MASK)
            return false;

        Assert(!pVCpu->CTX_SUFF(pVM)->cpum.ro.GuestFeatures.fVmxModeBasedExecuteEpt);
        if (   !RT_BF_GET(pVCpu->pgm.s.uEptVpidCapMsr, VMX_BF_EPT_VPID_CAP_EXEC_ONLY)
            && (uEntry & VMX_BF_EPT_PT_EXECUTE_MASK))
            return false;
    }
    return true;
}


DECLINLINE(bool) PGM_GST_SLAT_NAME_EPT(WalkIsMemTypeValid)(uint64_t uEntry, uint8_t uLevel)
{
    Assert(uLevel >= 3 && uLevel <= 1); NOREF(uLevel);
    uint64_t const fEptMemTypeMask = uEntry & VMX_BF_EPT_PT_MEMTYPE_MASK;
    if (   fEptMemTypeMask == EPT_E_MEMTYPE_INVALID_2
        || fEptMemTypeMask == EPT_E_MEMTYPE_INVALID_3
        || fEptMemTypeMask == EPT_E_MEMTYPE_INVALID_7)
        return false;
    return true;
}


DECLINLINE(int) PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(PCVMCPUCC pVCpu, PPGMPTWALK pWalk, uint64_t uEntry, uint8_t uLevel)
{
    static PGMSLATFAIL const s_aEptViolation[] = { PGMSLATFAIL_EPT_VIOLATION, PGMSLATFAIL_EPT_VIOLATION_CONVERTIBLE };
    uint8_t const fEptVeSupported  = pVCpu->CTX_SUFF(pVM)->cpum.ro.GuestFeatures.fVmxEptXcptVe;
    uint8_t const idxViolationType = fEptVeSupported & !RT_BF_GET(uEntry, VMX_BF_EPT_PT_SUPPRESS_VE);

    pWalk->fNotPresent = true;
    pWalk->uLevel      = uLevel;
    pWalk->enmSlatFail = s_aEptViolation[idxViolationType];
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


DECLINLINE(int) PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(PCVMCPUCC pVCpu, PPGMPTWALK pWalk, uint8_t uLevel, int rc)
{
    AssertMsg(rc == VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS, ("%Rrc\n", rc)); NOREF(rc); NOREF(pVCpu);
    pWalk->fBadPhysAddr = true;
    pWalk->uLevel       = uLevel;
    pWalk->enmSlatFail  = PGMSLATFAIL_EPT_VIOLATION;
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


DECLINLINE(int) PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(PVMCPUCC pVCpu, PPGMPTWALK pWalk, uint8_t uLevel)
{
    NOREF(pVCpu);
    pWalk->fRsvdError  = true;
    pWalk->uLevel      = uLevel;
    pWalk->enmSlatFail = PGMSLATFAIL_EPT_MISCONFIG;
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


DECLINLINE(int) PGM_GST_SLAT_NAME_EPT(Walk)(PVMCPUCC pVCpu, RTGCPHYS GCPhysNested, bool fIsLinearAddrValid, RTGCPTR GCPtrNested,
                                            PPGMPTWALK pWalk, PGSTPTWALK pGstWalk)
{
    /*
     * Init walk structures.
     */
    RT_ZERO(*pWalk);
    RT_ZERO(*pGstWalk);

    pWalk->GCPtr              = GCPtrNested;
    pWalk->GCPhysNested       = GCPhysNested;
    pWalk->fIsLinearAddrValid = fIsLinearAddrValid;
    pWalk->fIsSlat            = true;

    /*
     * Figure out EPT attributes that are cumulative (logical-AND) across page walks.
     *   - R, W, X_SUPER are unconditionally cumulative.
     *     See Intel spec. Table 26-7 "Exit Qualification for EPT Violations".
     *
     *   - X_USER is Cumulative but relevant only when mode-based execute control for EPT
     *     which we currently don't support it (asserted below).
     *
     *   - MEMTYPE is not cumulative and only applicable to the final paging entry.
     *
     *   - A, D EPT bits map to the regular page-table bit positions. Thus, they're not
     *     included in the mask below and handled separately. Accessed bits are
     *     cumulative but dirty bits are not cumulative as they're only applicable to
     *     the final paging entry.
     */
    Assert(!pVCpu->CTX_SUFF(pVM)->cpum.ro.GuestFeatures.fVmxModeBasedExecuteEpt);
    uint64_t const fCumulativeEpt = PGM_PTATTRS_EPT_R_MASK
                                  | PGM_PTATTRS_EPT_W_MASK
                                  | PGM_PTATTRS_EPT_X_SUPER_MASK;

    /*
     * Do the walk.
     */
    int const rc2 = pgmGstGetEptPML4PtrEx(pVCpu, &pGstWalk->pPml4);
    if (RT_SUCCESS(rc2))
    { /* likely */ }
    else
        return PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(pVCpu, pWalk, 4, rc2);

    uint64_t fEffective;
    {
        /*
         * PML4E.
         */
        PEPTPML4E pPml4e;
        pGstWalk->pPml4e = pPml4e = &pGstWalk->pPml4->a[(GCPhysNested >> EPT_PML4_SHIFT) & EPT_PML4_MASK];
        EPTPML4E  Pml4e;
        pGstWalk->Pml4e.u = Pml4e.u = pPml4e->u;

        if (GST_IS_PGENTRY_PRESENT(pVCpu, Pml4e)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(pVCpu, pWalk, Pml4e.u, 4);

        if (RT_LIKELY(GST_IS_PML4E_VALID(pVCpu, Pml4e))) { /* likely */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 4);

        Assert(!pVCpu->CTX_SUFF(pVM)->cpum.ro.GuestFeatures.fVmxModeBasedExecuteEpt);
        uint64_t const fEptAttrs     = Pml4e.u & EPT_PML4E_ATTR_MASK;
        uint8_t const fRead          = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_READ);
        uint8_t const fWrite         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_WRITE);
        uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
        uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
        fEffective = RT_BF_MAKE(PGM_PTATTRS_R, fRead)
                   | RT_BF_MAKE(PGM_PTATTRS_W, fWrite)
                   | RT_BF_MAKE(PGM_PTATTRS_A, fAccessed)
                   | fEffectiveEpt;
        pWalk->fEffective = fEffective;

        int const rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, Pml4e.u & EPT_PML4E_PG_MASK, &pGstWalk->pPdpt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(pVCpu, pWalk, 3, rc);
    }
    {
        /*
         * PDPTE.
         */
        PEPTPDPTE pPdpte;
        pGstWalk->pPdpte = pPdpte = &pGstWalk->pPdpt->a[(GCPhysNested >> GST_PDPT_SHIFT) & GST_PDPT_MASK];
        EPTPDPTE  Pdpte;
        pGstWalk->Pdpte.u = Pdpte.u = pPdpte->u;

        if (GST_IS_PGENTRY_PRESENT(pVCpu, Pdpte)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(pVCpu, pWalk, Pdpte.u, 3);

        /* The order of the following 2 "if" statements matter. */
        if (GST_IS_PDPE_VALID(pVCpu, Pdpte))
        {
            uint64_t const fEptAttrs     = Pdpte.u & EPT_PDPTE_ATTR_MASK;
            uint8_t const fRead          = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_READ);
            uint8_t const fWrite         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_WRITE);
            uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
            uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
            fEffective &= RT_BF_MAKE(PGM_PTATTRS_R, fRead)
                        | RT_BF_MAKE(PGM_PTATTRS_W, fWrite)
                        | RT_BF_MAKE(PGM_PTATTRS_A, fAccessed)
                        | (fEffectiveEpt & fCumulativeEpt);
            pWalk->fEffective = fEffective;
        }
        else if (   GST_IS_BIG_PDPE_VALID(pVCpu, Pdpte)
                 && PGM_GST_SLAT_NAME_EPT(WalkIsMemTypeValid)(Pdpte.u, 3))
        {
            uint64_t const fEptAttrs     = Pdpte.u & EPT_PDPTE1G_ATTR_MASK;
            uint8_t const fRead          = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_READ);
            uint8_t const fWrite         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_WRITE);
            uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
            uint8_t const fDirty         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_DIRTY);
            uint8_t const fMemType       = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_MEMTYPE);
            uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
            fEffective &= RT_BF_MAKE(PGM_PTATTRS_R,           fRead)
                        | RT_BF_MAKE(PGM_PTATTRS_W,           fWrite)
                        | RT_BF_MAKE(PGM_PTATTRS_A,           fAccessed)
                        | (fEffectiveEpt & fCumulativeEpt);
            fEffective |= RT_BF_MAKE(PGM_PTATTRS_D,           fDirty)
                        | RT_BF_MAKE(PGM_PTATTRS_EPT_MEMTYPE, fMemType);
            pWalk->fEffective = fEffective;

            pWalk->fGigantPage  = true;
            pWalk->fSucceeded   = true;
            pWalk->GCPhys       = GST_GET_BIG_PDPE_GCPHYS(pVCpu->CTX_SUFF(pVM), Pdpte)
                                     | (GCPhysNested & GST_GIGANT_PAGE_OFFSET_MASK);
            PGM_A20_APPLY_TO_VAR(pVCpu, pWalk->GCPhys);
            return VINF_SUCCESS;
        }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 3);

        int const rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, Pdpte.u & EPT_PDPTE_PG_MASK, &pGstWalk->pPd);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(pVCpu, pWalk, 3, rc);
    }
    {
        /*
         * PDE.
         */
        PGSTPDE pPde;
        pGstWalk->pPde  = pPde  = &pGstWalk->pPd->a[(GCPhysNested >> GST_PD_SHIFT) & GST_PD_MASK];
        GSTPDE  Pde;
        pGstWalk->Pde.u = Pde.u = pPde->u;

        if (GST_IS_PGENTRY_PRESENT(pVCpu, Pde)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(pVCpu, pWalk, Pde.u, 2);

        /* The order of the following 2 "if" statements matter. */
        if (GST_IS_PDE_VALID(pVCpu, Pde))
        {
            uint64_t const fEptAttrs     = Pde.u & EPT_PDE_ATTR_MASK;
            uint8_t const fRead          = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_READ);
            uint8_t const fWrite         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_WRITE);
            uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
            uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
            fEffective &= RT_BF_MAKE(PGM_PTATTRS_R, fRead)
                        | RT_BF_MAKE(PGM_PTATTRS_W, fWrite)
                        | RT_BF_MAKE(PGM_PTATTRS_A, fAccessed)
                        | (fEffectiveEpt & fCumulativeEpt);
            pWalk->fEffective = fEffective;

        }
        else if (   GST_IS_BIG_PDE_VALID(pVCpu, Pde)
                 && PGM_GST_SLAT_NAME_EPT(WalkIsMemTypeValid)(Pde.u, 2))
        {
            uint64_t const fEptAttrs     = Pde.u & EPT_PDE2M_ATTR_MASK;
            uint8_t const fRead          = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_READ);
            uint8_t const fWrite         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_WRITE);
            uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
            uint8_t const fDirty         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_DIRTY);
            uint8_t const fMemType       = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_MEMTYPE);
            uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
            fEffective &= RT_BF_MAKE(PGM_PTATTRS_R,           fRead)
                        | RT_BF_MAKE(PGM_PTATTRS_W,           fWrite)
                        | RT_BF_MAKE(PGM_PTATTRS_A,           fAccessed)
                        | (fEffectiveEpt & fCumulativeEpt);
            fEffective |= RT_BF_MAKE(PGM_PTATTRS_D,           fDirty)
                        | RT_BF_MAKE(PGM_PTATTRS_EPT_MEMTYPE, fMemType);
            pWalk->fEffective = fEffective;

            pWalk->fBigPage     = true;
            pWalk->fSucceeded   = true;
            pWalk->GCPhys       = GST_GET_BIG_PDE_GCPHYS(pVCpu->CTX_SUFF(pVM), Pde)
                                     | (GCPhysNested & GST_BIG_PAGE_OFFSET_MASK);
            PGM_A20_APPLY_TO_VAR(pVCpu, pWalk->GCPhys);
            return VINF_SUCCESS;
        }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 2);

        int const rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GST_GET_PDE_GCPHYS(Pde), &pGstWalk->pPt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnBadPhysAddr)(pVCpu, pWalk, 1, rc);
    }
    {
        /*
         * PTE.
         */
        PGSTPTE pPte;
        pGstWalk->pPte  = pPte  = &pGstWalk->pPt->a[(GCPhysNested >> GST_PT_SHIFT) & GST_PT_MASK];
        GSTPTE  Pte;
        pGstWalk->Pte.u = Pte.u = pPte->u;

        if (GST_IS_PGENTRY_PRESENT(pVCpu, Pte)) { /* probable */ }
        else return PGM_GST_SLAT_NAME_EPT(WalkReturnNotPresent)(pVCpu, pWalk, Pte.u, 1);

        if (   GST_IS_PTE_VALID(pVCpu, Pte)
            && PGM_GST_SLAT_NAME_EPT(WalkIsMemTypeValid)(Pte.u, 1))
        { /* likely*/  }
        else
            return PGM_GST_SLAT_NAME_EPT(WalkReturnRsvdError)(pVCpu, pWalk, 1);

        uint64_t const fEptAttrs     = Pte.u & EPT_PTE_ATTR_MASK;
        uint8_t const fRead          = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_READ);
        uint8_t const fWrite         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_WRITE);
        uint8_t const fAccessed      = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_ACCESSED);
        uint8_t const fDirty         = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_DIRTY);
        uint8_t const fMemType       = RT_BF_GET(fEptAttrs, VMX_BF_EPT_PT_MEMTYPE);
        uint64_t const fEffectiveEpt = (fEptAttrs << PGM_PTATTRS_EPT_SHIFT) & PGM_PTATTRS_EPT_MASK;
        fEffective &= RT_BF_MAKE(PGM_PTATTRS_R,           fRead)
                    | RT_BF_MAKE(PGM_PTATTRS_W,           fWrite)
                    | RT_BF_MAKE(PGM_PTATTRS_A,           fAccessed)
                    | (fEffectiveEpt & fCumulativeEpt);
        fEffective |= RT_BF_MAKE(PGM_PTATTRS_D,           fDirty)
                    | RT_BF_MAKE(PGM_PTATTRS_EPT_MEMTYPE, fMemType);
        pWalk->fEffective = fEffective;

        pWalk->fSucceeded   = true;
        pWalk->GCPhys       = GST_GET_PTE_GCPHYS(Pte) | (GCPhysNested & PAGE_OFFSET_MASK);
        return VINF_SUCCESS;
    }
}
#else
# error "Guest paging type must be EPT."
#endif

