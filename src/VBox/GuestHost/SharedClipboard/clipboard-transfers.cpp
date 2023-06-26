/* $Id$ */
/** @file
 * Shared Clipboard: Common clipboard transfer handling code.
 */

/*
 * Copyright (C) 2019-2023 Oracle and/or its affiliates.
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

#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/log.h>

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/path.h>
#include <iprt/rand.h>
#include <iprt/semaphore.h>
#include <iprt/uri.h>

#include <VBox/err.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/GuestHost/SharedClipboard-transfers.h>


static int shClTransferThreadCreate(PSHCLTRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser);
static int shClTransferThreadDestroy(PSHCLTRANSFER pTransfer, RTMSINTERVAL uTimeoutMs);

static void shclTransferCtxTransferRemoveAndUnregister(PSHCLTRANSFERCTX pTransferCtx, PSHCLTRANSFER pTransfer);
static PSHCLTRANSFER shClTransferCtxGetTransferByIdInternal(PSHCLTRANSFERCTX pTransferCtx, uint32_t uId);
static PSHCLTRANSFER shClTransferCtxGetTransferByIndexInternal(PSHCLTRANSFERCTX pTransferCtx, uint32_t uIdx);


/*********************************************************************************************************************************
 * Transfer List                                                                                                                 *
 ********************************************************************************************************************************/

/**
 * Initializes a transfer list.
 *
 * @param   pList               Transfer list to initialize.
 */
void ShClTransferListInit(PSHCLLIST pList)
{
    RT_ZERO(pList->Hdr);
    RTListInit(&pList->lstEntries);
}

/**
 * Destroys a transfer list.
 *
 * @param   pList               Transfer list to destroy.
 */
void ShClTransferListDestroy(PSHCLLIST pList)
{
    if (!pList)
        return;

    PSHCLLISTENTRY pEntry, pEntryNext;
    RTListForEachSafe(&pList->lstEntries, pEntry, pEntryNext, SHCLLISTENTRY, Node)
    {
        RTListNodeRemove(&pEntry->Node);
        ShClTransferListEntryDestroy(pEntry);
        RTMemFree(pEntry);
    }

    RT_ZERO(pList->Hdr);
}

/**
 * Adds a list entry to a transfer list.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to add entry to.
 * @param   pEntry              Entry to add.
 * @param   fAppend             \c true to append to a list, or \c false to prepend.
 */
int ShClTransferListAddEntry(PSHCLLIST pList, PSHCLLISTENTRY pEntry, bool fAppend)
{
    AssertReturn(ShClTransferListEntryIsValid(pEntry), VERR_INVALID_PARAMETER);

    if (fAppend)
        RTListAppend(&pList->lstEntries, &pEntry->Node);
    else
        RTListPrepend(&pList->lstEntries, &pEntry->Node);
    pList->Hdr.cEntries++;

    LogFlowFunc(("%p: '%s' (%RU32 bytes) + %RU32 bytes info -> now %RU32 entries\n",
                 pList, pEntry->pszName, pEntry->cbName, pEntry->cbInfo, pList->Hdr.cEntries));

    return VINF_SUCCESS;
}

/**
 * Allocates a new transfer list.
 *
 * @returns Allocated transfer list on success, or NULL on failure.
 */
PSHCLLIST ShClTransferListAlloc(void)
{
    PSHCLLIST pList = (PSHCLLIST)RTMemAllocZ(sizeof(SHCLLIST));
    if (pList)
    {
        ShClTransferListInit(pList);
        return pList;
    }

    return NULL;
}

/**
 * Frees a transfer list.
 *
 * @param   pList               Transfer list to free. The pointer will be
 *                              invalid after returning from this function.
 */
void ShClTransferListFree(PSHCLLIST pList)
{
    if (!pList)
        return;

    ShClTransferListDestroy(pList);

    RTMemFree(pList);
    pList = NULL;
}

/**
 * Returns a specific list entry of a transfer list.
 *
 * @returns Pointer to list entry if found, or NULL if not found.
 * @param   pList               Clipboard transfer list to get list entry from.
 * @param   uIdx                Index of list entry to return.
 */
DECLINLINE(PSHCLLISTENTRY) shClTransferListGetEntryById(PSHCLLIST pList, uint32_t uIdx)
{
    if (uIdx >= pList->Hdr.cEntries)
        return NULL;

    Assert(!RTListIsEmpty(&pList->lstEntries));

    PSHCLLISTENTRY pIt = RTListGetFirst(&pList->lstEntries, SHCLLISTENTRY, Node);
    while (uIdx) /** @todo Slow, but works for now. */
    {
        pIt = RTListGetNext(&pList->lstEntries, pIt, SHCLLISTENTRY, Node);
        uIdx--;
    }

    return pIt;
}

/**
 * Initializes an list handle info structure.
 *
 * @returns VBox status code.
 * @param   pInfo               List handle info structure to initialize.
 */
int ShClTransferListHandleInfoInit(PSHCLLISTHANDLEINFO pInfo)
{
    AssertPtrReturn(pInfo, VERR_INVALID_POINTER);

    pInfo->hList   = NIL_SHCLLISTHANDLE;
    pInfo->enmType = SHCLOBJTYPE_INVALID;

    pInfo->pszPathLocalAbs = NULL;

    RT_ZERO(pInfo->u);

    return VINF_SUCCESS;
}

/**
 * Destroys a list handle info structure.
 *
 * @param   pInfo               List handle info structure to destroy.
 */
void ShClTransferListHandleInfoDestroy(PSHCLLISTHANDLEINFO pInfo)
{
    if (!pInfo)
        return;

    if (pInfo->pszPathLocalAbs)
    {
        RTStrFree(pInfo->pszPathLocalAbs);
        pInfo->pszPathLocalAbs = NULL;
    }
}

/**
 * Allocates a transfer list header structure.
 *
 * @returns VBox status code.
 * @param   ppListHdr           Where to store the allocated transfer list header structure on success.
 */
