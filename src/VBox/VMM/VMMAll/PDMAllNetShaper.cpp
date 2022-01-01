/* $Id$ */
/** @file
 * PDM Network Shaper - Limit network traffic according to bandwidth group settings.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_NET_SHAPER
#include <VBox/vmm/pdm.h>
#include <VBox/log.h>
#include <iprt/time.h>

#include <VBox/vmm/pdmnetshaper.h>
#include "PDMNetShaperInternal.h"


/**
 * Obtain bandwidth in a bandwidth group.
 *
 * @returns True if bandwidth was allocated, false if not.
 * @param   pVM             The cross context VM structure.
 * @param   pFilter         Pointer to the filter that allocates bandwidth.
 * @param   cbTransfer      Number of bytes to allocate.
 */
VMM_INT_DECL(bool) PDMNetShaperAllocateBandwidth(PVMCC pVM, PPDMNSFILTER pFilter, size_t cbTransfer)
{
    AssertPtrReturn(pFilter, true);
    if (!RT_VALID_PTR(pFilter->CTX_SUFF(pBwGroup)))
        return true;

    PPDMNSBWGROUP pBwGroup = ASMAtomicReadPtrT(&pFilter->CTX_SUFF(pBwGroup), PPDMNSBWGROUP);
    int rc = PDMCritSectEnter(pVM, &pBwGroup->Lock, VERR_SEM_BUSY); AssertRC(rc);
    if (RT_SUCCESS(rc))
    { /* likely */ }
    else
    {
        if (rc == VERR_SEM_BUSY)
            return true;
        PDM_CRITSECT_RELEASE_ASSERT_RC(pVM, &pBwGroup->Lock, rc);
        return false;
    }

    bool fAllowed = true;
    if (pBwGroup->cbPerSecMax)
    {
        /* Re-fill the bucket first */
        uint64_t tsNow        = RTTimeSystemNanoTS();
        uint32_t uTokensAdded = (tsNow - pBwGroup->tsUpdatedLast) * pBwGroup->cbPerSecMax / (1000 * 1000 * 1000);
        uint32_t uTokens      = RT_MIN(pBwGroup->cbBucket, uTokensAdded + pBwGroup->cbTokensLast);

        if (cbTransfer > uTokens)
        {
            fAllowed = false;
            ASMAtomicWriteBool(&pFilter->fChoked, true);
        }
        else
        {
            pBwGroup->tsUpdatedLast = tsNow;
            pBwGroup->cbTokensLast = uTokens - (uint32_t)cbTransfer;
        }
        Log2(("pdmNsAllocateBandwidth: BwGroup=%#p{%s} cbTransfer=%u uTokens=%u uTokensAdded=%u fAllowed=%RTbool\n",
              pBwGroup, R3STRING(pBwGroup->pszNameR3), cbTransfer, uTokens, uTokensAdded, fAllowed));
    }
    else
        Log2(("pdmNsAllocateBandwidth: BwGroup=%#p{%s} disabled fAllowed=%RTbool\n",
              pBwGroup, R3STRING(pBwGroup->pszNameR3), fAllowed));

    rc = PDMCritSectLeave(pVM, &pBwGroup->Lock); AssertRC(rc);
    return fAllowed;
}

