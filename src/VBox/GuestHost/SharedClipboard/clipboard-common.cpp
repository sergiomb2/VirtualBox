/* $Id$ */
/** @file
 * Shared Clipboard: Some helper function for converting between the various eol.
 */

/*
 * Includes contributions from François Revol
 *
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

#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/path.h>

#include <iprt/errcore.h>
#include <VBox/log.h>
#include <VBox/GuestHost/clipboard-helper.h>
#ifdef LOG_ENABLED
# include <VBox/HostServices/VBoxClipboardSvc.h>
#endif

/** @todo use const where appropriate; delinuxify the code (*Lin* -> *Host*); use AssertLogRel*. */

int vboxClipboardUtf16GetWinSize(PRTUTF16 pwszSrc, size_t cwSrc, size_t *pcwDest)
{
    size_t cwDest, i;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    AssertLogRelMsgReturn(pwszSrc != NULL, ("vboxClipboardUtf16GetWinSize: received a null Utf16 string, returning VERR_INVALID_PARAMETER\n"), VERR_INVALID_PARAMETER);
    if (cwSrc == 0)
    {
        *pcwDest = 0;
        LogFlowFunc(("empty source string, returning\n"));
        return VINF_SUCCESS;
    }
/** @todo convert the remainder of the Assert stuff to AssertLogRel. */
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16GetWinSize: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    cwDest = 0;
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    for (i = (pwszSrc[0] == UTF16LEMARKER ? 1 : 0); i < cwSrc; ++i, ++cwDest)
    {
        /* Check for a single line feed */
        if (pwszSrc[i] == LINEFEED)
            ++cwDest;
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        if (pwszSrc[i] == CARRIAGERETURN)
            ++cwDest;
#endif
        if (pwszSrc[i] == 0)
        {
            /* Don't count this, as we do so below. */
            break;
        }
    }
    /* Count the terminating null byte. */
    ++cwDest;
    LogFlowFunc(("returning VINF_SUCCESS, %d 16bit words\n", cwDest));
    *pcwDest = cwDest;
    return VINF_SUCCESS;
}

