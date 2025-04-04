/** @file
 * IPRT - Advanced Configuration and Power Interface (ACPI) Table generation API.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef IPRT_INCLUDED_acpi_h
#define IPRT_INCLUDED_acpi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/vfs.h>

#include <iprt/formats/acpi-tables.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_acpi   RTAcpi - Advanced Configuration and Power Interface (ACPI) Table generation API.
 * @ingroup grp_rt
 * @{
 */

#ifdef IN_RING3

/**
 * ACPI table type.
 */
typedef enum RTACPITBLTYPE
{
    /** The invalid output type. */
    RTACPITBLTYPE_INVALID = 0,
    /** Type is an UTF-8 ASL source. */
    RTACPITBLTYPE_ASL,
    /** Type is the AML bytecode. */
    RTACPITBLTYPE_AML,
    /** Usual 32-bit hack. */
    RTACPITBLTYPE_32BIT_HACK = 0x7fffffff
} RTACPITBLTYPE;


/**
 * Regenerates the ACPI checksum for the given data.
 *
 * @returns The checksum for the given data.
 * @param   pvData              The data to check sum.
 * @param   cbData              Number of bytes to check sum.
 */
RTDECL(uint8_t) RTAcpiChecksumGenerate(const void *pvData, size_t cbData);


/**
 * Generates and writes the table header checksum for the given ACPI table.
 *
 * @param   pTbl                Pointer to the ACPI table to set the checksum for.
 * @param   cbTbl               Size of the table in bytes, including the ACPI table header.
 */
RTDECL(void) RTAcpiTblHdrChecksumGenerate(PACPITBLHDR pTbl, size_t cbTbl);


/**
 * Creates an ACPI table from the given VFS file.
 *
 * @returns IPRT status code.
 * @param   phAcpiTbl           Where to store the ACPI table handle on success.
 * @param   hVfsIos             The VFS I/O stream handle to read the ACPI table from.
 * @param   enmInType           The input type of the ACPI table.
 * @param   pErrInfo            Where to return additional error information.
 */
RTDECL(int) RTAcpiTblCreateFromVfsIoStrm(PRTACPITBL phAcpiTbl, RTVFSIOSTREAM hVfsIos, RTACPITBLTYPE enmInType, PRTERRINFO pErrInfo);


/**
 * Converts a given ACPI table input stream to the given output type.
 *
 * @returns IPRT status code.
 * @param   hVfsIosOut          The VFS I/O stream handle to output the result to.
 * @param   enmOutType          The output type.
 * @param   hVfsIosIn           The VFS I/O stream handle to read the ACPI table from.
 * @param   enmInType           The input type of the ACPI table.
 * @param   pErrInfo            Where to return additional error information.
 */
RTDECL(int) RTAcpiTblConvertFromVfsIoStrm(RTVFSIOSTREAM hVfsIosOut, RTACPITBLTYPE enmOutType,
                                          RTVFSIOSTREAM hVfsIosIn, RTACPITBLTYPE enmInType, PRTERRINFO pErrInfo);


/**
 * Creates an ACPI table from the given filename.
 *
 * @returns IPRT status code.
 * @param   phAcpiTbl           Where to store the ACPI table handle on success.
 * @param   pszFilename         The filename to read the ACPI table from.
 * @param   enmInType           The input type of the ACPI table.
 * @param   pErrInfo            Where to return additional error information.
 */
RTDECL(int) RTAcpiTblCreateFromFile(PRTACPITBL phAcpiTbl, const char *pszFilename, RTACPITBLTYPE enmInType, PRTERRINFO pErrInfo);


/**
 * Creates a new empty ACPI table.
 *
 * @returns IPRT status code.
 * @param   phAcpiTbl           Where to store the ACPI table handle on success.
 * @param   u32TblSig           The signature of the table to use.
 * @param   bRevision           The revision of the table.
 * @param   pszOemId            The OEM supplied string identifiying the OEM, maximum of 6 characters.
 * @param   pszOemTblId         The OEM supplied string identifiying the OEM table, maximum of 8 characters.
 * @param   u32OemRevision      The OEM supplied revision number.
 * @param   pszCreatorId        Vendor ID of the utility that created the table, maximum of 4 characters.
 * @param   u32CreatorRevision  Revision of the utility that created the table.
 */
RTDECL(int) RTAcpiTblCreate(PRTACPITBL phAcpiTbl, uint32_t u32TblSig, uint8_t bRevision, const char *pszOemId,
                            const char *pszOemTblId, uint32_t u32OemRevision, const char *pszCreatorId,
                            uint32_t u32CreatorRevision);


/**
 * Destroys the given ACPI table, freeing all resources.
 *
 * @param  hAcpiTbl             The ACPI table handle to destroy.
 */
RTDECL(void) RTAcpiTblDestroy(RTACPITBL hAcpiTbl);


/**
 * Finalizes the given ACPI table, setting the header and generating checksums.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle to finalize.
 *
 * @note Nothing can be added to the table after this was called.
 */
RTDECL(int) RTAcpiTblFinalize(RTACPITBL hAcpiTbl);


/**
 * Returns the size of the given ACPI table.
 *
 * @returns Size of the given ACPI table in bytes, 0 on error.
 * @param   hAcpiTbl            The ACPI table handle.
 *
 * @note This can only be called after RTAcpiTblFinalize() was called successfully.
 */
RTDECL(uint32_t) RTAcpiTblGetSize(RTACPITBL hAcpiTbl);


/**
 * Dumps the given ACPI table to the given VFS I/O stream.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   enmOutType          The output type.
 * @param   hVfsIos             The VFS I/O stream handle to dump the table to.
 */
RTDECL(int) RTAcpiTblDumpToVfsIoStrm(RTACPITBL hAcpiTbl, RTACPITBLTYPE enmOutType, RTVFSIOSTREAM hVfsIos);


/**
 * Dumps the given ACPI table to the given file.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   enmOutType          The output type.
 * @param   pszFilename         The file path to dump the table to.
 */
RTDECL(int) RTAcpiTblDumpToFile(RTACPITBL hAcpiTbl, RTACPITBLTYPE enmOutType, const char *pszFilename);


/**
 * Dumps the given ACPI table to a memory buffer allocated with RTMemAlloc() and returns the pointer
 * to the allocated memory.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   enmOutType          The output type.
 * @param   ppbAcpiTbl          Where to store the pointer to the ACPI table on success.
 * @param   pcbAcpiTbl          Where to store the size of the ACPI table in bytes on success.
 *
 * @note The caller has to free the buffer with RTMemFree().
 */
