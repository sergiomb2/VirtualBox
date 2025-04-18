/** @file
 * IPRT - ARM Specific Assembly Macros.
 */

/*
 * Copyright (C) 2023-2024 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_asmdefs_arm_h
#define IPRT_INCLUDED_asmdefs_arm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#ifdef ASM_FORMAT_PE
# include <iprt/formats/pecoff.h>
#endif

/* Quick hack for #defines from pecoff.h */
#define UINT8_C(v)      v
#define UINT16_C(v)     v
#define UINT32_C(v)     v
#define UINT64_C(v)     v
#define INT8_C(v)       v
#define INT16_C(v)      v
#define INT32_C(v)      v
#define INT64_C(v)      v


#if !defined(RT_ARCH_ARM64) && !defined(RT_ARCH_ARM32)
# error "Not on ARM64 or ARM32"
#endif

/** @defgroup grp_rt_asmdefs_arm  ARM Specific ASM (Clang and gcc) Macros
 * @ingroup grp_rt_asm
 * @{
 */

/**
 * Align code, pad with BRK. */
#define ALIGNCODE(alignment)    .balignl alignment, 0xd4201980

/**
 * Align data, pad with ZEROs. */
#define ALIGNDATA(alignment)    .balign alignment

/**
 * Align BSS, pad with ZEROs. */
#define ALIGNBSS(alignment)     .balign alignment


/** Marks the beginning of a code section. */
#ifdef ASM_FORMAT_MACHO
# define BEGINCODE .section __TEXT,__text,regular,pure_instructions
#elif defined(ASM_FORMAT_ELF) || defined(ASM_FORMAT_PE)
# define BEGINCODE .section .text
#else
# error "Port me!"
#endif

/** Marks the end of a code section. */
#ifdef ASM_FORMAT_MACHO
# define ENDCODE
#elif defined(ASM_FORMAT_ELF) || defined(ASM_FORMAT_PE)
# define ENDCODE
#else
# error "Port me!"
#endif


/** Marks the beginning of a data section. */
#ifdef ASM_FORMAT_MACHO
# define BEGINDATA .section __DATA,__data
#elif defined(ASM_FORMAT_ELF) || defined(ASM_FORMAT_PE)
# define BEGINDATA .section .data
#else
# error "Port me!"
#endif

/** Marks the end of a data section. */
#ifdef ASM_FORMAT_MACHO
# define ENDDATA
#elif defined(ASM_FORMAT_ELF) || defined(ASM_FORMAT_PE)
# define ENDDATA
#else
# error "Port me!"
#endif


/** Marks the beginning of a readonly data section. */
#ifdef ASM_FORMAT_MACHO
# define BEGINCONST .section __RODATA,__rodata
#elif defined(ASM_FORMAT_ELF) || defined(ASM_FORMAT_PE)
# define BEGINCONST .section .rodata
#else
# error "Port me!"
#endif

/** Marks the end of a readonly data section. */
#ifdef ASM_FORMAT_MACHO
# define ENDCONST
#elif defined(ASM_FORMAT_ELF) || defined(ASM_FORMAT_PE)
# define ENDCONST
#else
# error "Port me!"
#endif


/** Marks the beginning of a readonly C strings section. */
#ifdef ASM_FORMAT_MACHO
# define BEGINCONSTSTRINGS .section __TEXT,__cstring,cstring_literals
#elif defined(ASM_FORMAT_ELF) || defined(ASM_FORMAT_PE)
# define BEGINCONSTSTRINGS .section .rodata
#else
# error "Port me!"
#endif

/** Marks the end of a readonly C strings section. */
#ifdef ASM_FORMAT_MACHO
# define ENDCONSTSTRINGS
#elif defined(ASM_FORMAT_ELF) || defined(ASM_FORMAT_PE)
# define ENDCONSTSTRINGS
#else
# error "Port me!"
#endif