int vboxClipboardUtf16LinToWin(PRTUTF16 pwszSrc, size_t cwSrc, PRTUTF16 pu16Dest,
                               size_t cwDest)
{
    size_t i, j;
    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    if (!VALID_PTR(pwszSrc) || !VALID_PTR(pu16Dest))
    {
        LogRel(("vboxClipboardUtf16LinToWin: received an invalid pointer, pwszSrc=%p, pu16Dest=%p, returning VERR_INVALID_PARAMETER\n", pwszSrc, pu16Dest));
        AssertReturn(VALID_PTR(pwszSrc) && VALID_PTR(pu16Dest), VERR_INVALID_PARAMETER);
    }
    if (cwSrc == 0)
    {
        if (cwDest == 0)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        pu16Dest[0] = 0;
        LogFlowFunc(("empty source string, returning\n"));
        return VINF_SUCCESS;
    }
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16LinToWin: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    /* Don't copy the endian marker. */
    for (i = (pwszSrc[0] == UTF16LEMARKER ? 1 : 0), j = 0; i < cwSrc; ++i, ++j)
    {
        /* Don't copy the null byte, as we add it below. */
        if (pwszSrc[i] == 0)
            break;
        if (j == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (pwszSrc[i] == LINEFEED)
        {
            pu16Dest[j] = CARRIAGERETURN;
            ++j;
            if (j == cwDest)
            {
                LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
                return VERR_BUFFER_OVERFLOW;
            }
        }
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        else if (pwszSrc[i] == CARRIAGERETURN)
        {
            /* set cr */
            pu16Dest[j] = CARRIAGERETURN;
            ++j;
            if (j == cwDest)
            {
                LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
                return VERR_BUFFER_OVERFLOW;
            }
            /* add the lf */
            pu16Dest[j] = LINEFEED;
            continue;
        }
#endif
        pu16Dest[j] = pwszSrc[i];
    }
    /* Add the trailing null. */
    if (j == cwDest)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    pu16Dest[j] = 0;
    LogFlowFunc(("rc=VINF_SUCCESS, pu16Dest=%ls\n", pu16Dest));
    return VINF_SUCCESS;
}

int vboxClipboardUtf16GetLinSize(PRTUTF16 pwszSrc, size_t cwSrc, size_t *pcwDest)
{
    size_t cwDest;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    if (!VALID_PTR(pwszSrc))
    {
        LogRel(("vboxClipboardUtf16GetLinSize: received an invalid Utf16 string %p.  Returning VERR_INVALID_PARAMETER.\n", pwszSrc));
        AssertReturn(VALID_PTR(pwszSrc), VERR_INVALID_PARAMETER);
    }
    if (cwSrc == 0)
    {
        LogFlowFunc(("empty source string, returning VINF_SUCCESS\n"));
        *pcwDest = 0;
        return VINF_SUCCESS;
    }
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16GetLinSize: received a big endian Utf16 string.  Returning VERR_INVALID_PARAMETER.\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    if (pwszSrc[0] == UTF16LEMARKER)
        cwDest = 0;
    else
        cwDest = 1;
    for (size_t i = 0; i < cwSrc; ++i, ++cwDest)
    {
        if (   (i + 1 < cwSrc)
            && (pwszSrc[i] == CARRIAGERETURN)
            && (pwszSrc[i + 1] == LINEFEED))
        {
            ++i;
        }
        if (pwszSrc[i] == 0)
        {
            break;
        }
    }
    /* Terminating zero */
    ++cwDest;
    LogFlowFunc(("returning %d\n", cwDest));
    *pcwDest = cwDest;
    return VINF_SUCCESS;
}

int vboxClipboardUtf16WinToLin(PRTUTF16 pwszSrc, size_t cwSrc, PRTUTF16 pu16Dest,
                               size_t cwDest)
{
    size_t cwDestPos;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u, pu16Dest=%p, cwDest=%u\n",
                 cwSrc, pwszSrc, cwSrc, pu16Dest, cwDest));
    /* A buffer of size 0 may not be an error, but it is not a good idea either. */
    Assert(cwDest > 0);
    if (!VALID_PTR(pwszSrc) || !VALID_PTR(pu16Dest))
    {
        LogRel(("vboxClipboardUtf16WinToLin: received an invalid ptr, pwszSrc=%p, pu16Dest=%p, returning VERR_INVALID_PARAMETER\n", pwszSrc, pu16Dest));
        AssertReturn(VALID_PTR(pwszSrc) && VALID_PTR(pu16Dest), VERR_INVALID_PARAMETER);
    }
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16WinToLin: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertMsgFailedReturn(("received a big endian string\n"), VERR_INVALID_PARAMETER);
    }
    if (cwDest == 0)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    if (cwSrc == 0)
    {
        pu16Dest[0] = 0;
        LogFlowFunc(("received empty string.  Returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
    /* Prepend the Utf16 byte order marker if it is missing. */
    if (pwszSrc[0] == UTF16LEMARKER)
    {
        cwDestPos = 0;
    }
    else
    {
        pu16Dest[0] = UTF16LEMARKER;
        cwDestPos = 1;
    }
    for (size_t i = 0; i < cwSrc; ++i, ++cwDestPos)
    {
        if (pwszSrc[i] == 0)
        {
            break;
        }
        if (cwDestPos == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (   (i + 1 < cwSrc)
            && (pwszSrc[i] == CARRIAGERETURN)
            && (pwszSrc[i + 1] == LINEFEED))
        {
            ++i;
        }
        pu16Dest[cwDestPos] = pwszSrc[i];
    }
    /* Terminating zero */
    if (cwDestPos == cwDest)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    pu16Dest[cwDestPos] = 0;
    LogFlowFunc(("set string %ls.  Returning\n", pu16Dest + 1));
    return VINF_SUCCESS;
}

int vboxClipboardDibToBmp(const void *pvSrc, size_t cbSrc, void **ppvDest, size_t *pcbDest)
{
    size_t        cb            = sizeof(BMFILEHEADER) + cbSrc;
    PBMFILEHEADER pFileHeader   = NULL;
    void         *pvDest        = NULL;
    size_t        offPixel      = 0;

    AssertPtrReturn(pvSrc,   VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDest, VERR_INVALID_PARAMETER);

    PBMINFOHEADER pBitmapInfoHeader = (PBMINFOHEADER)pvSrc;
    /** @todo Support all the many versions of the DIB headers. */
    if (   cbSrc < sizeof(BMINFOHEADER)
        || RT_LE2H_U32(pBitmapInfoHeader->u32Size) < sizeof(BMINFOHEADER)
        || RT_LE2H_U32(pBitmapInfoHeader->u32Size) != sizeof(BMINFOHEADER))
    {
        Log(("vboxClipboardDibToBmp: invalid or unsupported bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    offPixel = sizeof(BMFILEHEADER)
                + RT_LE2H_U32(pBitmapInfoHeader->u32Size)
                + RT_LE2H_U32(pBitmapInfoHeader->u32ClrUsed) * sizeof(uint32_t);
    if (cbSrc < offPixel)
    {
        Log(("vboxClipboardDibToBmp: invalid bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    pvDest = RTMemAlloc(cb);
    if (!pvDest)
    {
        Log(("writeToPasteboard: cannot allocate memory for bitmap.\n"));
        return VERR_NO_MEMORY;
    }

    pFileHeader = (PBMFILEHEADER)pvDest;
    pFileHeader->u16Type        = BITMAPHEADERMAGIC;
    pFileHeader->u32Size        = (uint32_t)RT_H2LE_U32(cb);
    pFileHeader->u16Reserved1   = pFileHeader->u16Reserved2 = 0;
    pFileHeader->u32OffBits     = (uint32_t)RT_H2LE_U32(offPixel);
    memcpy((uint8_t *)pvDest + sizeof(BMFILEHEADER), pvSrc, cbSrc);
    *ppvDest = pvDest;
    *pcbDest = cb;
    return VINF_SUCCESS;
}

int vboxClipboardBmpGetDib(const void *pvSrc, size_t cbSrc, const void **ppvDest, size_t *pcbDest)
{
    AssertPtrReturn(pvSrc,   VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDest, VERR_INVALID_PARAMETER);

    PBMFILEHEADER pFileHeader = (PBMFILEHEADER)pvSrc;
    if (   cbSrc < sizeof(BMFILEHEADER)
        || pFileHeader->u16Type != BITMAPHEADERMAGIC
        || RT_LE2H_U32(pFileHeader->u32Size) != cbSrc)
    {
        Log(("vboxClipboardBmpGetDib: invalid bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    *ppvDest = ((uint8_t *)pvSrc) + sizeof(BMFILEHEADER);
    *pcbDest = cbSrc - sizeof(BMFILEHEADER);
    return VINF_SUCCESS;
}

#ifdef LOG_ENABLED
int VBoxClipboardDbgDumpHtml(const char *pszSrc, size_t cbSrc)
{
    size_t cchIgnored = 0;
    int rc = RTStrNLenEx(pszSrc, cbSrc, &cchIgnored);
    if (RT_SUCCESS(rc))
    {
        char *pszBuf = (char *)RTMemAllocZ(cbSrc + 1);
        if (pszBuf)
        {
            rc = RTStrCopy(pszBuf, cbSrc + 1, (const char *)pszSrc);
            if (RT_SUCCESS(rc))
            {
                for (size_t i = 0; i < cbSrc; ++i)
                    if (pszBuf[i] == '\n' || pszBuf[i] == '\r')
                        pszBuf[i] = ' ';
            }
            else
                LogFunc(("Error in copying string\n"));
            LogFunc(("Removed \\r\\n: %s\n", pszBuf));
            RTMemFree(pszBuf);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}

void VBoxClipboardDbgDumpData(const void *pv, size_t cb, VBOXCLIPBOARDFORMAT u32Format)
{
    if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT:\n"));
        if (pv && cb)
            LogFunc(("%ls\n", pv));
        else
            LogFunc(("%p %zu\n", pv, cb));
    }
    else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
        LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_BITMAP\n"));
    else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_HTML)
    {
        LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_HTML:\n"));
        if (pv && cb)
        {
            LogFunc(("%s\n", pv));

            //size_t cb = RTStrNLen(pv, );
            char *pszBuf = (char *)RTMemAllocZ(cb + 1);
            RTStrCopy(pszBuf, cb + 1, (const char *)pv);
            for (size_t off = 0; off < cb; ++off)
            {
                if (pszBuf[off] == '\n' || pszBuf[off] == '\r')
                    pszBuf[off] = ' ';
            }

            LogFunc(("%s\n", pszBuf));
            RTMemFree(pszBuf);
        }
        else
            LogFunc(("%p %zu\n", pv, cb));
    }
    else
        LogFunc(("Invalid format %02X\n", u32Format));
}

/**
 * Translates a Shared Clipboard host message enum to a string.
 *
 * @returns Message ID string name.
 * @param   uMsg                The message to translate.
 */
const char *VBoxClipboardHostMsgToStr(uint32_t uMsg)
{
    switch (uMsg)
    {
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_HDR_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_HDR_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_ENTRY_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ROOT_LIST_ENTRY_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_OPEN);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_CLOSE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_CANCEL);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ERROR);
    }
    return "Unknown";
}

/**
 * Translates a Shared Clipboard guest message enum to a string.
 *
 * @returns Message ID string name.
 * @param   uMsg                The message to translate.
 */
const char *VBoxClipboardGuestMsgToStr(uint32_t uMsg)
{
    switch (uMsg)
    {
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_REPORT_FORMATS);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DATA);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_NOWAIT);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_WAIT);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_REPLY);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_HDR_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_HDR_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_ENTRY_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_ROOT_LIST_ENTRY_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_CLOSE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_OPEN);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_CLOSE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_READ);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_WRITE);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_CANCEL);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_GUEST_FN_ERROR);
        RT_CASE_RET_STR(VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT);
    }
    return "Unknown";
}
#endif /* LOG_ENABLED */