RTDECL(int) RTAcpiTblDumpToBufferA(RTACPITBL hAcpiTbl, RTACPITBLTYPE enmOutType, uint8_t **ppbAcpiTbl, size_t *pcbAcpiTbl);


/**
 * Starts a new DefScope object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             Name of the scope, can have a root (\) specifier optionally.
 */
RTDECL(int) RTAcpiTblScopeStart(RTACPITBL hAcpiTbl, const char *pszName);


/**
 * Finalizes the current scope object, nothing can be added to the scope afterwards.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblScopeFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new DefPackage object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   cElements           Number of element which will be inside the package,
 *                              only supports up to 255 elements, use DefVarPackage if more is required.
 */
RTDECL(int) RTAcpiTblPackageStart(RTACPITBL hAcpiTbl, uint8_t cElements);


/**
 * Finalizes the current DefPackage object, and return to the enclosing object's scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblPackageFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new device object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             Name of the device object, must be <= 4 characters long.
 */
RTDECL(int) RTAcpiTblDeviceStart(RTACPITBL hAcpiTbl, const char *pszName);


/**
 * Starts a new device object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszNameFmt          The name of the device as a format string.
 * @param   ...                 The format arguments.
 */
RTDECL(int) RTAcpiTblDeviceStartF(RTACPITBL hAcpiTbl, const char *pszNameFmt, ...) RT_IPRT_FORMAT_ATTR(2, 3);


/**
 * Starts a new device object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszNameFmt          The name of the device as a format string.
 * @param   va                  The format arguments.
 */
RTDECL(int) RTAcpiTblDeviceStartV(RTACPITBL hAcpiTbl, const char *pszNameFmt, va_list va) RT_IPRT_FORMAT_ATTR(2, 0);


/**
 * Finalizes the current scope object, nothing can be added to the scope afterwards.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblDeviceFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new processor object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             Name of the device object, must be <= 4 characters long.
 * @param   bProcId             The processor ID.
 * @param   u32PBlkAddr         Address of the processor register block.
 * @param   cbPBlk              Size of the processor register block in bytes.
 */
RTDECL(int) RTAcpiTblProcessorStart(RTACPITBL hAcpiTbl, const char *pszName, uint8_t bProcId, uint32_t u32PBlkAddr,
                                    uint8_t cbPBlk);


/**
 * Starts a new processor object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   bProcId             The processor ID.
 * @param   u32PBlkAddr         Address of the processor register block.
 * @param   cbPBlk              Size of the processor register block in bytes.
 * @param   pszNameFmt          The name of the device as a format string.
 * @param   ...                 The format arguments.
 */
RTDECL(int) RTAcpiTblProcessorStartF(RTACPITBL hAcpiTbl, uint8_t bProcId, uint32_t u32PBlkAddr, uint8_t cbPBlk,
                                     const char *pszNameFmt, ...) RT_IPRT_FORMAT_ATTR(5, 6);


/**
 * Starts a new processor object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   bProcId             The processor ID.
 * @param   u32PBlkAddr         Address of the processor register block.
 * @param   cbPBlk              Size of the processor register block in bytes.
 * @param   pszNameFmt          The name of the device as a format string.
 * @param   va                  The format arguments.
 */
RTDECL(int) RTAcpiTblProcessorStartV(RTACPITBL hAcpiTbl, uint8_t bProcId, uint32_t u32PBlkAddr, uint8_t cbPBlk,
                                     const char *pszNameFmt, va_list va) RT_IPRT_FORMAT_ATTR(5, 0);


/**
 * Finalizes the current scope object, nothing can be added to the scope afterwards.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblProcessorFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new buffer object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblBufferStart(RTACPITBL hAcpiTbl);


/**
 * Finalizes the current buffer object, nothing can be added to the scope afterwards.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblBufferFinalize(RTACPITBL hAcpiTbl);


/**
 * Appends data to the current raw buffer object (needs to be called between RTAcpiTblBufferStart() and RTAcpiTblBufferFinalize()).
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pvBuf               The data to append.
 * @param   cbBuf               Size of the buffer in bytes.
 */
RTDECL(int) RTAcpiTblBufferAppendRawData(RTACPITBL hAcpiTbl, const void *pvBuf, size_t cbBuf);


/**
 * Starts a new method object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             The method name.
 * @param   fFlags              AML method flags, see RTACPI_METHOD_F_XXX.
 * @param   cArgs               Number of arguments this method takes.
 * @param   uSyncLvl            The sync level.
 */
RTDECL(int) RTAcpiTblMethodStart(RTACPITBL hAcpiTbl, const char *pszName, uint8_t cArgs, uint32_t fFlags, uint8_t uSyncLvl);


/**
 * Starts a new method object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   fFlags              AML method flags, see RTACPI_METHOD_F_XXX.
 * @param   cArgs               Number of arguments this method takes.
 * @param   uSyncLvl            The sync level.
 * @param   pszNameFmt          The method name format string.
 * @param   ...                 Format string arguments.
 */
RTDECL(int) RTAcpiTblMethodStartF(RTACPITBL hAcpiTbl, uint8_t cArgs, uint32_t fFlags, uint8_t uSyncLvl,
                                  const char *pszNameFmt, ...) RT_IPRT_FORMAT_ATTR(5, 6);


/**
 * Starts a new method object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   fFlags              AML method flags, see RTACPI_METHOD_F_XXX.
 * @param   cArgs               Number of arguments this method takes.
 * @param   uSyncLvl            The sync level.
 * @param   pszNameFmt          The method name format string.
 * @param   va                  Format string arguments.
 */
RTDECL(int) RTAcpiTblMethodStartV(RTACPITBL hAcpiTbl, uint8_t cArgs, uint32_t fFlags, uint8_t uSyncLvl,
                                  const char *pszNameFmt, va_list va) RT_IPRT_FORMAT_ATTR(5, 0);


/** ACPI method is not serialized. */
#define RTACPI_METHOD_F_NOT_SERIALIZED 0
/** ACPI method call needs to be serialized in the ACPI interpreter. */
#define RTACPI_METHOD_F_SERIALIZED     RT_BIT_32(0)


/**
 * Finalizes the current method object, nothing can be added to the method afterwards.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblMethodFinalize(RTACPITBL hAcpiTbl);


/**
 * Appends a new DefName object (only the NameOp NameString part, DataRefObject is left for the caller
 * to append).
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             The name to append.
 */
RTDECL(int) RTAcpiTblNameAppend(RTACPITBL hAcpiTbl, const char *pszName);