int ShClTransferListHdrAlloc(PSHCLLISTHDR *ppListHdr)
{
    int rc;

    PSHCLLISTHDR pListHdr = (PSHCLLISTHDR)RTMemAllocZ(sizeof(SHCLLISTHDR));
    if (pListHdr)
    {
        *ppListHdr = pListHdr;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Frees a transfer list header structure.
 *
 * @param   pListEntry          Transfer list header structure to free.
 *                              The pointer will be invalid on return.
 */
void ShClTransferListHdrFree(PSHCLLISTHDR pListHdr)
{
    if (!pListHdr)
        return;

    LogFlowFuncEnter();

    ShClTransferListHdrDestroy(pListHdr);

    RTMemFree(pListHdr);
    pListHdr = NULL;
}

/**
 * Duplicates (allocates) a transfer list header structure.
 *
 * @returns Duplicated transfer list header structure on success.
 * @param   pListHdr            Transfer list header to duplicate.
 */
PSHCLLISTHDR ShClTransferListHdrDup(PSHCLLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, NULL);

    PSHCLLISTHDR pListHdrDup = (PSHCLLISTHDR)RTMemAlloc(sizeof(SHCLLISTHDR));
    if (pListHdrDup)
        *pListHdrDup = *pListHdr;

    return pListHdrDup;
}

/**
 * Initializes a transfer list header structure.
 *
 * @returns VBox status code.
 * @param   pListHdr            Transfer list header struct to initialize.
 */
int ShClTransferListHdrInit(PSHCLLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    ShClTransferListHdrReset(pListHdr);

    return VINF_SUCCESS;
}

/**
 * Destroys a transfer list header structure.
 *
 * @param   pListHdr            Transfer list header struct to destroy.
 */
void ShClTransferListHdrDestroy(PSHCLLISTHDR pListHdr)
{
    if (!pListHdr)
        return;

    LogFlowFuncEnter();
}

/**
 * Resets a transfer list header structure.
 *
 * @returns VBox status code.
 * @param   pListHdr            Transfer list header struct to reset.
 */
void ShClTransferListHdrReset(PSHCLLISTHDR pListHdr)
{
    AssertPtrReturnVoid(pListHdr);

    LogFlowFuncEnter();

    RT_BZERO(pListHdr, sizeof(SHCLLISTHDR));
}

/**
 * Returns whether a given transfer list header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pListHdr            Transfer list header to validate.
 */
bool ShClTransferListHdrIsValid(PSHCLLISTHDR pListHdr)
{
    RT_NOREF(pListHdr);
    return true; /** @todo Implement this. */
}

/**
 * (Deep-)Copies a transfer list open parameters structure from one into another.
 *
 * @returns VBox status code.
 * @param   pDst                Destination parameters to copy to.
 * @param   pSrc                Source parameters to copy from.
 */
int ShClTransferListOpenParmsCopy(PSHCLLISTOPENPARMS pDst, PSHCLLISTOPENPARMS pSrc)
{
    AssertPtrReturn(pDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrc, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    if (pSrc->pszFilter)
    {
        pDst->pszFilter = RTStrDup(pSrc->pszFilter);
        if (!pDst->pszFilter)
            rc = VERR_NO_MEMORY;
    }

    if (   RT_SUCCESS(rc)
        && pSrc->pszPath)
    {
        pDst->pszPath = RTStrDup(pSrc->pszPath);
        if (!pDst->pszPath)
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        pDst->fList    = pDst->fList;
        pDst->cbFilter = pSrc->cbFilter;
        pDst->cbPath   = pSrc->cbPath;
    }

    return rc;
}

/**
 * Duplicates a transfer list open parameters structure.
 *
 * @returns Duplicated transfer list open parameters structure on success, or NULL on failure.
 * @param   pParms              Transfer list open parameters structure to duplicate.
 */
PSHCLLISTOPENPARMS ShClTransferListOpenParmsDup(PSHCLLISTOPENPARMS pParms)
{
    AssertPtrReturn(pParms, NULL);

    PSHCLLISTOPENPARMS pParmsDup = (PSHCLLISTOPENPARMS)RTMemAllocZ(sizeof(SHCLLISTOPENPARMS));
    if (!pParmsDup)
        return NULL;

    int rc = ShClTransferListOpenParmsCopy(pParmsDup, pParms);
    if (RT_FAILURE(rc))
    {
        ShClTransferListOpenParmsDestroy(pParmsDup);

        RTMemFree(pParmsDup);
        pParmsDup = NULL;
    }

    return pParmsDup;
}

/**
 * Initializes a transfer list open parameters structure.
 *
 * @returns VBox status code.
 * @param   pParms              Transfer list open parameters structure to initialize.
 */
int ShClTransferListOpenParmsInit(PSHCLLISTOPENPARMS pParms)
{
    AssertPtrReturn(pParms, VERR_INVALID_POINTER);

    RT_BZERO(pParms, sizeof(SHCLLISTOPENPARMS));

    pParms->cbFilter  = SHCL_TRANSFER_PATH_MAX; /** @todo Make this dynamic. */
    pParms->pszFilter = RTStrAlloc(pParms->cbFilter);

    pParms->cbPath    = SHCL_TRANSFER_PATH_MAX; /** @todo Make this dynamic. */
    pParms->pszPath   = RTStrAlloc(pParms->cbPath);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

/**
 * Destroys a transfer list open parameters structure.
 *
 * @param   pParms              Transfer list open parameters structure to destroy.
 */
void ShClTransferListOpenParmsDestroy(PSHCLLISTOPENPARMS pParms)
{
    if (!pParms)
        return;

    if (pParms->pszFilter)
    {
        RTStrFree(pParms->pszFilter);
        pParms->pszFilter = NULL;
    }

    if (pParms->pszPath)
    {
        RTStrFree(pParms->pszPath);
        pParms->pszPath = NULL;
    }
}

/**
 * Creates (allocates) and initializes a clipboard list entry structure.
 *
 * @returns VBox status code.
 * @param   ppListEntry         Where to return the created clipboard list entry structure on success.
 *                              Must be free'd with  ShClTransferListEntryFree().
 */
int ShClTransferListEntryAlloc(PSHCLLISTENTRY *ppListEntry)
{
    PSHCLLISTENTRY pListEntry = (PSHCLLISTENTRY)RTMemAlloc(sizeof(SHCLLISTENTRY));
    if (!pListEntry)
        return VERR_NO_MEMORY;

    int rc;

    size_t cbInfo = sizeof(SHCLFSOBJINFO);
    void  *pvInfo = RTMemAlloc(cbInfo);
    if (pvInfo)
    {
        rc = ShClTransferListEntryInitEx(pListEntry, VBOX_SHCL_INFO_F_NONE, NULL /* pszName */, pvInfo, (uint32_t)cbInfo);
        if (RT_SUCCESS(rc))
            *ppListEntry = pListEntry;

        return rc;
    }
    else
        rc = VERR_NO_MEMORY;

    RTMemFree(pListEntry);
    return rc;
}

/**
 * Frees a clipboard list entry structure.
 *
 * @param   pEntry              Clipboard list entry structure to free.
 *                              The pointer will be invalid on return.
 */
void ShClTransferListEntryFree(PSHCLLISTENTRY pEntry)
{
    if (!pEntry)
        return;

    /* Make sure to destroy the entry properly, in case the caller forgot this. */
    ShClTransferListEntryDestroy(pEntry);

    RTMemFree(pEntry);
    pEntry = NULL;
}

/**
 * (Deep-)Copies a clipboard list entry structure.
 *
 * @returns VBox status code.
 * @param   pDst                Destination list entry to copy to.
 * @param   pSrc                Source list entry to copy from.
 */
int ShClTransferListEntryCopy(PSHCLLISTENTRY pDst, PSHCLLISTENTRY pSrc)
{
    AssertPtrReturn(pDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrc, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    *pDst = *pSrc;

    if (pSrc->pszName)
    {
        pDst->pszName = RTStrDup(pSrc->pszName);
        if (!pDst->pszName)
            rc = VERR_NO_MEMORY;
    }

    if (   RT_SUCCESS(rc)
        && pSrc->pvInfo)
    {
        pDst->pvInfo = RTMemDup(pSrc->pvInfo, pSrc->cbInfo);
        if (pDst->pvInfo)
        {
            pDst->cbInfo = pSrc->cbInfo;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
    {
        if (pDst->pvInfo)
        {
            RTMemFree(pDst->pvInfo);
            pDst->pvInfo = NULL;
            pDst->cbInfo = 0;
        }
    }

    return rc;
}

/**
 * Duplicates (allocates) a clipboard list entry structure.
 *
 * @returns Duplicated clipboard list entry structure on success.
 * @param   pEntry              Clipboard list entry to duplicate.
 */
PSHCLLISTENTRY ShClTransferListEntryDup(PSHCLLISTENTRY pEntry)
{
    AssertPtrReturn(pEntry, NULL);

    int rc = VINF_SUCCESS;

    PSHCLLISTENTRY pListEntryDup = (PSHCLLISTENTRY)RTMemAllocZ(sizeof(SHCLLISTENTRY));
    if (pListEntryDup)
        rc = ShClTransferListEntryCopy(pListEntryDup, pEntry);

    if (RT_FAILURE(rc))
    {
        ShClTransferListEntryDestroy(pListEntryDup);

        RTMemFree(pListEntryDup);
        pListEntryDup = NULL;
    }

    return pListEntryDup;
}

/**
 * Returns whether a given list entry name is valid or not.
 *
 * @returns \c true if valid, or \c false if not.
 * @param   pszName             Name to check.
 * @param   cbName              Size (in bytes) of \a pszName to check.
 *                              Includes terminator.
 */
static bool shclTransferListEntryNameIsValid(const char *pszName, size_t cbName)
{
    if (!pszName)
        return false;

    size_t const cchLen = strlen(pszName);

    if (  !cbName
        || cchLen == 0
        || cchLen > cbName                 /* Includes zero termination */ - 1
        || cchLen > SHCLLISTENTRY_MAX_NAME /* Ditto */ - 1)
    {
        return false;
    }

    int rc = ShClTransferValidatePath(pszName, false /* fMustExist */);
    if (RT_FAILURE(rc))
        return false;

    return true;
}

/**
 * Initializes a clipboard list entry structure, extended version.
 *
 * @returns VBox status code.
 * @param   pListEntry          Clipboard list entry structure to initialize.
 * @param   fInfo               Info flags (of type VBOX_SHCL_INFO_F_XXX).
 * @param   pszName             Name (e.g. filename) to use. Can be NULL if not being used.
 *                              Up to SHCLLISTENTRY_MAX_NAME characters.
 * @param   pvInfo              Pointer to info data to assign. Must match \a fInfo.
 *                              The list entry takes the ownership of the data on success.
 * @param   cbInfo              Size (in bytes) of \a pvInfo data to assign.
 */
int ShClTransferListEntryInitEx(PSHCLLISTENTRY pListEntry, uint32_t fInfo, const char *pszName, void *pvInfo, uint32_t cbInfo)
{
    AssertPtrReturn(pListEntry, VERR_INVALID_POINTER);
    AssertReturn   (   pszName == NULL
                    || shclTransferListEntryNameIsValid(pszName, strlen(pszName) + 1), VERR_INVALID_PARAMETER);
    /* pvInfo + cbInfo depend on fInfo. See below. */

    RT_BZERO(pListEntry, sizeof(SHCLLISTENTRY));

    if (pszName)
    {
        pListEntry->pszName = RTStrDupN(pszName, SHCLLISTENTRY_MAX_NAME);
        AssertPtrReturn(pListEntry->pszName, VERR_NO_MEMORY);
        pListEntry->cbName = (uint32_t)strlen(pListEntry->pszName) + 1 /* Include terminator */;
    }

    pListEntry->pvInfo = pvInfo;
    pListEntry->cbInfo = cbInfo;
    pListEntry->fInfo  = fInfo;

    return VINF_SUCCESS;
}

/**
 * Initializes a clipboard list entry structure (as empty / invalid).
 *
 * @returns VBox status code.
 * @param   pListEntry          Clipboard list entry structure to initialize.
 */
int ShClTransferListEntryInit(PSHCLLISTENTRY pListEntry)
{
    return ShClTransferListEntryInitEx(pListEntry, VBOX_SHCL_INFO_F_NONE, NULL /* pszName */, NULL /* pvInfo */, 0 /* cbInfo */);
}

/**
 * Destroys a clipboard list entry structure.
 *
 * @param   pListEntry          Clipboard list entry structure to destroy.
 */
void ShClTransferListEntryDestroy(PSHCLLISTENTRY pListEntry)
{
    if (!pListEntry)
        return;

    if (pListEntry->pszName)
    {
        RTStrFree(pListEntry->pszName);

        pListEntry->pszName = NULL;
        pListEntry->cbName  = 0;
    }

    if (pListEntry->pvInfo)
    {
        RTMemFree(pListEntry->pvInfo);
        pListEntry->pvInfo = NULL;
        pListEntry->cbInfo = 0;
    }
}

/**
 * Returns whether a given clipboard list entry is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pListEntry          Clipboard list entry to validate.
 */
bool ShClTransferListEntryIsValid(PSHCLLISTENTRY pListEntry)
{
    AssertPtrReturn(pListEntry, false);

    if (!shclTransferListEntryNameIsValid(pListEntry->pszName, pListEntry->cbName))
        return false;

    if (pListEntry->cbInfo) /* cbInfo / pvInfo is optional. */
    {
        if (!pListEntry->pvInfo)
            return false;
    }

    return true;
}


/*********************************************************************************************************************************
 * Transfer Object                                                                                                               *
 ********************************************************************************************************************************/

/**
 * Initializes a transfer object context.
 *
 * @returns VBox status code.
 * @param   pObjCtx             Transfer object context to initialize.
 */
int ShClTransferObjCtxInit(PSHCLCLIENTTRANSFEROBJCTX pObjCtx)
{
    AssertPtrReturn(pObjCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    pObjCtx->uHandle  = NIL_SHCLOBJHANDLE;

    return VINF_SUCCESS;
}

/**
 * Destroys a transfer object context.
 *
 * @param   pObjCtx             Transfer object context to destroy.
 */
void ShClTransferObjCtxDestroy(PSHCLCLIENTTRANSFEROBJCTX pObjCtx)
{
    AssertPtrReturnVoid(pObjCtx);

    LogFlowFuncEnter();
}

/**
 * Returns if a transfer object context is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pObjCtx             Transfer object context to check.
 */
bool ShClTransferObjCtxIsValid(PSHCLCLIENTTRANSFEROBJCTX pObjCtx)
{
    return (   pObjCtx
            && pObjCtx->uHandle != NIL_SHCLOBJHANDLE);
}

/**
 * Initializes an object handle info structure.
 *
 * @returns VBox status code.
 * @param   pInfo               Object handle info structure to initialize.
 */
int ShClTransferObjHandleInfoInit(PSHCLOBJHANDLEINFO pInfo)
{
    AssertPtrReturn(pInfo, VERR_INVALID_POINTER);

    pInfo->hObj    = NIL_SHCLOBJHANDLE;
    pInfo->enmType = SHCLOBJTYPE_INVALID;

    pInfo->pszPathLocalAbs = NULL;

    RT_ZERO(pInfo->u);

    return VINF_SUCCESS;
}

/**
 * Destroys an object handle info structure.
 *
 * @param   pInfo               Object handle info structure to destroy.
 */
void ShClTransferObjHandleInfoDestroy(PSHCLOBJHANDLEINFO pInfo)
{
    if (!pInfo)
        return;

    if (pInfo->pszPathLocalAbs)
    {
        RTStrFree(pInfo->pszPathLocalAbs);
        pInfo->pszPathLocalAbs = NULL;
    }
}

/**
 * Initializes a transfer object open parameters structure.
 *
 * @returns VBox status code.
 * @param   pParms              Transfer object open parameters structure to initialize.
 */
int ShClTransferObjOpenParmsInit(PSHCLOBJOPENCREATEPARMS pParms)
{
    AssertPtrReturn(pParms, VERR_INVALID_POINTER);

    int rc;

    RT_BZERO(pParms, sizeof(SHCLOBJOPENCREATEPARMS));

    pParms->cbPath    = RTPATH_MAX; /** @todo Make this dynamic. */
    pParms->pszPath   = RTStrAlloc(pParms->cbPath);
    if (pParms->pszPath)
    {
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Copies a transfer object open parameters structure from source to destination.
 *
 * @returns VBox status code.
 * @param   pParmsDst           Where to copy the source transfer object open parameters to.
 * @param   pParmsSrc           Which source transfer object open parameters to copy.
 */
int ShClTransferObjOpenParmsCopy(PSHCLOBJOPENCREATEPARMS pParmsDst, PSHCLOBJOPENCREATEPARMS pParmsSrc)
{
    int rc;

    *pParmsDst = *pParmsSrc;

    if (pParmsSrc->pszPath)
    {
        Assert(pParmsSrc->cbPath);
        pParmsDst->pszPath = RTStrDup(pParmsSrc->pszPath);
        if (pParmsDst->pszPath)
        {
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys a transfer object open parameters structure.
 *
 * @param   pParms              Transfer object open parameters structure to destroy.
 */
void ShClTransferObjOpenParmsDestroy(PSHCLOBJOPENCREATEPARMS pParms)
{
    if (!pParms)
        return;

    if (pParms->pszPath)
    {
        RTStrFree(pParms->pszPath);
        pParms->pszPath = NULL;
    }
}

/**
 * Returns a specific object handle info of a transfer.
 *
 * @returns Pointer to object handle info if found, or NULL if not found.
 * @param   pTransfer           Clipboard transfer to get object handle info from.
 * @param   hObj                Object handle of the object to get handle info for.
 */
PSHCLOBJHANDLEINFO ShClTransferObjGet(PSHCLTRANSFER pTransfer, SHCLOBJHANDLE hObj)
{
    PSHCLOBJHANDLEINFO pIt;
    RTListForEach(&pTransfer->lstObj, pIt, SHCLOBJHANDLEINFO, Node) /** @todo Slooow ...but works for now. */
    {
        if (pIt->hObj == hObj)
            return pIt;
    }

    return NULL;
}

/**
 * Opens a transfer object.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to open the object for.
 * @param   pOpenCreateParms    Open / create parameters of transfer object to open / create.
 * @param   phObj               Where to store the handle of transfer object opened on success.
 */
int ShClTransferObjOpen(PSHCLTRANSFER pTransfer, PSHCLOBJOPENCREATEPARMS pOpenCreateParms, PSHCLOBJHANDLE phObj)
{
    AssertPtrReturn(pTransfer,        VERR_INVALID_POINTER);
    AssertPtrReturn(pOpenCreateParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phObj,            VERR_INVALID_POINTER);
    AssertMsgReturn(pTransfer->pszPathRootAbs, ("Transfer has no root path set\n"), VERR_INVALID_PARAMETER);
    /** @todo Check pOpenCreateParms->fCreate flags. */
    AssertMsgReturn(pOpenCreateParms->pszPath, ("No path in open/create params set\n"), VERR_INVALID_PARAMETER);

    if (pTransfer->cObjHandles >= pTransfer->cMaxObjHandles)
        return VERR_SHCLPB_MAX_OBJECTS_REACHED;

    LogFlowFunc(("pszPath=%s, fCreate=0x%x\n", pOpenCreateParms->pszPath, pOpenCreateParms->fCreate));

    int rc;
    if (pTransfer->ProviderIface.pfnObjOpen)
        rc = pTransfer->ProviderIface.pfnObjOpen(&pTransfer->ProviderCtx, pOpenCreateParms, phObj);
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Closes a transfer object.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer that contains the object to close.
 * @param   hObj                Handle of transfer object to close.
 */
int ShClTransferObjClose(PSHCLTRANSFER pTransfer, SHCLOBJHANDLE hObj)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    int rc;
    if (pTransfer->ProviderIface.pfnObjClose)
        rc = pTransfer->ProviderIface.pfnObjClose(&pTransfer->ProviderCtx, hObj);
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads from a transfer object.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer that contains the object to read from.
 * @param   hObj                Handle of transfer object to read from.
 * @param   pvBuf               Buffer for where to store the read data.
 * @param   cbBuf               Size (in bytes) of buffer.
 * @param   fFlags              Read flags. Optional.
 * @param   pcbRead             Where to return how much bytes were read on success. Optional.
 */
int ShClTransferObjRead(PSHCLTRANSFER pTransfer,
                        SHCLOBJHANDLE hObj, void *pvBuf, uint32_t cbBuf, uint32_t fFlags, uint32_t *pcbRead)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,     VERR_INVALID_POINTER);
    AssertReturn   (cbBuf,     VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */
    /** @todo Validate fFlags. */

    int rc;
    if (pTransfer->ProviderIface.pfnObjRead)
        rc = pTransfer->ProviderIface.pfnObjRead(&pTransfer->ProviderCtx, hObj, pvBuf, cbBuf, fFlags, pcbRead);
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes to a transfer object.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer that contains the object to write to.
 * @param   hObj                Handle of transfer object to write to.
 * @param   pvBuf               Buffer of data to write.
 * @param   cbBuf               Size (in bytes) of buffer to write.
 * @param   fFlags              Write flags. Optional.
 * @param   pcbWritten          How much bytes were writtenon success. Optional.
 */
int ShClTransferObjWrite(PSHCLTRANSFER pTransfer,
                         SHCLOBJHANDLE hObj, void *pvBuf, uint32_t cbBuf, uint32_t fFlags, uint32_t *pcbWritten)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,     VERR_INVALID_POINTER);
    AssertReturn   (cbBuf,     VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    int rc;
    if (pTransfer->ProviderIface.pfnObjWrite)
        rc = pTransfer->ProviderIface.pfnObjWrite(&pTransfer->ProviderCtx, hObj, pvBuf, cbBuf, fFlags, pcbWritten);
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Duplicates a transfer object data chunk.
 *
 * @returns Duplicated object data chunk on success, or NULL on failure.
 * @param   pDataChunk          Transfer object data chunk to duplicate.
 */
PSHCLOBJDATACHUNK ShClTransferObjDataChunkDup(PSHCLOBJDATACHUNK pDataChunk)
{
    AssertPtrReturn(pDataChunk, NULL);

    PSHCLOBJDATACHUNK pDataChunkDup = (PSHCLOBJDATACHUNK)RTMemAllocZ(sizeof(SHCLOBJDATACHUNK));
    if (!pDataChunkDup)
        return NULL;

    if (pDataChunk->pvData)
    {
        Assert(pDataChunk->cbData);

        pDataChunkDup->uHandle = pDataChunk->uHandle;
        pDataChunkDup->pvData  = RTMemDup(pDataChunk->pvData, pDataChunk->cbData);
        AssertPtrReturn(pDataChunkDup->pvData, NULL);
        pDataChunkDup->cbData  = pDataChunk->cbData;
    }

    return pDataChunkDup;
}

/**
 * Destroys a transfer object data chunk.
 *
 * @param   pDataChunk          Transfer object data chunk to destroy.
 */
void ShClTransferObjDataChunkDestroy(PSHCLOBJDATACHUNK pDataChunk)
{
    if (!pDataChunk)
        return;

    if (pDataChunk->pvData)
    {
        Assert(pDataChunk->cbData);

        RTMemFree(pDataChunk->pvData);

        pDataChunk->pvData = NULL;
        pDataChunk->cbData = 0;
    }

    pDataChunk->uHandle = 0;
}

/**
 * Frees a transfer object data chunk.
 *
 * @param   pDataChunk          Transfer object data chunk to free.
 *                              The pointer will be invalid on return.
 */
void ShClTransferObjDataChunkFree(PSHCLOBJDATACHUNK pDataChunk)
{
    if (!pDataChunk)
        return;

    ShClTransferObjDataChunkDestroy(pDataChunk);

    RTMemFree(pDataChunk);
    pDataChunk = NULL;
}


/*********************************************************************************************************************************
 * Transfer                                                                                                                      *
 ********************************************************************************************************************************/

/**
 * Creates a clipboard transfer, extended version.
 *
 * @returns VBox status code.
 * @param   cbMaxChunkSize      Maximum transfer chunk size (in bytes) to use.
 * @param   cMaxListHandles     Maximum list entries the transfer can have.
 * @param   cMaxObjHandles      Maximum transfer objects the transfer can have.
 * @param   ppTransfer          Where to return the created clipboard transfer struct.
 *                              Must be destroyed by ShClTransferDestroy().
 */
int ShClTransferCreateEx(uint32_t cbMaxChunkSize, uint32_t cMaxListHandles, uint32_t cMaxObjHandles,
                         PSHCLTRANSFER *ppTransfer)
{


    AssertPtrReturn(ppTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PSHCLTRANSFER pTransfer = (PSHCLTRANSFER)RTMemAllocZ(sizeof(SHCLTRANSFER));
    AssertPtrReturn(pTransfer, VERR_NO_MEMORY);

    pTransfer->State.uID       = 0;
    pTransfer->State.enmStatus = SHCLTRANSFERSTATUS_NONE;
    pTransfer->State.enmDir    = SHCLTRANSFERDIR_UNKNOWN;
    pTransfer->State.enmSource = SHCLSOURCE_INVALID;

    pTransfer->Thread.hThread    = NIL_RTTHREAD;
    pTransfer->Thread.fCancelled = false;
    pTransfer->Thread.fStarted   = false;
    pTransfer->Thread.fStop      = false;

    pTransfer->pszPathRootAbs    = NULL;

    pTransfer->uTimeoutMs      = SHCL_TIMEOUT_DEFAULT_MS;
    pTransfer->cbMaxChunkSize  = cbMaxChunkSize;
    pTransfer->cMaxListHandles = cMaxListHandles;
    pTransfer->cMaxObjHandles  = cMaxObjHandles;

    pTransfer->pvUser = NULL;
    pTransfer->cbUser = 0;

    RTListInit(&pTransfer->lstHandles);
    RTListInit(&pTransfer->lstObj);

    /* The provider context + interface is NULL by default. */
    RT_ZERO(pTransfer->ProviderCtx);
    RT_ZERO(pTransfer->ProviderIface);

    ShClTransferListInit(&pTransfer->lstRoots);

    int rc = ShClEventSourceCreate(&pTransfer->Events, 0 /* uID */);
    if (RT_SUCCESS(rc))
    {
        *ppTransfer = pTransfer;
    }
    else
    {
        if (pTransfer)
        {
            ShClTransferDestroy(pTransfer);
            RTMemFree(pTransfer);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates a clipboard transfer with default settings.
 *
 * @returns VBox status code.
 * @param   ppTransfer          Where to return the created clipboard transfer struct.
 *                              Must be destroyed by ShClTransferDestroy().
 */
int ShClTransferCreate(PSHCLTRANSFER *ppTransfer)
{
    return ShClTransferCreateEx(SHCL_TRANSFER_DEFAULT_MAX_CHUNK_SIZE,
                                SHCL_TRANSFER_DEFAULT_MAX_LIST_HANDLES,
                                SHCL_TRANSFER_DEFAULT_MAX_OBJ_HANDLES,
                                ppTransfer);
}

/**
 * Destroys a clipboard transfer.
 *
 * @returns VBox status code.
 * @param   pTransferCtx                Clipboard transfer to destroy.
 */
int ShClTransferDestroy(PSHCLTRANSFER pTransfer)
{
    if (!pTransfer)
        return VINF_SUCCESS;

    /* Must come before the refcount check below, as the callback might release a reference. */
    if (pTransfer->Callbacks.pfnOnDestroy)
        pTransfer->Callbacks.pfnOnDestroy(&pTransfer->CallbackCtx);

    AssertMsgReturn(pTransfer->cRefs == 0, ("Number of references > 0 (%RU32)\n", pTransfer->cRefs), VERR_WRONG_ORDER);

    LogFlowFuncEnter();

    int rc = shClTransferThreadDestroy(pTransfer, RT_MS_30SEC /* Timeout in ms */);
    if (RT_FAILURE(rc))
        return rc;

    ShClTransferReset(pTransfer);

    if (RTCritSectIsInitialized(&pTransfer->CritSect))
        RTCritSectDelete(&pTransfer->CritSect);

    ShClEventSourceDestroy(&pTransfer->Events);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

/**
 * Initializes a clipboard transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to initialize.
 * @param   enmDir              Specifies the transfer direction of this transfer.
 * @param   enmSource           Specifies the data source of the transfer.
 */
int ShClTransferInit(PSHCLTRANSFER pTransfer, SHCLTRANSFERDIR enmDir, SHCLSOURCE enmSource)
{
    AssertMsgReturn(pTransfer->State.enmStatus < SHCLTRANSFERSTATUS_INITIALIZED,
                    ("Wrong status (currently is %s)\n", ShClTransferStatusToStr(pTransfer->State.enmStatus)),
                    VERR_WRONG_ORDER);

    pTransfer->cRefs = 0;

    pTransfer->State.enmDir    = enmDir;
    pTransfer->State.enmSource = enmSource;

    LogFlowFunc(("uID=%RU32, enmDir=%RU32, enmSource=%RU32\n",
                 pTransfer->State.uID, pTransfer->State.enmDir, pTransfer->State.enmSource));

    pTransfer->cListHandles    = 0;
    pTransfer->uListHandleNext = 1;

    pTransfer->cObjHandles     = 0;
    pTransfer->uObjHandleNext  = 1;

    /* Make sure that the callback context has all values set according to the callback table.
     * This only needs to be done once, so do this here. */
    pTransfer->CallbackCtx.pTransfer = pTransfer;
    pTransfer->CallbackCtx.pvUser    = pTransfer->Callbacks.pvUser;
    pTransfer->CallbackCtx.cbUser    = pTransfer->Callbacks.cbUser;

    int rc = RTCritSectInit(&pTransfer->CritSect);
    AssertRCReturn(rc, rc);

    if (pTransfer->Callbacks.pfnOnInitialized)
        pTransfer->Callbacks.pfnOnInitialized(&pTransfer->CallbackCtx);

    if (RT_SUCCESS(rc))
    {
        pTransfer->State.enmStatus = SHCLTRANSFERSTATUS_INITIALIZED; /* Now we're ready to run. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Locks a transfer.
 *
 * @param   pTransfer           Transfer to lock.
 */
DECLINLINE(void) shClTransferLock(PSHCLTRANSFER pTransfer)
{
    int rc2 = RTCritSectEnter(&pTransfer->CritSect);
    AssertRC(rc2);
}

/**
 * Unlocks a transfer.
 *
 * @param   pTransfer           Transfer to unlock.
 */
DECLINLINE(void) shClTransferUnlock(PSHCLTRANSFER pTransfer)
{
    int rc2 = RTCritSectLeave(&pTransfer->CritSect);
    AssertRC(rc2);
}

/**
 * Acquires a reference to this transfer.
 *
 * @returns New reference count.
 * @param   pTransfer           Transfer to acquire reference for.
 */
uint32_t ShClTransferAcquire(PSHCLTRANSFER pTransfer)
{
    return ASMAtomicIncU32(&pTransfer->cRefs);
}

/**
 * Releases a reference to this transfer.
 *
 * @returns New reference count.
 * @param   pTransfer           Transfer to release reference for.
 */
uint32_t ShClTransferRelease(PSHCLTRANSFER pTransfer)
{
    return ASMAtomicDecU32(&pTransfer->cRefs);
}

/**
 * Opens a transfer list.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to handle.
 * @param   pOpenParms          List open parameters to use for opening.
 * @param   phList              Where to store the List handle of opened list on success.
 */
int ShClTransferListOpen(PSHCLTRANSFER pTransfer, PSHCLLISTOPENPARMS pOpenParms,
                         PSHCLLISTHANDLE phList)
{
    AssertPtrReturn(pTransfer,  VERR_INVALID_POINTER);
    AssertPtrReturn(pOpenParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phList,     VERR_INVALID_POINTER);

    if (pTransfer->cListHandles == pTransfer->cMaxListHandles)
        return VERR_SHCLPB_MAX_LISTS_REACHED;

     int rc;
     if (pTransfer->ProviderIface.pfnListOpen)
        rc = pTransfer->ProviderIface.pfnListOpen(&pTransfer->ProviderCtx, pOpenParms, phList);
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Closes a transfer list.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to handle.
 * @param   hList               Handle of list to close.
 */
int ShClTransferListClose(PSHCLTRANSFER pTransfer, SHCLLISTHANDLE hList)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    if (hList == NIL_SHCLLISTHANDLE)
        return VINF_SUCCESS;

    int rc;
    if (pTransfer->ProviderIface.pfnListClose)
        rc = pTransfer->ProviderIface.pfnListClose(&pTransfer->ProviderCtx, hList);
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Retrieves the header of a transfer list.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to handle.
 * @param   hList               Handle of list to get header for.
 * @param   pHdr                Where to store the returned list header information.
 */
int ShClTransferListGetHeader(PSHCLTRANSFER pTransfer, SHCLLISTHANDLE hList,
                              PSHCLLISTHDR pHdr)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pHdr,      VERR_INVALID_POINTER);

    LogFlowFunc(("hList=%RU64\n", hList));

    int rc;
    if (pTransfer->ProviderIface.pfnListHdrRead)
        rc = pTransfer->ProviderIface.pfnListHdrRead(&pTransfer->ProviderCtx, hList, pHdr);
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns a specific list handle info of a clipboard transfer.
 *
 * @returns Pointer to list handle info if found, or NULL if not found.
 * @param   pTransfer           Clipboard transfer to get list handle info from.
 * @param   hList               List handle of the list to get handle info for.
 */
PSHCLLISTHANDLEINFO ShClTransferListGetByHandle(PSHCLTRANSFER pTransfer, SHCLLISTHANDLE hList)
{
    PSHCLLISTHANDLEINFO pIt;
    RTListForEach(&pTransfer->lstHandles, pIt, SHCLLISTHANDLEINFO, Node) /** @todo Sloooow ... improve this. */
    {
        if (pIt->hList == hList)
            return pIt;
    }

    return NULL;
}

/**
 * Returns the current transfer object of a transfer list.
 *
 * Currently not implemented and wil return NULL.
 *
 * @returns Pointer to transfer object, or NULL if not found / invalid.
 * @param   pTransfer           Clipboard transfer to return transfer object for.
 * @param   hList               Handle of clipboard transfer list to get object for.
 * @param   uIdx                Index of object to get.
 */
PSHCLTRANSFEROBJ ShClTransferListGetObj(PSHCLTRANSFER pTransfer,
                                        SHCLLISTHANDLE hList, uint64_t uIdx)
{
    AssertPtrReturn(pTransfer, NULL);

    RT_NOREF(hList, uIdx);

    LogFlowFunc(("hList=%RU64\n", hList));

    return NULL;
}

/**
 * Reads a single transfer list entry.
 *
 * @returns VBox status code or VERR_NO_MORE_FILES if the end of the list has been reached.
 * @param   pTransfer           Clipboard transfer to handle.
 * @param   hList               List handle of list to read from.
 * @param   pEntry              Where to store the read information.
 */
int ShClTransferListRead(PSHCLTRANSFER pTransfer, SHCLLISTHANDLE hList,
                         PSHCLLISTENTRY pEntry)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pEntry,    VERR_INVALID_POINTER);

    LogFlowFunc(("hList=%RU64\n", hList));

    int rc;
    if (pTransfer->ProviderIface.pfnListEntryRead)
        rc = pTransfer->ProviderIface.pfnListEntryRead(&pTransfer->ProviderCtx, hList, pEntry);
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes a single transfer list entry.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to handle.
 * @param   hList               List handle of list to write to.
 * @param   pEntry              Entry information to write.
 */
int ShClTransferListWrite(PSHCLTRANSFER pTransfer, SHCLLISTHANDLE hList,
                          PSHCLLISTENTRY pEntry)
{
    RT_NOREF(pTransfer, hList, pEntry);

    int rc = VINF_SUCCESS;

#if 0
    if (pTransfer->ProviderIface.pfnListEntryWrite)
        rc = pTransfer->ProviderIface.pfnListEntryWrite(&pTransfer->ProviderCtx, hList, pEntry);
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns whether a given transfer list handle is valid or not.
 *
 * @returns \c true if list handle is valid, \c false if not.
 * @param   pTransfer           Clipboard transfer to handle.
 * @param   hList               List handle to check.
 */
bool ShClTransferListHandleIsValid(PSHCLTRANSFER pTransfer, SHCLLISTHANDLE hList)
{
    bool fIsValid = false;

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        fIsValid = ShClTransferListGetByHandle(pTransfer, hList) != NULL;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
    {
        AssertFailed(); /** @todo Implement. */
    }
    else
        AssertFailedStmt(fIsValid = false);

    return fIsValid;
}

/**
 * Copies a transfer callback table from source to destination.
 *
 * @param   pCallbacksDst       Callback destination.
 * @param   pCallbacksSrc       Callback source. If set to NULL, the
 *                              destination callback table will be unset.
 */
void ShClTransferCopyCallbacks(PSHCLTRANSFERCALLBACKS pCallbacksDst,
                               PSHCLTRANSFERCALLBACKS pCallbacksSrc)
{
    AssertPtrReturnVoid(pCallbacksDst);

    if (pCallbacksSrc) /* Set */
    {
#define SET_CALLBACK(a_pfnCallback) \
        if (pCallbacksSrc->a_pfnCallback) \
            pCallbacksDst->a_pfnCallback = pCallbacksSrc->a_pfnCallback

        SET_CALLBACK(pfnOnInitialized);
        SET_CALLBACK(pfnOnDestroy);
        SET_CALLBACK(pfnOnStarted);
        SET_CALLBACK(pfnOnCompleted);
        SET_CALLBACK(pfnOnError);
        SET_CALLBACK(pfnOnRegistered);
        SET_CALLBACK(pfnOnUnregistered);

#undef SET_CALLBACK

        pCallbacksDst->pvUser = pCallbacksSrc->pvUser;
        pCallbacksDst->cbUser = pCallbacksSrc->cbUser;
    }
    else /* Unset */
        RT_BZERO(pCallbacksDst, sizeof(SHCLTRANSFERCALLBACKS));
}

/**
 * Sets or unsets the callback table to be used for a clipboard transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to set callbacks for.
 * @param   pCallbacks          Pointer to callback table to set. If set to NULL,
 *                              existing callbacks for this transfer will be unset.
 *
 * @note    Must come before initializing the transfer via ShClTransferInit().
 */
void ShClTransferSetCallbacks(PSHCLTRANSFER pTransfer,
                              PSHCLTRANSFERCALLBACKS pCallbacks)
{
    AssertPtrReturnVoid(pTransfer);
    /* pCallbacks can be NULL. */

    ShClTransferCopyCallbacks(&pTransfer->Callbacks, pCallbacks);
}

/**
 * Sets the transfer provider for a given transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to create transfer provider for.
 * @param   pProvider           Provider to use.
 */
int ShClTransferSetProvider(PSHCLTRANSFER pTransfer, PSHCLTXPROVIDER pProvider)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pProvider, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    pTransfer->ProviderIface         = pProvider->Interface;
    pTransfer->ProviderCtx.pTransfer = pTransfer;
    pTransfer->ProviderCtx.pvUser    = pProvider->pvUser;
    pTransfer->ProviderCtx.cbUser    = pProvider->cbUser;

    LogRelFunc(("pfnOnInitialized=%p\n", pTransfer->Callbacks.pfnOnInitialized));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns the number of transfer root list entries.
 *
 * @returns Root list entry count.
 * @param   pTransfer           Clipboard transfer to return root entry count for.
 */
uint64_t ShClTransferRootsCount(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, 0);

    shClTransferLock(pTransfer);

    uint32_t const cRoots = pTransfer->lstRoots.Hdr.cEntries;

    shClTransferUnlock(pTransfer);

    return cRoots;
}

/**
 * Resets the root list of a clipboard transfer.
 *
 * @param   pTransfer           Transfer to clear transfer root list for.
 *
 * @note    Caller needs to take critical section.
 */
static void shClTransferRootsReset(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturnVoid(pTransfer);
    Assert(RTCritSectIsOwner(&pTransfer->CritSect));

    if (pTransfer->pszPathRootAbs)
    {
        RTStrFree(pTransfer->pszPathRootAbs);
        pTransfer->pszPathRootAbs = NULL;
    }

    ShClTransferListDestroy(&pTransfer->lstRoots);
}

/**
 * Resets a clipboard transfer.
 *
 * @param   pTransfer           Clipboard transfer to reset.
 */
void ShClTransferReset(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturnVoid(pTransfer);

    LogFlowFuncEnter();

    shClTransferLock(pTransfer);

    shClTransferRootsReset(pTransfer);

    PSHCLLISTHANDLEINFO pItList, pItListNext;
    RTListForEachSafe(&pTransfer->lstHandles, pItList, pItListNext, SHCLLISTHANDLEINFO, Node)
    {
        ShClTransferListHandleInfoDestroy(pItList);

        RTListNodeRemove(&pItList->Node);

        RTMemFree(pItList);
    }

    PSHCLOBJHANDLEINFO pItObj, pItObjNext;
    RTListForEachSafe(&pTransfer->lstObj, pItObj, pItObjNext, SHCLOBJHANDLEINFO, Node)
    {
        ShClTransferObjHandleInfoDestroy(pItObj);

        RTListNodeRemove(&pItObj->Node);

        RTMemFree(pItObj);
    }

    shClTransferUnlock(pTransfer);
}

/**
 * Get a specific root list entry.
 *
 * @returns Const pointer to root list entry if found, or NULL if not found..
 * @param   pTransfer           Clipboard transfer to get root list entry of.
 * @param   uIndex              Index (zero-based) of entry to get.
 */
PCSHCLLISTENTRY ShClTransferRootsEntryGet(PSHCLTRANSFER pTransfer, uint64_t uIndex)
{
    AssertPtrReturn(pTransfer, NULL);

    shClTransferLock(pTransfer);

    if (uIndex >= pTransfer->lstRoots.Hdr.cEntries)
    {
        shClTransferUnlock(pTransfer);
        return NULL;
    }

    PCSHCLLISTENTRY pEntry = shClTransferListGetEntryById(&pTransfer->lstRoots, uIndex);

    shClTransferUnlock(pTransfer);

    return pEntry;
}

/**
 * Reads the root entries of a clipboard transfer.
 *
 * This gives the provider interface the chance of reading root entries information.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to read root list for.
 */
int ShClTransferRootListRead(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc;
    if (pTransfer->ProviderIface.pfnRootListRead)
        rc = pTransfer->ProviderIface.pfnRootListRead(&pTransfer->ProviderCtx);
    else
        rc = VERR_NOT_SUPPORTED;

    shClTransferLock(pTransfer);

    /* Make sure that we have at least an empty root path set. */
    if (   RT_SUCCESS(rc)
        && !pTransfer->pszPathRootAbs)
    {
        if (RTStrAPrintf(&pTransfer->pszPathRootAbs, "") < 0)
            rc = VERR_NO_MEMORY;
    }

    shClTransferUnlock(pTransfer);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Initializes the root list entries for a given clipboard transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to set transfer list entries for.
 * @param   pszRoots            String list (separated by CRLF) of root entries to set.
 *                              All entries must have the same root path.
 * @param   cbRoots             Size (in bytes) of string list. Includes zero terminator.
 *
 * @note    Accepts local paths or URI string lists (absolute only).
 */
int ShClTransferRootsInitFromStringList(PSHCLTRANSFER pTransfer, const char *pszRoots, size_t cbRoots)
{
    AssertPtrReturn(pTransfer,      VERR_INVALID_POINTER);
    AssertPtrReturn(pszRoots,       VERR_INVALID_POINTER);
    AssertReturn(cbRoots,           VERR_INVALID_PARAMETER);

    LogFlowFuncEnter();

    if (!RTStrIsValidEncoding(pszRoots))
        return VERR_INVALID_UTF8_ENCODING;

    int rc = VINF_SUCCESS;

    shClTransferLock(pTransfer);

    shClTransferRootsReset(pTransfer);

    PSHCLLIST pLstRoots      = &pTransfer->lstRoots;
    char     *pszPathRootAbs = NULL;

    RTCList<RTCString> lstRootEntries = RTCString(pszRoots, cbRoots - 1).split(SHCL_TRANSFER_URI_LIST_SEP_STR);
    if (!lstRootEntries.size())
    {
        shClTransferUnlock(pTransfer);
        return VINF_SUCCESS;
    }

    for (size_t i = 0; i < lstRootEntries.size(); ++i)
    {
        char *pszPathCur = NULL;

        char *pszPath = NULL;
        rc = RTUriFilePathEx(lstRootEntries.at(i).c_str(),
                             RTPATH_STR_F_STYLE_UNIX, &pszPath, 0 /*cbPath*/, NULL /*pcchPath*/);
        if (RT_SUCCESS(rc))
        {
            pszPathCur = pszPath;
            pszPath    = NULL; /* pszPath has ownership now. */
        }
        else if (rc == VERR_URI_NOT_FILE_SCHEME) /* Local file path? */
        {
            pszPathCur = RTStrDup(lstRootEntries.at(i).c_str());
            rc = VINF_SUCCESS;
        }

        LogFlowFunc(("pszPathCur=%s\n", pszPathCur));

        rc = ShClTransferValidatePath(pszPathCur, false);
        if (RT_FAILURE(rc))
        {
            RT_BREAKPOINT();
            break;
        }

        /* No root path determined yet? */
        if (!pszPathRootAbs)
        {
            pszPathRootAbs = RTStrDup(pszPathCur);
            if (pszPathRootAbs)
            {
                RTPathStripFilename(pszPathRootAbs);

                LogFlowFunc(("pszPathRootAbs=%s\n", pszPathRootAbs));

                /* We don't want to have a relative directory here. */
                if (RTPathStartsWithRoot(pszPathRootAbs))
                {
                    rc = ShClTransferValidatePath(pszPathRootAbs, true /* Path must exist */);
                }
                else
                    rc = VERR_PATH_IS_RELATIVE;
            }
            else
                rc = VERR_NO_MEMORY;
        }

        if (RT_SUCCESS(rc))
        {
            PSHCLLISTENTRY pEntry;
            rc = ShClTransferListEntryAlloc(&pEntry);
            if (RT_SUCCESS(rc))
            {
                PSHCLFSOBJINFO pFsObjInfo = (PSHCLFSOBJINFO )RTMemAlloc(sizeof(SHCLFSOBJINFO));
                if (pFsObjInfo)
                {
                    rc = ShClFsObjInfoQuery(pszPathCur, pFsObjInfo);
                    if (RT_SUCCESS(rc))
                    {
                        /* Calculate the relative path within the root path. */
                        const char *pszPathRelToRoot = &pszPathRootAbs[strlen(pszPathRootAbs) + 1 /* Skip terminator or (back)slash. */];
                        if (    pszPathRelToRoot
                            && *pszPathRelToRoot != '\0')
                        {
                            LogFlowFunc(("pszPathRelToRoot=%s\n", pszPathRelToRoot));

                            rc = ShClTransferListEntryInitEx(pEntry, VBOX_SHCL_INFO_F_FSOBJINFO, pszPathRelToRoot,
                                                             pFsObjInfo, sizeof(SHCLFSOBJINFO));
                            if (RT_SUCCESS(rc))
                            {
                                rc = ShClTransferListAddEntry(pLstRoots, pEntry, true /* fAppend */);
                                if (RT_SUCCESS(rc))
                                {
                                    pFsObjInfo = NULL; /* pEntry has ownership now. */
                                }
                            }
                        }
                        else
                            LogRel(("Shared Clipboard: Unable to construct relative path for '%s' (root is '%s')\n",
                                    pszPathCur, pszPathRootAbs));
                    }

                    if (pFsObjInfo)
                    {
                        RTMemFree(pFsObjInfo);
                        pFsObjInfo = NULL;
                    }
                }
                else
                    rc = VERR_NO_MEMORY;

                if (RT_FAILURE(rc))
                    ShClTransferListEntryFree(pEntry);
            }
        }

        RTStrFree(pszPathCur);
    }

    /* No (valid) root directory found? Bail out early. */
    if (!pszPathRootAbs)
        rc = VERR_PATH_DOES_NOT_START_WITH_ROOT;

    if (RT_SUCCESS(rc))
    {
        pTransfer->pszPathRootAbs = pszPathRootAbs;
        LogFlowFunc(("pszPathRootAbs=%s, cRoots=%zu\n", pTransfer->pszPathRootAbs, pTransfer->lstRoots.Hdr.cEntries));

        LogRel2(("Shared Clipboard: Transfer uses root '%s'\n", pTransfer->pszPathRootAbs));
    }
    else
    {
        LogRel(("Shared Clipboard: Unable to set roots for transfer, rc=%Rrc\n", rc));
        ShClTransferListDestroy(pLstRoots);
        RTStrFree(pszPathRootAbs);
    }

    shClTransferUnlock(pTransfer);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Initializes a single file as a transfer root.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to set transfer list entries for.
 * @param   pszFile             File to use as transfer root.
 *
 * @note    Convenience function, uses ShClTransferRootsSet() internally.
 */
int ShClTransferRootsInitFromFile(PSHCLTRANSFER pTransfer, const char *pszFile)
{
    char *pszRoots = NULL;

    LogFlowFuncEnter();

    int rc = RTStrAAppend(&pszRoots, pszFile);
    AssertRCReturn(rc, rc);
    rc = RTStrAAppend(&pszRoots, "\r\n");
    AssertRCReturn(rc, rc);
    rc =  ShClTransferRootsInitFromStringList(pTransfer, pszRoots, strlen(pszRoots) + 1 /* Include terminator */);
    RTStrFree(pszRoots);
    return rc;
}

/**
 * Returns the clipboard transfer's ID.
 *
 * @returns The transfer's ID.
 * @param   pTransfer           Clipboard transfer to return ID for.
 */
SHCLTRANSFERID ShClTransferGetID(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, 0);

    shClTransferLock(pTransfer);

    SHCLTRANSFERID const uID = pTransfer->State.uID;

    shClTransferUnlock(pTransfer);

    return uID;
}

/**
 * Returns the clipboard transfer's direction.
 *
 * @returns The transfer's direction.
 * @param   pTransfer           Clipboard transfer to return direction for.
 */
SHCLTRANSFERDIR ShClTransferGetDir(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, SHCLTRANSFERDIR_UNKNOWN);

    shClTransferLock(pTransfer);

    SHCLTRANSFERDIR const enmDir = pTransfer->State.enmDir;

    shClTransferUnlock(pTransfer);

    return enmDir;
}

/**
 * Returns the absolute root path of a transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to return absolute root path for.
 * @param   pszPath             Where to store the returned path.
 * @param   cbPath              Size (in bytes) of  \a pszPath.
 */
int ShClTransferGetRootPathAbs(PSHCLTRANSFER pTransfer, char *pszPath, size_t cbPath)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    shClTransferLock(pTransfer);

    AssertMsgReturn(pTransfer->pszPathRootAbs, ("Transfer has no root path set (yet)\n"), VERR_WRONG_ORDER);

    int const rc = RTStrCopy(pszPath, cbPath, pTransfer->pszPathRootAbs);

    shClTransferUnlock(pTransfer);

    return rc;
}

/**
 * Returns the transfer's source.
 *
 * @returns The transfer's source.
 * @param   pTransfer           Clipboard transfer to return source for.
 */
SHCLSOURCE ShClTransferGetSource(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, SHCLSOURCE_INVALID);

    shClTransferLock(pTransfer);

    SHCLSOURCE const enmSource = pTransfer->State.enmSource;

    shClTransferUnlock(pTransfer);

    return enmSource;
}

/**
 * Returns the current transfer status.
 *
 * @returns Current transfer status.
 * @param   pTransfer           Clipboard transfer to return status for.
 *
 * @note    Caller needs to take critical section.
 */
DECLINLINE(SHCLTRANSFERSTATUS) shClTransferGetStatusLocked(PSHCLTRANSFER pTransfer)
{
    Assert(RTCritSectIsOwner(&pTransfer->CritSect));

    shClTransferLock(pTransfer);

    SHCLTRANSFERSTATUS const enmStatus = pTransfer->State.enmStatus;

    shClTransferUnlock(pTransfer);

    return enmStatus;
}

/**
 * Returns the current transfer status.
 *
 * @returns Current transfer status.
 * @param   pTransfer           Clipboard transfer to return status for.
 */
SHCLTRANSFERSTATUS ShClTransferGetStatus(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, SHCLTRANSFERSTATUS_NONE);

    shClTransferLock(pTransfer);

    SHCLTRANSFERSTATUS const enmSts = shClTransferGetStatusLocked(pTransfer);

    shClTransferUnlock(pTransfer);

    return enmSts;
}

/**
 * Runs a started clipboard transfer in a dedicated thread.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to run.
 * @param   pfnThreadFunc       Pointer to thread function to use.
 * @param   pvUser              Pointer to user-provided data. Optional.
 */
int ShClTransferRun(PSHCLTRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser)
{
    AssertPtrReturn(pTransfer,     VERR_INVALID_POINTER);
    AssertPtrReturn(pfnThreadFunc, VERR_INVALID_POINTER);
    /* pvUser is optional. */

    AssertMsgReturn(pTransfer->State.enmStatus == SHCLTRANSFERSTATUS_STARTED,
                    ("Wrong status (currently is %s)\n", ShClTransferStatusToStr(pTransfer->State.enmStatus)),
                    VERR_WRONG_ORDER);

    int rc = shClTransferThreadCreate(pTransfer, pfnThreadFunc, pvUser);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Starts an initialized transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to start.
 */
int ShClTransferStart(PSHCLTRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    shClTransferLock(pTransfer);

    /* Ready to start? */
    AssertMsgReturn(pTransfer->ProviderIface.pfnRootListRead != NULL,
                    ("No provider interface set (yet)\n"),
                    VERR_WRONG_ORDER);
    AssertMsgReturn(pTransfer->State.enmStatus == SHCLTRANSFERSTATUS_INITIALIZED,
                    ("Wrong status (currently is %s)\n", ShClTransferStatusToStr(pTransfer->State.enmStatus)),
                    VERR_WRONG_ORDER);

    int rc = VINF_SUCCESS;

    pTransfer->State.enmStatus = SHCLTRANSFERSTATUS_STARTED;

    shClTransferUnlock(pTransfer);

    if (pTransfer->Callbacks.pfnOnStarted)
        pTransfer->Callbacks.pfnOnStarted(&pTransfer->CallbackCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates a thread for a clipboard transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to create thread for.
 * @param   pfnThreadFunc       Thread function to use for this transfer.
 * @param   pvUser              Pointer to user-provided data.
 */
static int shClTransferThreadCreate(PSHCLTRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser)

{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    shClTransferLock(pTransfer);

    /* Already marked for stopping? */
    AssertMsgReturn(pTransfer->Thread.fStop == false,
                    ("Transfer thread already marked for stopping"), VERR_WRONG_ORDER);
    /* Already started? */
    AssertMsgReturn(pTransfer->Thread.fStarted == false,
                    ("Transfer thread already started"), VERR_WRONG_ORDER);

    /* Spawn a worker thread, so that we don't block the window thread for too long. */
    int rc = RTThreadCreate(&pTransfer->Thread.hThread, pfnThreadFunc,
                            pvUser, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "shclptx");
    if (RT_SUCCESS(rc))
    {
        shClTransferUnlock(pTransfer); /* Leave lock while waiting. */

        int rc2 = RTThreadUserWait(pTransfer->Thread.hThread, RT_MS_30SEC /* Timeout in ms */);
        AssertRC(rc2);

        shClTransferLock(pTransfer);

        if (pTransfer->Thread.fStarted) /* Did the thread indicate that it started correctly? */
        {
            /* Nothing to do in here. */
        }
        else
            rc = VERR_GENERAL_FAILURE; /** @todo Find a better rc. */
    }

    shClTransferUnlock(pTransfer);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys the thread of a clipboard transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to destroy thread for.
 * @param   uTimeoutMs          Timeout (in ms) to wait for thread creation.
 */
static int shClTransferThreadDestroy(PSHCLTRANSFER pTransfer, RTMSINTERVAL uTimeoutMs)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    shClTransferLock(pTransfer);

    if (pTransfer->Thread.hThread == NIL_RTTHREAD)
    {
        shClTransferUnlock(pTransfer);
        return VINF_SUCCESS;
    }

    LogFlowFuncEnter();

    /* Set stop indicator. */
    pTransfer->Thread.fStop = true;

    shClTransferUnlock(pTransfer); /* Leave lock while waiting. */

    int rcThread = VERR_WRONG_ORDER;
    int rc = RTThreadWait(pTransfer->Thread.hThread, uTimeoutMs, &rcThread);

    LogFlowFunc(("Waiting for thread resulted in %Rrc (thread exited with %Rrc)\n", rc, rcThread));

    return rc;
}


/*********************************************************************************************************************************
 * Transfer Context                                                                                                              *
 ********************************************************************************************************************************/

/**
 * Locks a transfer context.
 *
 * @param   pTransferCtx        Transfer context to lock.
 */
DECLINLINE(void) shClTransferCtxLock(PSHCLTRANSFERCTX pTransferCtx)
{
    int rc2 = RTCritSectEnter(&pTransferCtx->CritSect);
    AssertRC(rc2);
}

/**
 * Unlocks a transfer context.
 *
 * @param   pTransferCtx        Transfer context to unlock.
 */
DECLINLINE(void) shClTransferCtxUnlock(PSHCLTRANSFERCTX pTransferCtx)
{
    int rc2 = RTCritSectLeave(&pTransferCtx->CritSect);
    AssertRC(rc2);
}

/**
 * Initializes a clipboard transfer context.
 *
 * @returns VBox status code.
 * @param   pTransferCtx                Transfer context to initialize.
 */
int ShClTransferCtxInit(PSHCLTRANSFERCTX pTransferCtx)
{
    AssertPtrReturn(pTransferCtx, VERR_INVALID_POINTER);

    LogFlowFunc(("pTransferCtx=%p\n", pTransferCtx));

    int rc = RTCritSectInit(&pTransferCtx->CritSect);
    if (RT_SUCCESS(rc))
    {
        RTListInit(&pTransferCtx->List);

        pTransferCtx->cTransfers  = 0;
        pTransferCtx->cRunning    = 0;
        pTransferCtx->cMaxRunning = 64; /** @todo Make this configurable? */

        RT_ZERO(pTransferCtx->bmTransferIds);

        ShClTransferCtxReset(pTransferCtx);
    }

    return VINF_SUCCESS;
}

/**
 * Destroys a clipboard transfer context.
 *
 * @param   pTransferCtx                Transfer context to destroy.
 */
void ShClTransferCtxDestroy(PSHCLTRANSFERCTX pTransferCtx)
{
    if (!pTransferCtx)
        return;

    LogFlowFunc(("pTransferCtx=%p\n", pTransferCtx));

    shClTransferCtxLock(pTransferCtx);

    PSHCLTRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pTransferCtx->List, pTransfer, pTransferNext, SHCLTRANSFER, Node)
    {
        ShClTransferDestroy(pTransfer);

        shclTransferCtxTransferRemoveAndUnregister(pTransferCtx, pTransfer);

        RTMemFree(pTransfer);
        pTransfer = NULL;
    }

    pTransferCtx->cRunning   = 0;
    pTransferCtx->cTransfers = 0;

    shClTransferCtxUnlock(pTransferCtx);

    if (RTCritSectIsInitialized(&pTransferCtx->CritSect))
        RTCritSectDelete(&pTransferCtx->CritSect);
}

/**
 * Resets a clipboard transfer context.
 *
 * @param   pTransferCtx                Transfer context to reset.
 */
void ShClTransferCtxReset(PSHCLTRANSFERCTX pTransferCtx)
{
    AssertPtrReturnVoid(pTransferCtx);

    shClTransferCtxLock(pTransferCtx);

    LogFlowFuncEnter();

    PSHCLTRANSFER pTransfer;
    RTListForEach(&pTransferCtx->List, pTransfer, SHCLTRANSFER, Node)
        ShClTransferReset(pTransfer);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS_HTTP
    /** @todo Anything to do here? */
#endif

    shClTransferCtxUnlock(pTransferCtx);
}

/**
 * Returns a specific clipboard transfer, internal version.
 *
 * @returns Clipboard transfer found, or NULL if not found.
 * @param   pTransferCtx                Transfer context to return transfer for.
 * @param   uID                         ID of the transfer to return.
 *
 * @note    Caller needs to take critical section.
 */
static PSHCLTRANSFER shClTransferCtxGetTransferByIdInternal(PSHCLTRANSFERCTX pTransferCtx, uint32_t uID)
{
    Assert(RTCritSectIsOwner(&pTransferCtx->CritSect));

    PSHCLTRANSFER pTransfer;
    RTListForEach(&pTransferCtx->List, pTransfer, SHCLTRANSFER, Node) /** @todo Slow, but works for now. */
    {
        if (pTransfer->State.uID == uID)
            return pTransfer;
    }

    return NULL;
}

/**
 * Returns a specific clipboard transfer by index, internal version.
 *
 * @returns Clipboard transfer found, or NULL if not found.
 * @param   pTransferCtx                Transfer context to return transfer for.
 * @param   uIdx                        Index of the transfer to return.
 *
 * @note    Caller needs to take critical section.
 */
static PSHCLTRANSFER shClTransferCtxGetTransferByIndexInternal(PSHCLTRANSFERCTX pTransferCtx, uint32_t uIdx)
{
    Assert(RTCritSectIsOwner(&pTransferCtx->CritSect));

    uint32_t idx = 0;

    PSHCLTRANSFER pTransfer;
    RTListForEach(&pTransferCtx->List, pTransfer, SHCLTRANSFER, Node) /** @todo Slow, but works for now. */
    {
        if (uIdx == idx)
            return pTransfer;
        idx++;
    }

    return NULL;
}

/**
 * Returns a clipboard transfer for a specific transfer ID.
 *
 * @returns Clipboard transfer found, or NULL if not found.
 * @param   pTransferCtx                Transfer context to return transfer for.
 * @param   uID                         ID of the transfer to return.
 */
PSHCLTRANSFER ShClTransferCtxGetTransferById(PSHCLTRANSFERCTX pTransferCtx, uint32_t uID)
{
    shClTransferCtxLock(pTransferCtx);

    PSHCLTRANSFER const pTransfer = shClTransferCtxGetTransferByIdInternal(pTransferCtx, uID);

    shClTransferCtxUnlock(pTransferCtx);

    return pTransfer;
}

/**
 * Returns a clipboard transfer for a specific list index.
 *
 * @returns Clipboard transfer found, or NULL if not found.
 * @param   pTransferCtx                Transfer context to return transfer for.
 * @param   uIdx                        List index of the transfer to return.
 */
PSHCLTRANSFER ShClTransferCtxGetTransferByIndex(PSHCLTRANSFERCTX pTransferCtx, uint32_t uIdx)
{
    shClTransferCtxLock(pTransferCtx);

    PSHCLTRANSFER const pTransfer = shClTransferCtxGetTransferByIndexInternal(pTransferCtx, uIdx);

    shClTransferCtxUnlock(pTransferCtx);

    return pTransfer;
}

/**
 * Returns the number of running clipboard transfers for a given transfer context.
 *
 * @returns Number of running transfers.
 * @param   pTransferCtx                Transfer context to return number for.
 */
uint32_t ShClTransferCtxGetRunningTransfers(PSHCLTRANSFERCTX pTransferCtx)
{
    AssertPtrReturn(pTransferCtx, 0);

    shClTransferCtxLock(pTransferCtx);

    uint32_t const cRunning = pTransferCtx->cRunning;

    shClTransferCtxUnlock(pTransferCtx);

    return cRunning;
}

/**
 * Returns the number of total clipboard transfers for a given transfer context.
 *
 * @returns Number of total transfers.
 * @param   pTransferCtx                Transfer context to return number for.
 */
uint32_t ShClTransferCtxGetTotalTransfers(PSHCLTRANSFERCTX pTransferCtx)
{
    AssertPtrReturn(pTransferCtx, 0);

    shClTransferCtxLock(pTransferCtx);

    uint32_t const cTransfers = pTransferCtx->cTransfers;

    shClTransferCtxUnlock(pTransferCtx);

    return cTransfers;
}

/**
 * Registers a clipboard transfer with a transfer context, i.e. allocates a transfer ID.
 *
 * @return  VBox status code.
 * @retval  VERR_SHCLPB_MAX_TRANSFERS_REACHED if the maximum of concurrent transfers
 *          is reached.
 * @param   pTransferCtx        Transfer context to register transfer to.
 * @param   pTransfer           Transfer to register. The context takes ownership of the transfer on success.
 * @param   pidTransfer         Where to return the transfer ID on success. Optional.
 */
int ShClTransferCtxTransferRegister(PSHCLTRANSFERCTX pTransferCtx, PSHCLTRANSFER pTransfer, SHCLTRANSFERID *pidTransfer)
{
    AssertPtrReturn(pTransferCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    /* pidTransfer is optional. */

    shClTransferCtxLock(pTransferCtx);

    /*
     * Pick a random bit as starting point.  If it's in use, search forward
     * for a free one, wrapping around.  We've reserved both the zero'th and
     * max-1 IDs.
     */
    SHCLTRANSFERID idTransfer = RTRandU32Ex(1, VBOX_SHCL_MAX_TRANSFERS - 2);

    if (!ASMBitTestAndSet(&pTransferCtx->bmTransferIds[0], idTransfer))
    { /* likely */ }
    else if (pTransferCtx->cTransfers < VBOX_SHCL_MAX_TRANSFERS - 2 /* First and last are not used */)
    {
        /* Forward search. */
        int iHit = ASMBitNextClear(&pTransferCtx->bmTransferIds[0], VBOX_SHCL_MAX_TRANSFERS, idTransfer);
        if (iHit < 0)
            iHit = ASMBitFirstClear(&pTransferCtx->bmTransferIds[0], VBOX_SHCL_MAX_TRANSFERS);
        AssertLogRelMsgReturn(iHit >= 0, ("Transfer count: %RU16\n", pTransferCtx->cTransfers), VERR_SHCLPB_MAX_TRANSFERS_REACHED);
        idTransfer = iHit;
        AssertLogRelMsgReturn(!ASMBitTestAndSet(&pTransferCtx->bmTransferIds[0], idTransfer), ("idObject=%#x\n", idTransfer), VERR_INTERNAL_ERROR_2);
    }
    else
    {
        LogFunc(("Maximum number of transfers reached (%RU16 transfers)\n", pTransferCtx->cTransfers));
        shClTransferCtxUnlock(pTransferCtx);
        return VERR_SHCLPB_MAX_TRANSFERS_REACHED;
    }

    pTransfer->State.uID = idTransfer;

    RTListAppend(&pTransferCtx->List, &pTransfer->Node);

    pTransferCtx->cTransfers++;

    Log2Func(("pTransfer=%p, idTransfer=%RU32 -- now %RU16 transfer(s)\n", pTransfer, idTransfer, pTransferCtx->cTransfers));

    shClTransferCtxUnlock(pTransferCtx);

    if (pTransfer->Callbacks.pfnOnRegistered)
        pTransfer->Callbacks.pfnOnRegistered(&pTransfer->CallbackCtx, pTransferCtx);

    if (pidTransfer)
        *pidTransfer = idTransfer;

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

/**
 * Registers a clipboard transfer with a transfer context by specifying an ID for the transfer.
 *
 * @return  VBox status code.
 * @retval  VERR_ALREADY_EXISTS if a transfer with the given ID already exists.
 * @retval  VERR_SHCLPB_MAX_TRANSFERS_REACHED if the maximum of concurrent transfers for this context has been reached.
 * @param   pTransferCtx                Transfer context to register transfer to.
 * @param   pTransfer           Transfer to register.
 * @param   idTransfer          Transfer ID to use for registration.
 */
int ShClTransferCtxTransferRegisterById(PSHCLTRANSFERCTX pTransferCtx, PSHCLTRANSFER pTransfer, SHCLTRANSFERID idTransfer)
{
    shClTransferCtxLock(pTransferCtx);

    if (pTransferCtx->cTransfers < VBOX_SHCL_MAX_TRANSFERS - 2 /* First and last are not used */)
    {
        if (!ASMBitTestAndSet(&pTransferCtx->bmTransferIds[0], idTransfer))
        {
            RTListAppend(&pTransferCtx->List, &pTransfer->Node);

            pTransfer->State.uID = idTransfer;

            shClTransferCtxUnlock(pTransferCtx);

            if (pTransfer->Callbacks.pfnOnRegistered)
                pTransfer->Callbacks.pfnOnRegistered(&pTransfer->CallbackCtx, pTransferCtx);

            shClTransferCtxLock(pTransferCtx);

            pTransferCtx->cTransfers++;

            LogFunc(("Registered transfer ID %RU16 -- now %RU16 transfers total\n", idTransfer, pTransferCtx->cTransfers));

            shClTransferCtxUnlock(pTransferCtx);
            return VINF_SUCCESS;
        }

        return VERR_ALREADY_EXISTS;
    }

    LogFunc(("Maximum number of transfers reached (%RU16 transfers)\n", pTransferCtx->cTransfers));

    shClTransferCtxUnlock(pTransferCtx);

    return VERR_SHCLPB_MAX_TRANSFERS_REACHED;
}

/**
 * Removes and unregisters a transfer from a transfer context.
 *
 * @param   pTransferCtx        Transfer context to remove transfer from.
 * @param   pTransfer           Transfer to remove.
 *
 * @note    Caller needs to take critical section.
 */
static void shclTransferCtxTransferRemoveAndUnregister(PSHCLTRANSFERCTX pTransferCtx, PSHCLTRANSFER pTransfer)
{
    Assert(RTCritSectIsOwner(&pTransferCtx->CritSect));

    RTListNodeRemove(&pTransfer->Node);

    Assert(pTransferCtx->cTransfers);
    pTransferCtx->cTransfers--;

    Assert(pTransferCtx->cTransfers >= pTransferCtx->cRunning);

    shClTransferCtxUnlock(pTransferCtx);

    if (pTransfer->Callbacks.pfnOnUnregistered)
        pTransfer->Callbacks.pfnOnUnregistered(&pTransfer->CallbackCtx, pTransferCtx);

    shClTransferCtxLock(pTransferCtx);

    LogFlowFunc(("Now %RU32 transfers left\n", pTransferCtx->cTransfers));
}

/**
 * Unregisters a transfer from an transfer context, given by its ID.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_FOUND if the transfer ID was not found.
 * @param   pTransferCtx        Transfer context to unregister transfer from.
 * @param   idTransfer          Transfer ID to unregister.
 */
int ShClTransferCtxTransferUnregisterById(PSHCLTRANSFERCTX pTransferCtx, SHCLTRANSFERID idTransfer)
{
    AssertPtrReturn(pTransferCtx, VERR_INVALID_POINTER);
    AssertReturn(idTransfer, VERR_INVALID_PARAMETER);

    shClTransferCtxLock(pTransferCtx);

    int rc = VINF_SUCCESS;
    AssertMsgStmt(ASMBitTestAndClear(&pTransferCtx->bmTransferIds, idTransfer), ("idTransfer=%#x\n", idTransfer), rc = VERR_NOT_FOUND);

    LogFlowFunc(("idTransfer=%RU32\n", idTransfer));

    if (RT_SUCCESS(rc))
    {
        PSHCLTRANSFER pTransfer = shClTransferCtxGetTransferByIdInternal(pTransferCtx, idTransfer);
        if (pTransfer)
        {
            shclTransferCtxTransferRemoveAndUnregister(pTransferCtx, pTransfer);
        }
        else
            rc = VERR_NOT_FOUND;
    }

    shClTransferCtxUnlock(pTransferCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Cleans up all associated transfers which are not needed (anymore).
 * This can be due to transfers which only have been announced but not / never being run.
 *
 * @param   pTransferCtx        Transfer context to cleanup transfers for.
 */
void ShClTransferCtxCleanup(PSHCLTRANSFERCTX pTransferCtx)
{
    AssertPtrReturnVoid(pTransferCtx);

    shClTransferCtxLock(pTransferCtx);

    LogFlowFunc(("pTransferCtx=%p, cTransfers=%RU16 cRunning=%RU16\n",
                 pTransferCtx, pTransferCtx->cTransfers, pTransferCtx->cRunning));

    if (pTransferCtx->cTransfers == 0)
    {
        shClTransferCtxUnlock(pTransferCtx);
        return;
    }

    /* Remove all transfers which are not in a running state (e.g. only announced). */
    PSHCLTRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pTransferCtx->List, pTransfer, pTransferNext, SHCLTRANSFER, Node)
    {
        shClTransferLock(pTransfer);

        SHCLTRANSFERSTATUS const enmStatus = shClTransferGetStatusLocked(pTransfer);
        LogFlowFunc(("\tTransfer #%RU16: %s\n", pTransfer->State.uID, ShClTransferStatusToStr(enmStatus)));

        if (enmStatus != SHCLTRANSFERSTATUS_STARTED)
        {
            shClTransferUnlock(pTransfer);

            shclTransferCtxTransferRemoveAndUnregister(pTransferCtx, pTransfer);

            ShClTransferDestroy(pTransfer);

            RTMemFree(pTransfer);
            pTransfer = NULL;
        }
        else
            shClTransferUnlock(pTransfer);
    }

    shClTransferCtxUnlock(pTransferCtx);
}

/**
 * Returns whether the maximum of concurrent transfers of a specific transfer contexthas been reached or not.
 *
 * @returns \c if maximum has been reached, \c false if not.
 * @param   pTransferCtx        Transfer context to determine value for.
 */
bool ShClTransferCtxTransfersMaximumReached(PSHCLTRANSFERCTX pTransferCtx)
{
    AssertPtrReturn(pTransferCtx, true);

    shClTransferCtxLock(pTransferCtx);

    LogFlowFunc(("cRunning=%RU32, cMaxRunning=%RU32\n", pTransferCtx->cRunning, pTransferCtx->cMaxRunning));

    Assert(pTransferCtx->cRunning <= pTransferCtx->cMaxRunning);
    bool const fMaximumReached = pTransferCtx->cRunning == pTransferCtx->cMaxRunning;

    shClTransferCtxUnlock(pTransferCtx);

    return fMaximumReached;
}

/**
 * Copies file system objinfo from IPRT to Shared Clipboard format.
 *
 * @return VBox status code.
 * @param  pDst                 The Shared Clipboard structure to convert data to.
 * @param  pSrc                 The IPRT structure to convert data from.
 */
int ShClFsObjInfoFromIPRT(PSHCLFSOBJINFO pDst, PCRTFSOBJINFO pSrc)
{
    AssertPtrReturn(pDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrc, VERR_INVALID_POINTER);

    pDst->cbObject          = pSrc->cbObject;
    pDst->cbAllocated       = pSrc->cbAllocated;
    pDst->AccessTime        = pSrc->AccessTime;
    pDst->ModificationTime  = pSrc->ModificationTime;
    pDst->ChangeTime        = pSrc->ChangeTime;
    pDst->BirthTime         = pSrc->BirthTime;
    pDst->Attr.fMode        = pSrc->Attr.fMode;
    /* Clear bits which we don't pass through for security reasons. */
    pDst->Attr.fMode       &= ~(RTFS_UNIX_ISUID | RTFS_UNIX_ISGID | RTFS_UNIX_ISTXT);
    RT_ZERO(pDst->Attr.u);
    switch (pSrc->Attr.enmAdditional)
    {
        default:
            RT_FALL_THROUGH();
        case RTFSOBJATTRADD_NOTHING:
            pDst->Attr.enmAdditional        = SHCLFSOBJATTRADD_NOTHING;
            break;

        case RTFSOBJATTRADD_UNIX:
            pDst->Attr.enmAdditional        = SHCLFSOBJATTRADD_UNIX;
            pDst->Attr.u.Unix.uid           = pSrc->Attr.u.Unix.uid;
            pDst->Attr.u.Unix.gid           = pSrc->Attr.u.Unix.gid;
            pDst->Attr.u.Unix.cHardlinks    = pSrc->Attr.u.Unix.cHardlinks;
            pDst->Attr.u.Unix.INodeIdDevice = pSrc->Attr.u.Unix.INodeIdDevice;
            pDst->Attr.u.Unix.INodeId       = pSrc->Attr.u.Unix.INodeId;
            pDst->Attr.u.Unix.fFlags        = pSrc->Attr.u.Unix.fFlags;
            pDst->Attr.u.Unix.GenerationId  = pSrc->Attr.u.Unix.GenerationId;
            pDst->Attr.u.Unix.Device        = pSrc->Attr.u.Unix.Device;
            break;

        case RTFSOBJATTRADD_EASIZE:
            pDst->Attr.enmAdditional        = SHCLFSOBJATTRADD_EASIZE;
            pDst->Attr.u.EASize.cb          = pSrc->Attr.u.EASize.cb;
            break;
    }

    return VINF_SUCCESS;
}

/**
 * Queries Shared Clipboard file system information from a given path.
 *
 * @returns VBox status code.
 * @param   pszPath             Path to query file system information for.
 * @param   pObjInfo            Where to return the queried file system information on success.
 */
int ShClFsObjInfoQuery(const char *pszPath, PSHCLFSOBJINFO pObjInfo)
{
    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(pszPath, &objInfo,
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
                            RTFSOBJATTRADD_NOTHING
#else
                            RTFSOBJATTRADD_UNIX
#endif
                            );
    if (RT_SUCCESS(rc))
        rc = ShClFsObjInfoFromIPRT(pObjInfo, &objInfo);

    return rc;
}

/**
 * Translates a clipboard transfer status (SHCLTRANSFERSTATUS_XXX) into a string.
 *
 * @returns Transfer status string name.
 * @param   enmStatus           The transfer status to translate.
 */
const char *ShClTransferStatusToStr(SHCLTRANSFERSTATUS enmStatus)
{
    switch (enmStatus)
    {
        RT_CASE_RET_STR(SHCLTRANSFERSTATUS_NONE);
        RT_CASE_RET_STR(SHCLTRANSFERSTATUS_INITIALIZED);
        RT_CASE_RET_STR(SHCLTRANSFERSTATUS_UNINITIALIZED);
        RT_CASE_RET_STR(SHCLTRANSFERSTATUS_STARTED);
        RT_CASE_RET_STR(SHCLTRANSFERSTATUS_STOPPED);
        RT_CASE_RET_STR(SHCLTRANSFERSTATUS_CANCELED);
        RT_CASE_RET_STR(SHCLTRANSFERSTATUS_KILLED);
        RT_CASE_RET_STR(SHCLTRANSFERSTATUS_ERROR);
    }
    return "Unknown";
}

/**
 * Validates whether a given path matches our set of rules or not.
 *
 * @returns VBox status code.
 * @param   pcszPath            Path to validate.
 * @param   fMustExist          Whether the path to validate also must exist.
 */
int ShClTransferValidatePath(const char *pcszPath, bool fMustExist)
{
    int rc = VINF_SUCCESS;

    if (!strlen(pcszPath))
        rc = VERR_INVALID_PARAMETER;

    if (   RT_SUCCESS(rc)
        && !RTStrIsValidEncoding(pcszPath))
    {
        rc = VERR_INVALID_UTF8_ENCODING;
    }

    if (   RT_SUCCESS(rc)
        && RTStrStr(pcszPath, ".."))
    {
        rc = VERR_INVALID_PARAMETER;
    }

    if (   RT_SUCCESS(rc)
        && fMustExist)
    {
        RTFSOBJINFO objInfo;
        rc = RTPathQueryInfo(pcszPath, &objInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_SUCCESS(rc))
        {
            if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
            {
                if (!RTDirExists(pcszPath)) /* Path must exist. */
                    rc = VERR_PATH_NOT_FOUND;
            }
            else if (RTFS_IS_FILE(objInfo.Attr.fMode))
            {
                if (!RTFileExists(pcszPath)) /* File must exist. */
                    rc = VERR_FILE_NOT_FOUND;
            }
            else /* Everything else (e.g. symbolic links) are not supported. */
            {
                LogRelMax(64, ("Shared Clipboard: Path '%s' contains a symbolic link or junction, which are not supported\n", pcszPath));
                rc = VERR_NOT_SUPPORTED;
            }
        }
    }

    if (RT_FAILURE(rc))
        LogRelMax(64, ("Shared Clipboard: Validating path '%s' failed: %Rrc\n", pcszPath, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}