/**
 * Mangles the name so it can be referenced using DECLASM() in the C/C++ world.
 *
 * @returns a_SymbolC with the necessary prefix/postfix.
 * @param   a_SymbolC   A C symbol name to mangle as needed.
 */
#if defined(RT_OS_DARWIN)
# define NAME(a_SymbolC)    _ ## a_SymbolC
#else
# define NAME(a_SymbolC)    a_SymbolC
#endif


/**
 * Returns the page address of the given symbol (used with the adrp instruction primarily).
 *
 * @returns Page aligned address of the given symbol
 * @param   a_Symbol    The symbol to get the page address from.
 */
#if defined(ASM_FORMAT_MACHO) || defined(ASM_FORMAT_PE)
# define PAGE(a_Symbol) a_Symbol ## @PAGE
#elif defined(ASM_FORMAT_ELF)
# define PAGE(a_Symbol) a_Symbol
# define PAGE_GOT(a_Symbol) :got: ## a_Symbol
#else
# error "Port me!"
#endif

/**
 * Returns the offset inside the page of the given symbol.
 *
 * @returns Page offset of the given symbol inside a page.
 * @param   a_Symbol    The symbol to get the page offset from.
 */
#if defined(ASM_FORMAT_MACHO) || defined(ASM_FORMAT_PE)
# define PAGEOFF(a_Symbol) a_Symbol ## @PAGEOFF
#elif defined(ASM_FORMAT_ELF)
# define PAGEOFF(a_Symbol) :lo12: ## a_Symbol
# define PAGEOFF_GOT(a_Symbol) :got_lo12: ## a_Symbol
#else
# error "Port me!"
#endif


/**
 * Starts an externally visible procedure.
 *
 * @param   a_Name      The unmangled symbol name.
 */
.macro BEGINPROC, a_Name
#if defined(ASM_FORMAT_PE)
        .def            NAME(\a_Name)
        .scl            IMAGE_SYM_CLASS_EXTERNAL
        .type           IMAGE_SYM_DTYPE_FUNCTION << N_BTSHFT
        .endef
#endif
        .globl          NAME(\a_Name)
NAME(\a_Name):
.endm


/**
 * Starts a procedure with hidden visibility.
 *
 * @param   a_Name      The unmangled symbol name.
 */
.macro BEGINPROC_HIDDEN, a_Name
#ifdef ASM_FORMAT_MACHO
        .private_extern NAME(\a_Name)
#elif defined(ASM_FORMAT_ELF)
        .hidden         NAME(\a_Name)
#elif defined(ASM_FORMAT_PE)
        .def            NAME(\a_Name)
        .scl            IMAGE_SYM_CLASS_EXTERNAL
        .type           IMAGE_SYM_DTYPE_FUNCTION << N_BTSHFT
        .endef
#endif
        .globl          NAME(\a_Name)
NAME(\a_Name):
.endm


/**
 * Starts an exported procedure.
 *
 * @param   a_Name      The unmangled symbol name.
 */
.macro BEGINPROC_EXPORTED, a_Name
#ifdef ASM_FORMAT_MACHO
        //.private_extern NAME(\a_Name)
#elif defined(ASM_FORMAT_ELF)
        //.hidden         NAME(\a_Name)
#elif defined(ASM_FORMAT_PE)
        .pushsection    .drectve
        .string "-export:\a_Name"
        .popsection
        .def            NAME(\a_Name)
        .scl            IMAGE_SYM_CLASS_EXTERNAL
        .type           IMAGE_SYM_DTYPE_FUNCTION << N_BTSHFT
        .endef
#endif
        .globl          NAME(\a_Name)
NAME(\a_Name):
.endm


/**
 * Ends a procedure.
 *
 * @param   a_Name      The unmangled symbol name.
 */
.macro ENDPROC, a_Name
NAME(\a_Name)\()_EndProc:
.endm


/** @} */

#endif /* !IPRT_INCLUDED_asmdefs_arm_h */