/**
 * Appends a new NullName object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblNullNameAppend(RTACPITBL hAcpiTbl);


/**
 * Appends a new NameString object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             The name to append.
 */
RTDECL(int) RTAcpiTblNameStringAppend(RTACPITBL hAcpiTbl, const char *pszName);


/**
 * Appends a new String object - format string variant.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszNameFmt          The format string to build the name string from.
 * @param   ...                 Arguments for the format string.
 */
RTDECL(int) RTAcpiTblNameStringAppendF(RTACPITBL hAcpiTbl, const char *pszNameFmt, ...) RT_IPRT_FORMAT_ATTR(2, 3);


/**
 * Appends a new String object - format string variant.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszNameFmt          The format string to build the name string from.
 * @param   va                  The format arguments.
 */
RTDECL(int) RTAcpiTblNameStringAppendV(RTACPITBL hAcpiTbl, const char *pszNameFmt, va_list va) RT_IPRT_FORMAT_ATTR(2, 0);


/**
 * Appends a new String object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   psz                 The string to append.
 */
RTDECL(int) RTAcpiTblStringAppend(RTACPITBL hAcpiTbl, const char *psz);


/**
 * Appends a new String object - format string variant.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszFmt              The format string to build the string from.
 * @param   ...                 Arguments for the format string.
 */
RTDECL(int) RTAcpiTblStringAppendF(RTACPITBL hAcpiTbl, const char *pszFmt, ...) RT_IPRT_FORMAT_ATTR(2, 3);


/**
 * Appends a new String object - format string variant.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszFmt              The format string to build the string from.
 * @param   va                  The format arguments.
 */
RTDECL(int) RTAcpiTblStringAppendV(RTACPITBL hAcpiTbl, const char *pszFmt, va_list va) RT_IPRT_FORMAT_ATTR(2, 0);


/**
 * Appends a given UTF-8 string as UTF-16 using a buffer object (Unicode() equivalent).
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   psz                 The string to append.
 */
RTDECL(int) RTAcpiTblStringAppendAsUtf16(RTACPITBL hAcpiTbl, const char *psz);


/**
 * Appends a new integer object (depending on the value ZeroOp, OneOp,
 * BytePrefix, WordPrefix, DWordPrefix or QWordPrefix is used).
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   u64                 The 64-bit value to append.
 */
RTDECL(int) RTAcpiTblIntegerAppend(RTACPITBL hAcpiTbl, uint64_t u64);


/**
 * Appends a new DefBuffer object under the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pvBuf               The buffer data.
 * @param   cbBuf               Size of the buffer in bytes.
 */
RTDECL(int) RTAcpiTblBufferAppend(RTACPITBL hAcpiTbl, const void *pvBuf, size_t cbBuf);


/**
 * Appends the given resource as a DefBuffer under the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   hAcpiRes            The ACPI resource handle.
 */
RTDECL(int) RTAcpiTblResourceAppend(RTACPITBL hAcpiTbl, RTACPIRES hAcpiRes);


/**
 * List of statements.
 */
typedef enum RTACPISTMT
{
    /** Invalid statement. */
    kAcpiStmt_Invalid = 0,
    /** Ones statement. */
    kAcpiStmt_Ones,
    /** Return statement. */
    kAcpiStmt_Return,
    /** Breakpoint statement. */
    kAcpiStmt_Breakpoint,
    /** No operation statement. */
    kAcpiStmt_Nop,
    /** Break statement. */
    kAcpiStmt_Break,
    /** Continue statement. */
    kAcpiStmt_Continue,
    /** Add(Operand, Operand, Target) statement. */
    kAcpiStmt_Add,
    /** Subtract(Operand, Operand, Target) statement. */
    kAcpiStmt_Subtract,
    /** Multiply(Operand, Operand, Target) statement. */
    kAcpiStmt_Multiply,
    /** And(Operand, Operand, Target) statement. */
    kAcpiStmt_And,
    /** Nand(Operand, Operand, Target) statement. */
    kAcpiStmt_Nand,
    /** Or(Operand, Operand, Target) statement. */
    kAcpiStmt_Or,
    /** Xor(Operand, Operand, Target) statement. */
    kAcpiStmt_Xor,
    /** ShiftLeft(Operand, Operand, Target) statement. */
    kAcpiStmt_ShiftLeft,
    /** ShiftRight(Operand, Operand, Target) statement. */
    kAcpiStmt_ShiftRight,
    /** Not(Operand, Target) statement. */
    kAcpiStmt_Not,
    /** Store(TermArg, Supername) statement. */
    kAcpiStmt_Store,
    /** Index(BuffPkgStrObj, IndexValue, Target) statement. */
    kAcpiStmt_Index,
    /** DerefOf(ObjReference) statement. */
    kAcpiStmt_DerefOf,
    /** Store(SuperName, TermArg => Integer) statement. */
    kAcpiStmt_Notify,
    /** SizeOf(SuperName) statement. */
    kAcpiStmt_SizeOf,
    /** Increment(TermArg) statement. */
    kAcpiStmt_Increment,
    /** Decrement(TermArg) statement. */
    kAcpiStmt_Decrement,
    /** CondRefOf(TermArg, Target) statement. */
    kAcpiStmt_CondRefOf,
    /** LNot(Source) statement. */
    kAcpiStmt_LNot,
    /** CreateBitField(SourceBuff, BitIndex, NameString) statement. */
    kAcpiStmt_CreateBitField,
    /** CreateByteField(SourceBuff, ByteIndex, NameString) statement. */
    kAcpiStmt_CreateByteField,
    /** CreateWordField(SourceBuff, ByteIndex, NameString) statement. */
    kAcpiStmt_CreateWordField,
    /** CreateDWordField(SourceBuff, ByteIndex, NameString) statement. */
    kAcpiStmt_CreateDWordField,
    /** CreateQWordField(SourceBuff, ByteIndex, NameString) statement. */
    kAcpiStmt_CreateQWordField,
    /** ConcatenateResTemplate(Source1, Source2, Result) statement. */
    kAcpiStmt_ConcatenateResTemplate,
    /** FindSetLeftBit(Source, Result) statement. */
    kAcpiStmt_FindSetLeftBit,
    /** FindSetRightBit(Source, Result) statement. */
    kAcpiStmt_FindSetRightBit
} RTACPISTMT;


/**
 * Appends the given simple statement to the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   enmStmt             The statement to add.
 */
RTDECL(int) RTAcpiTblStmtSimpleAppend(RTACPITBL hAcpiTbl, RTACPISTMT enmStmt);


