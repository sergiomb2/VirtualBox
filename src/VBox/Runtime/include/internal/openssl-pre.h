/* $Id$ */
/** @file
 * IPRT - Internal header to be included before a block of openssl includes.
 */

/*
 * Copyright (C) 2020-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/* No guard or pragma once! */

#if defined(_MSC_VER) && defined(__cplusplus)
# pragma warning(push)
# pragma warning(disable:4668) /* openssl-1.1.0e-x86\include\openssl/opensslconf.h(104) : warning C4668: '__GNUC__' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' */
# if _MSC_VER >= 1700 /*RT_MSC_VER_VC120*/ /** @todo check this. 1600 (VS2010) doesn't know it and warns. */
# pragma warning(disable:5031) /* warning C5031: #pragma warning(pop): likely mismatch, popping warning state pushed in different file */
# endif
# if _MSC_VER >= 1900 /*RT_MSC_VER_VC140*/
#  pragma warning(disable:5039) /* Passing callbacks that may throw C++ exception to nothrowing extern "C" functions. */
# endif
#endif