/**
 * Starts a new If statement operation.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblIfStart(RTACPITBL hAcpiTbl);


/**
 * Finalizes the current If statement operation.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblIfFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new Else operation (only valid if currently inside an If oepration).
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblElseStart(RTACPITBL hAcpiTbl);


/**
 * Finalizes the current Else statement operation.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblElseFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new While statement operation.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblWhileStart(RTACPITBL hAcpiTbl);


/**
 * Finalizes the current While statement operation.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblWhileFinalize(RTACPITBL hAcpiTbl);


/**
 * List of binary operations.
 */
typedef enum RTACPIBINARYOP
{
    /** Invalid binary operation. */
    kAcpiBinaryOp_Invalid = 0,
    /** LAnd(Operand, Operand). */
    kAcpiBinaryOp_LAnd,
    /** LOr(Operand, Operand). */
    kAcpiBinaryOp_LOr,
    /** LEqual(Operand, Operand). */
    kAcpiBinaryOp_LEqual,
    /** LGreater(Operand, Operand). */
    kAcpiBinaryOp_LGreater,
    /** LGreaterEqual(Operand, Operand). */
    kAcpiBinaryOp_LGreaterEqual,
    /** LLess(Operand, Operand). */
    kAcpiBinaryOp_LLess,
    /** LLessEqual(Operand, Operand). */
    kAcpiBinaryOp_LLessEqual,
    /** LNotEqual(Operand, Operand). */
    kAcpiBinaryOp_LNotEqual
} RTACPIBINARYOP;


/**
 * Appends the given binary operand.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   enmBinaryOp         The binary operation to append.
 */
RTDECL(int) RTAcpiTblBinaryOpAppend(RTACPITBL hAcpiTbl, RTACPIBINARYOP enmBinaryOp);


/**
 * Appends the given Arg<idArg> operand.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   idArg               The argument ID to append [0..6].
 */
RTDECL(int) RTAcpiTblArgOpAppend(RTACPITBL hAcpiTbl, uint8_t idArg);


/**
 * Appends the given Local<idLocal> operand.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   idLocal             The local ID to append [0..7].
 */
RTDECL(int) RTAcpiTblLocalOpAppend(RTACPITBL hAcpiTbl, uint8_t idLocal);


/**
 * Appends the given UUID as a buffer object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pUuid               The UUID to append.
 */
RTDECL(int) RTAcpiTblUuidAppend(RTACPITBL hAcpiTbl, PCRTUUID pUuid);


/**
 * Appends the given UUID string as a UUID buffer object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszUuid             The UUID string to append as a buffer.
 */
RTDECL(int) RTAcpiTblUuidAppendFromStr(RTACPITBL hAcpiTbl, const char *pszUuid);


/**
 * Appends the given 7 character EISA ID string as the corresponding 4-byte
 * integer.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszEisaId           The EISA ID to append.
 */
RTDECL(int) RTAcpiTblEisaIdAppend(RTACPITBL hAcpiTbl, const char *pszEisaId);


/**
 * Known operation region space types.
 */
typedef enum RTACPIOPREGIONSPACE
{
    /** Invalid region space type. */
    kAcpiOperationRegionSpace_Invalid = 0,
    /** Region is in system memory space. */
    kAcpiOperationRegionSpace_SystemMemory,
    /** Region is in system I/O space. */
    kAcpiOperationRegionSpace_SystemIo,
    /** Region is in PCI config space. */
    kAcpiOperationRegionSpace_PciConfig,
    /** Region is in embedded control space. */
    kAcpiOperationRegionSpace_EmbeddedControl,
    /** Region is in SMBUS space. */
    kAcpiOperationRegionSpace_SmBus,
    /** Region is in system CMOS space. */
    kAcpiOperationRegionSpace_SystemCmos,
    /** Region is a PCI bar target. */
    kAcpiOperationRegionSpace_PciBarTarget,
    /** Region is in IPMI space. */
    kAcpiOperationRegionSpace_Ipmi,
    /** Region is in GPIO space. */
    kAcpiOperationRegionSpace_Gpio,
    /** Region is in generic serial bus space. */
    kAcpiOperationRegionSpace_GenericSerialBus,
    /** Region is in platform communications channel (PCC) space. */
    kAcpiOperationRegionSpace_Pcc,
    /** 32bit hack. */
    kAcpiOperationRegionSpace_32bit_Hack = 0x7fffffff
} RTACPIOPREGIONSPACE;


/**
 * Appends a new OperationRegion() to the given ACPI table - extended version.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             The name of the operation region.
 * @param   enmSpace            The operation region space type.
 *
 * @note This doesn't encode the region offset and size arguments but leaves it up to the caller
 *       to be able to encode complex stuff.
 */
RTDECL(int) RTAcpiTblOpRegionAppendEx(RTACPITBL hAcpiTbl, const char *pszName, RTACPIOPREGIONSPACE enmSpace);


/**
 * Appends a new OperationRegion() to the given ACPI table.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             The name of the operation region.
 * @param   enmSpace            The operation region space type.
 * @param   offRegion           Offset of the region.
 * @param   cbRegion            Size of the region in bytes.
 */
RTDECL(int) RTAcpiTblOpRegionAppend(RTACPITBL hAcpiTbl, const char *pszName, RTACPIOPREGIONSPACE enmSpace,
                                    uint64_t offRegion, uint64_t cbRegion);


/**
 * Field access type.
 */
typedef enum RTACPIFIELDACC
{
    /** Invalid access type. */
    kAcpiFieldAcc_Invalid = 0,
    /** Any access width is okay. */
    kAcpiFieldAcc_Any,
    /** Byte (8-bit) access. */
    kAcpiFieldAcc_Byte,
    /** Word (16-bit) access. */
    kAcpiFieldAcc_Word,
    /** Double word (32-bit) access. */
    kAcpiFieldAcc_DWord,
    /** Quad word (64-bit) access. */
    kAcpiFieldAcc_QWord,
    /** Buffer like access. */
    kAcpiFieldAcc_Buffer
} RTACPIFIELDACC;


/**
 * Field update rule.
 */
typedef enum RTACPIFIELDUPDATE
{
    /** Invalid upadte rule. */
    kAcpiFieldUpdate_Invalid = 0,
    /** Preserve content not being accessed. */
    kAcpiFieldUpdate_Preserve,
    /** Write as ones. */
    kAcpiFieldUpdate_WriteAsOnes,
    /** Write as zeroes. */
    kAcpiFieldUpdate_WriteAsZeroes
} RTACPIFIELDUPDATE;


/**
 * Field entry.
 */
typedef struct RTACPIFIELDENTRY
{
    /** The field name - NULL means the NullName. */
    const char              *pszName;
    /** Number of bits of the field. */
    uint64_t                cBits;
} RTACPIFIELDENTRY;
/** Pointer to a field entry. */
typedef RTACPIFIELDENTRY *PRTACPIFIELDENTRY;
/** Pointer to a const field entry. */
typedef const RTACPIFIELDENTRY *PCRTACPIFIELDENTRY;


/**
 * Appends a new field descriptor to the given ACPI table.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszNameRef          The region/buffer the field describes.
 * @param   enmAcc              The access type,
 * @param   fLock               Flag whether access must happen under a lock.
 * @param   enmUpdate           The update rule.
 * @param   paFields            Pointer to the field descriptors.
 * @param   cFields             Number of entries in the array.
 */
RTDECL(int) RTAcpiTblFieldAppend(RTACPITBL hAcpiTbl, const char *pszNameRef, RTACPIFIELDACC enmAcc,
                                 bool fLock, RTACPIFIELDUPDATE enmUpdate, PCRTACPIFIELDENTRY paFields,
                                 uint32_t cFields);


/**
 * Appends a new index field descriptor to the given ACPI table.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszNameIndex        The index object.
 * @param   pszNameData         The data object.
 * @param   enmAcc              The access type,
 * @param   fLock               Flag whether access must happen under a lock.
 * @param   enmUpdate           The update rule.
 * @param   paFields            Pointer to the field descriptors.
 * @param   cFields             Number of entries in the array.
 */
RTDECL(int) RTAcpiTblIndexFieldAppend(RTACPITBL hAcpiTbl, const char *pszNameIndex, const char *pszNameData,
                                      RTACPIFIELDACC enmAcc, bool fLock, RTACPIFIELDUPDATE enmUpdate,
                                      PCRTACPIFIELDENTRY paFields, uint32_t cFields);


/**
 * Object type.
 */
typedef enum RTACPIOBJTYPE
{
    /** Invalid object type. */
    kAcpiObjType_Invalid = 0,
    /** Unknown object - UnknownObj */
    kAcpiObjType_Unknown,
    /** Integer object - IntObj */
    kAcpiObjType_Int,
    /** String object - StrObj */
    kAcpiObjType_Str,
    /** Buffer object - BuffObj */
    kAcpiObjType_Buff,
    /** Package object - PkgObj */
    kAcpiObjType_Pkg,
    /** Field unit object - FieldUnitObj */
    kAcpiObjType_FieldUnit,
    /** Device object - DeviceObj */
    kAcpiObjType_Device,
    /** Event object - EventObj */
    kAcpiObjType_Event,
    /** Method object - MethodObj */
    kAcpiObjType_Method,
    /** Mutex object - MutexObj */
    kAcpiObjType_MutexObj,
    /** OpRegion object - OpRegionObj */
    kAcpiObjType_OpRegion,
    /** Power resource object - PowerResObj */
    kAcpiObjType_PowerRes,
    /** Thermal zone object - ThermalZoneObj */
    kAcpiObjType_ThermalZone,
    /** Buffer field object - BuffFieldObj */
    kAcpiObjType_BuffField,
    /** Processor object - ProcessorObj */
    kAcpiObjType_Processor
} RTACPIOBJTYPE;


/**
 * Appends a new External declaration to the given ACPI table.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             The name stirng of the external object.
 * @param   enmObjType          The object type.
 * @param   cArgs               Number of arguments for the object (mostly method), valid is [0..7].
 */
RTDECL(int) RTAcpiTblExternalAppend(RTACPITBL hAcpiTbl, const char *pszName, RTACPIOBJTYPE enmObjType, uint8_t cArgs);


/** @name ACPI resource builder related API.
 * @{ */

/**
 * Creates a new empty resource template.
 *
 * @returns IPRT status code.
 * @param   phAcpiRes           Where to store the handle to the ACPI resource on success.
 */
RTDECL(int) RTAcpiResourceCreate(PRTACPIRES phAcpiRes);


/**
 * Destroys the given ACPI resource, freeing all allocated resources.
 *
 * @param   hAcpiRes            The ACPI resource handle to destroy.
 */
RTDECL(void) RTAcpiResourceDestroy(RTACPIRES hAcpiRes);


/**
 * Resets the given ACPI resource handle to create a new empty template.
 *
 * @param   hAcpiRes            The ACPI resource handle.
 */
RTDECL(void) RTAcpiResourceReset(RTACPIRES hAcpiRes);


/**
 * Returns the offset where the next resource added to the template would be.
 *
 * @returns Offset into the resource buffer where the next resource will be appended
 * @retval  UINT32_MAX if the handle is invalid or the resource is in an error state.
 * @param   hAcpiRes            The ACPI resource handle.
 */
RTDECL(uint32_t) RTAcpiResourceGetOffset(RTACPIRES hAcpiRes);


/**
 * Seals the given ACPI resource against further changes and adds any
 * missing data required to complete the resource buffer.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 *
 * @note After a call to this method completed successfully it is not possible
 *       to add new resources until RTAcpiResourceReset() was called.
 */
RTDECL(int) RTAcpiResourceSeal(RTACPIRES hAcpiRes);


/**
 * Queries the pointer to the buffer holding the encoded data.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   ppvRes              Where to store the pointer to the buffer holding the encoded resource template on success.
 * @param   pcbRes              Where to store the size of the encoded data in bytes on success.
 *
 * @note The ACPI resource must be successfully sealed with RTAcpiResourceSeal() for this function to succeed.
 *       Also the buffer pointer will only be valid until a call to any other RTAcpiResource* method.
 */
RTDECL(int) RTAcpiResourceQueryBuffer(RTACPIRES hAcpiRes, const void **ppvRes, size_t *pcbRes);


/**
 * Adds a fixed memory range with the given start address and size to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   u32AddrBase         The base address to encode.
 * @param   cbRange             The range length in bytes to encode.
 * @param   fRw                 Flag whether this address range is read-write or read-only.
 */
RTDECL(int) RTAcpiResourceAdd32BitFixedMemoryRange(RTACPIRES hAcpiRes, uint32_t u32AddrBase, uint32_t cbRange,
                                                   bool fRw);


/**
 * Adds an extended interrupt descriptor with the given configuration to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   fConsumer           Flag whether the entity this resource is assigned to consumes the interrupt (true) or produces it (false).
 * @param   fEdgeTriggered      Flag whether the interrupt is edged (true) or level (false) triggered.
 * @param   fActiveLow          Flag whether the interrupt polarity is active low (true) or active high (false).
 * @param   fShared             Flag whether the interrupt is shared between different entities (true) or exclusive to the assigned entity (false).
 * @param   fWakeCapable        Flag whether the interrupt can wake the system (true) or not (false).
 * @param   cIntrs              Number of interrupts following.
 * @param   pau32Intrs          Pointer to the array of interrupt numbers.
 */
RTDECL(int) RTAcpiResourceAddExtendedInterrupt(RTACPIRES hAcpiRes, bool fConsumer, bool fEdgeTriggered, bool fActiveLow, bool fShared,
                                               bool fWakeCapable, uint8_t cIntrs, uint32_t *pau32Intrs);


/** @name Generic address space flags.
 * @{ */
#define RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_SUB        RT_BIT_32(0)
#define RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_POS        0

#define RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_FIXED         RT_BIT_32(1)
#define RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_CHANGEABLE    0

#define RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_FIXED         RT_BIT_32(2)
#define RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_CHANGEABLE    0

#define RTACPI_RESOURCE_ADDR_RANGE_F_PRODUCER               RT_BIT_32(3)
#define RTACPI_RESOURCE_ADDR_RANGE_F_CONSUMER               0

#define RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK             UINT32_C(0x0000000f)
/** @} */

/**
 * Memory range cacheability
 */
typedef enum RTACPIRESMEMRANGECACHEABILITY
{
    /** Usual invalid value. */
    kAcpiResMemRangeCacheability_Invalid = 0,
    /** Memory range is non cacheable (like MMIO, etc.). */
    kAcpiResMemRangeCacheability_NonCacheable,
    /** Memory is cacheable. */
    kAcpiResMemRangeCacheability_Cacheable,
    /** Memory is cacheable and supports write combining. */
    kAcpiResMemRangeCacheability_CacheableWriteCombining,
    /** Memory is cacheable and supports prefetching. */
    kAcpiResMemRangeCacheability_CacheablePrefetchable,
    /** 32-bit blow up hack. */
    kAcpiResMemRangeCacheability_32BitHack = 0x7fffffff
} RTACPIRESMEMRANGECACHEABILITY;


/**
 * Memory attribute.
 */
typedef enum RTACPIRESMEMRANGETYPE
{
    /** Invalid memory range type. */
    kAcpiResMemType_Invalid = 0,
    /** Memory range is actual memory. */
    kAcpiResMemType_Memory,
    /** Memory range is reserved. */
    kAcpiResMemType_Reserved,
    /** Memory range is reserved to ACPI. */
    kAcpiResMemType_Acpi,
    /** Memory range is no volatile storage. */
    kAcpiResMemType_Nvs,
    /** 32-bit blow up hack. */
    kAcpiResMemType_32BitHack = 0x7fffffff
} RTACPIRESMEMRANGETYPE;


/**
 * Adds a quad word (64-bit) memory range to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmCacheability     The cacheability of the memory range.
 * @param   enmType             Memory range type.
 * @param   fRw                 Flag whether the memory range is read/write (true) or readonly (false).
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u64AddrMin          The start address of the memory range.
 * @param   u64AddrMax          Last valid address of the range.
 * @param   u64OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u64Granularity      The access granularity of the range in bytes.
 * @param   u64Length           Length of the memory range in bytes.
 */
RTDECL(int) RTAcpiResourceAddQWordMemoryRange(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                              RTACPIRESMEMRANGETYPE enmType, bool fRw, uint32_t fAddrSpace,
                                              uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                              uint64_t u64Granularity, uint64_t u64Length);


/**
 * Adds a quad word (64-bit) memory range to the given ACPI resource - extended version.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmCacheability     The cacheability of the memory range.
 * @param   enmType             Memory range type.
 * @param   fRw                 Flag whether the memory range is read/write (true) or readonly (false).
 * @param   fStatic             Flag whether the translation type is static (true) or translation (false).
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u64AddrMin          The start address of the memory range.
 * @param   u64AddrMax          Last valid address of the range.
 * @param   u64OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u64Granularity      The access granularity of the range in bytes.
 * @param   u64Length           Length of the memory range in bytes.
 * @param   pszRsrcSrc          Name of the device object that produces the the descriptor consumed by the device, optional.
 *                              NULL means the device consumes the resource out of a global pool.
 * @param   bRsrcIndex          Index into the resource descriptor this device consumes from. Ignored if pszRsrcSrc is NULL.
 */
RTDECL(int) RTAcpiResourceAddQWordMemoryRangeEx(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                                RTACPIRESMEMRANGETYPE enmType, bool fRw, bool fStatic, uint32_t fAddrSpace,
                                                uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                                uint64_t u64Granularity, uint64_t u64Length,
                                                const char *pszRsrcSrc, uint8_t bRsrcIndex);


/**
 * Adds a double word (32-bit) memory range to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmCacheability     The cacheability of the memory range.
 * @param   enmType             Memory range type.
 * @param   fRw                 Flag whether the memory range is read/write (true) or readonly (false).
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u32AddrMin          The start address of the memory range.
 * @param   u32AddrMax          Last valid address of the range.
 * @param   u32OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u32Granularity      The access granularity of the range in bytes.
 * @param   u32Length           Length of the memory range in bytes.
 */
RTDECL(int) RTAcpiResourceAddDWordMemoryRange(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                              RTACPIRESMEMRANGETYPE enmType, bool fRw, uint32_t fAddrSpace,
                                              uint32_t u32AddrMin, uint32_t u32AddrMax, uint32_t u32OffTrans,
                                              uint32_t u32Granularity, uint32_t u32Length);


/**
 * Adds a double word (32-bit) memory range to the given ACPI resource - extended version.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmCacheability     The cacheability of the memory range.
 * @param   enmType             Memory range type.
 * @param   fRw                 Flag whether the memory range is read/write (true) or readonly (false).
 * @param   fStatic             Flag whether the translation type is static (true) or translation (false).
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u32AddrMin          The start address of the memory range.
 * @param   u32AddrMax          Last valid address of the range.
 * @param   u32OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u32Granularity      The access granularity of the range in bytes.
 * @param   u32Length           Length of the memory range in bytes.
 * @param   pszRsrcSrc          Name of the device object that produces the the descriptor consumed by the device, optional.
 *                              NULL means the device consumes the resource out of a global pool.
 * @param   bRsrcIndex          Index into the resource descriptor this device consumes from. Ignored if pszRsrcSrc is NULL.
 */
RTDECL(int) RTAcpiResourceAddDWordMemoryRangeEx(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                                RTACPIRESMEMRANGETYPE enmType, bool fRw, bool fStatic, uint32_t fAddrSpace,
                                                uint32_t u32AddrMin, uint32_t u32AddrMax, uint32_t u32OffTrans,
                                                uint32_t u32Granularity, uint32_t u32Length,
                                                const char *pszRsrcSrc, uint8_t bRsrcIndex);


/**
 * I/O range coverage.
 */
typedef enum RTACPIRESIORANGE
{
    /** Invalid range. */
    kAcpiResIoRange_Invalid = 0,
    /** Range covers only non ISA I/O ports. */
    kAcpiResIoRange_NonIsaOnly,
    /** Range covers only ISA I/O ports. */
    kAcpiResIoRange_IsaOnly,
    /** Range covers the whole I/O port range. */
    kAcpiResIoRange_Whole,
    /** 32-bit blow up hack. */
    kAcpiResIoRange_32BitHack = 0x7fffffff
} RTACPIRESIORANGE;


/**
 * I/O range type.
 */
typedef enum RTACPIRESIORANGETYPE
{
    /** Invalid value. */
    kAcpiResIoRangeType_Invalid = 0,
    /** Resource is I/O on the primary and secondary side of the bridge. */
    kAcpiResIoRangeType_Static,
    /** Resource is memory on the primary and I/O on the secondary side of the bridge,
     * primary side memory address for a given I/O port is calculated with
     * address = (((Port & 0xfffc) << 10) || (Port & 0xfff)) + AddrMin. */
    kAcpiResIoRangeType_Translation_Sparse,
    /** Resource is memory on the primary and I/O on the secondary side of the bridge,
     * primary side memory address for a given I/O port is calculated with
     * address = AddrMin + Port. */
    kAcpiResIoRangeType_Translation_Dense,
    /** 32-bit blowup hack. */
    kAcpiResIoRangeType_32BitHack = 0x7fffffff
} RTACPIRESIORANGETYPE;


/**
 * Adds a quad word (64-bit) I/O range to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmIoType           The I/O range type.
 * @param   enmIoRange          The I/O range coverage.
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u64AddrMin          The start address of the memory range.
 * @param   u64AddrMax          Last valid address of the range.
 * @param   u64OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u64Granularity      The access granularity of the range in bytes.
 * @param   u64Length           Length of the memory range in bytes.
 */
RTDECL(int) RTAcpiResourceAddQWordIoRange(RTACPIRES hAcpiRes, RTACPIRESIORANGETYPE enmIoType, RTACPIRESIORANGE enmIoRange,
                                          uint32_t fAddrSpace, uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                          uint64_t u64Granularity, uint64_t u64Length);


/**
 * Adds a word (16-bit) I/O range to the given ACPI resource - extended version.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmIoType           The I/O range type.
 * @param   enmIoRange          The I/O range coverage.
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u16AddrMin          The start address of the memory range.
 * @param   u16AddrMax          Last valid address of the range.
 * @param   u16OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u16Granularity      The access granularity of the range in bytes.
 * @param   u16Length           Length of the memory range in bytes.
 * @param   pszRsrcSrc          Name of the device object that produces the the descriptor consumed by the device, optional.
 *                              NULL means the device consumes the resource out of a global pool.
 * @param   bRsrcIndex          Index into the resource descriptor this device consumes from. Ignored if pszRsrcSrc is NULL.
 */
RTDECL(int) RTAcpiResourceAddWordIoRangeEx(RTACPIRES hAcpiRes, RTACPIRESIORANGETYPE enmIoType, RTACPIRESIORANGE enmIoRange,
                                           uint32_t fAddrSpace, uint16_t u16AddrMin, uint16_t u16AddrMax, uint64_t u16OffTrans,
                                           uint16_t u16Granularity, uint16_t u16Length, const char *pszRsrcSrc, uint8_t bRsrcIndex);


/**
 * Adds a word (16-bit) bus number to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u16BusMin           Starting bus number.
 * @param   u16BusMax           Last valid bus number.
 * @param   u16OffTrans         Translation offset being applied to the bus number.
 * @param   u16Granularity      The access granularity of the bus number.
 * @param   u16Length           Length of the bus range.
 */
RTDECL(int) RTAcpiResourceAddWordBusNumber(RTACPIRES hAcpiRes, uint32_t fAddrSpace, uint16_t u16BusMin, uint16_t u16BusMax,
                                           uint16_t u16OffTrans, uint16_t u16Granularity, uint16_t u16Length);


/**
 * Adds a word (16-bit) bus number to the given ACPI resource - extended version.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u16BusMin           Starting bus number.
 * @param   u16BusMax           Last valid bus number.
 * @param   u16OffTrans         Translation offset being applied to the bus number.
 * @param   u16Granularity      The access granularity of the bus number.
 * @param   u16Length           Length of the bus range.
 * @param   pszRsrcSrc          Name of the device object that produces the the descriptor consumed by the device, optional.
 *                              NULL means the device consumes the resource out of a global pool.
 * @param   bRsrcIndex          Index into the resource descriptor this device consumes from. Ignored if pszRsrcSrc is NULL.
 */
RTDECL(int) RTAcpiResourceAddWordBusNumberEx(RTACPIRES hAcpiRes, uint32_t fAddrSpace, uint16_t u16BusMin, uint16_t u16BusMax,
                                             uint16_t u16OffTrans, uint16_t u16Granularity, uint16_t u16Length,
                                             const char *pszRsrcSrc, uint8_t bRsrcIndex);


/**
 * I/O decode type.
 */
typedef enum RTACPIRESIODECODETYPE
{
    /** Invalid value. */
    kAcpiResIoDecodeType_Invalid = 0,
    /** 10-bit decoding. */
    kAcpiResIoDecodeType_Decode10,
    /** 16-bit decoding. */
    kAcpiResIoDecodeType_Decode16,
    /** 32-bit blowup hack. */
    kAcpiResIoDecodeType_32BitHack = 0x7fffffff
} RTACPIRESIODECODETYPE;


/**
 * Adds an I/O port descriptor to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmDecode           The decoding type of the range.
 * @param   u16AddrMin          Minimum base I/O address the range might be configured for.
 * @param   u16AddrMax          Maximum base I/O address the range might be configured for.
 * @param   u8AddrAlignment     Alignment of the minimum base address.
 * @param   u8RangeLength       Number of contiguous I/O ports requested.
 */
RTDECL(int) RTAcpiResourceAddIo(RTACPIRES hAcpiRes, RTACPIRESIODECODETYPE enmDecode, uint16_t u16AddrMin, uint16_t u16AddrMax,
                                uint8_t u8AddrAlignment, uint8_t u8RangeLength);


/**
 * Adds an IRQ descriptor with the given configuration to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   fEdgeTriggered      Flag whether the interrupt is edged (true) or level (false) triggered.
 * @param   fActiveLow          Flag whether the interrupt polarity is active low (true) or active high (false).
 * @param   fShared             Flag whether the interrupt is shared between different entities (true) or exclusive to the assigned entity (false).
 * @param   fWakeCapable        Flag whether the interrupt can wake the system (true) or not (false).
 * @param   bmIntrs             Bitmap of interrupts (0..15) requested.
 */
RTDECL(int) RTAcpiResourceAddIrq(RTACPIRES hAcpiRes, bool fEdgeTriggered, bool fActiveLow, bool fShared,
                                 bool fWakeCapable, uint16_t bmIntrs);


/**
 * DMA channel speed.
 */
typedef enum RTACPIRESDMACHANSPEED
{
    /** Invalid value. */
    kAcpiResDmaChanSpeed_Invalid = 0,
    /** Compatibility mode. */
    kAcpiResDmaChanSpeed_Compatibility,
    /** Type A DMA as described in EISA. */
    kAcpiResDmaChanSpeed_TypeA,
    /** Type B DMA. */
    kAcpiResDmaChanSpeed_TypeB,
    /** Type F. */
    kAcpiResDmaChanSpeed_TypeF,
    /** 32-bit blowup hack. */
    kAcpiResDmaChanSpeed_32BitHack = 0x7fffffff
} RTACPIRESDMACHANSPEED;


/**
 * DMA transfer type.
 */
typedef enum RTACPIRESDMATRANSFERTYPE
{
    /** Invalid value. */
    kAcpiResDmaTransferType_Invalid = 0,
    /** 8bit only. */
    kAcpiResDmaTransferType_8Bit,
    /** 8-bit and 16-bit. */
    kAcpiResDmaTransferType_8Bit_16Bit,
    /** 16-bit only. */
    kAcpiResDmaTransferType_16Bit,
    /** 32-bit blowup hack. */
    kAcpiResDmaTransferType_32BitHack = 0x7fffffff
} RTACPIRESDMATRANSFERTYPE;


/**
 * Adds a DMA descriptor with the given configuration to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmChanSpeed        The DMA channel speed.
 * @param   fBusMaster          Flag whether the device is a bus master.
 * @param   enmTransferType     DMA transfer type preference.
 * @param   bmChannels          Bitmap of DMA channels (0..7) requested.
 */
RTDECL(int) RTAcpiResourceAddDma(RTACPIRES hAcpiRes, RTACPIRESDMACHANSPEED enmChanSpeed, bool fBusMaster,
                                 RTACPIRESDMATRANSFERTYPE enmTransferType, uint8_t bmChannels);


/**
 * GPIO Interrupt type.
 */
typedef enum RTACPIRESGPIOMOD
{
    /** Invalid type. */
    kAcpiResGpioMod_Invalid = 0,
    /** Edge interrupt type. */
    kAcpiResGpioMod_Edge,
    /** Level interrupt type. */
    kAcpiResGpioMod_Level,
    /** 32-bit blowup hack. */
    kAcpiResGpioMod_32Bit_Hack = 0x7fffffff
} RTACPIRESGPIOMOD;


/**
 * GPIO polarity config.
 */
typedef enum RTACPIRESGPIOPOL
{
    /** Invalid type. */
    kAcpiResGpioPol_Invalid = 0,
    /** Active if input is high. */
    kAcpiResGpioPol_ActiveHigh,
    /** Active if input is low. */
    kAcpiResGpioPol_ActiveLow,
    /** Active on both (only supported if interrupt type is Edge). */
    kAcpiResGpioPol_ActiveBoth,
    /** 32-bit blowup hack. */
    kAcpiResGpioPol_32Bit_Hack = 0x7fffffff
} RTACPIRESGPIOPOL;


/**
 * GPIO shared/exclusive config.
 */
typedef enum RTACPIRESGPIOSHR
{
    /** Invalid type. */
    kAcpiResGpioShr_Invalid = 0,
    /** GPIO pins are shared. */
    kAcpiResGpioShr_Shared,
    /** GPIO pins are exclusive. */
    kAcpiResGpioShr_Exclusive,
    /** GPIO pins are shared and capabale to wake the system. */
    kAcpiResGpioShr_SharedAndWake,
    /** GPIO pins are exclusive and capabale to wake the system. */
    kAcpiResGpioShr_ExclusiveAndWake,
    /** 32-bit blowup hack. */
    kAcpiResGpioShr_32Bit_Hack = 0x7fffffff
} RTACPIRESGPIOSHR;


/**
 * GPIO pin config.
 */
typedef enum RTACPIRESGPIOPPI
{
    /** Invalid config. */
    kAcpiResGpioPpi_Invalid = 0,
    /** Default pull up/down config. */
    kAcpiResGpioPpi_PullDefault,
    /** Pin is pulled up. */
    kAcpiResGpioPpi_PullUp,
    /** Pin is pulled down. */
    kAcpiResGpioPpi_PullDown,
    /** Pin has no pull up/down resistor attached and is floating. */
    kAcpiResGpioPpi_PullNone,
    /** 32-bit blowup hack. */
    kAcpiResGpioPpi_32Bit_Hack = 0x7fffffff
} RTACPIRESGPIOPPI;


/**
 * Adds a GPIO connection descriptor suitable for GPIO input pins with interrupt support
 * with the given configuration to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmMod              Interrupt configuration.
 * @param   enmPol              Polarity config.
 * @param   enmShr              Shared/Exclusive config.
 * @param   enmPpi              The pin pull up/down config.
 * @param   u16DebounceWait     Debounce wait time in 100ms steps.
 * @param   pszRsrcSrc          The GPIO controller resource name these GPIO pins are part of.
 * @param   pau16Pins           Array of pin numbers.
 * @param   cPins               Number of entries in the array.
 */
RTDECL(int) RTAcpiResourceAddGpioInt(RTACPIRES hAcpiRes, RTACPIRESGPIOMOD enmMod, RTACPIRESGPIOPOL enmPol, RTACPIRESGPIOSHR enmShr,
                                     RTACPIRESGPIOPPI enmPpi, uint16_t u16DebounceWait, const char *pszRsrcSrc,
                                     uint16_t *pau16Pins, uint16_t cPins);

/** @} */

#endif /* IN_RING3 */

/** @} */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_acpi_h */

