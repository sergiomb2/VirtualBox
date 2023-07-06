/** @file
 * IPRT - PKCS \#8, Private-Key Information Syntax Standard.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_crypto_pkcs8_h
#define IPRT_INCLUDED_crypto_pkcs8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/asn1.h>
#include <iprt/crypto/x509.h>
#include <iprt/crypto/pkcs7.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_crpkcs8 RTCrPkcs8 - PKCS \#8, Private-Key Information Syntax Standard
 * @ingroup grp_rt_crypto
 * @{
 */

/**
 * PKCS\#8 PrivateKeyInfo.
 */
/** @todo bird: rename to RTCRPKCS8PRIVATEKEYINFO. Should eventually be moved
 *        into a new iprt/crypto/pkcs8.h header file. Ditto for the template
 *        and associated code instantiation/whatever. */
typedef struct RTCRPKCS8PRIVATEKEYINFO
{
    /** Sequence core for the structure. */
    RTASN1SEQUENCECORE          SeqCore;
    /** Key version number. */
    RTASN1INTEGER               Version;
    /** The private key algorithm. */
    RTCRX509ALGORITHMIDENTIFIER PrivateKeyAlgorithm;
    /** The private key. */
    RTASN1OCTETSTRING           PrivateKey;
    /** Attributes, optional [0].
     * @todo check this one. */
    RTCRPKCS7ATTRIBUTES         Attributes;
} RTCRPKCS8PRIVATEKEYINFO;
/** Pointer to the ASN.1 IPRT representation of a PKCS8 private key. */
typedef RTCRPKCS8PRIVATEKEYINFO *PRTCRPKCS8PRIVATEKEYINFO;
/** Pointer to the const ASN.1 IPRT representation of a PKCS8 private key. */
typedef RTCRPKCS8PRIVATEKEYINFO const *PCRTCRPKCS8PRIVATEKEYINFO;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRPKCS8PRIVATEKEYINFO, RTDECL, RTCrPkcs8PrivateKeyInfo, SeqCore.Asn1Core);

#if 0

/**
 * PKCS\#8 EncryptedPrivateKeyInfo.
 */
typedef struct RTCRENCRYPTEDPRIVATEKEY
{
    /** Sequence core for the structure. */
    RTASN1SEQUENCECORE          SeqCore;
    /** The encryption algorithm. */
    RTCRX509ALGORITHMIDENTIFIER EncryptionAlgorithm;
    /** The encrypted data. */
    RTASN1OCTETSTRING           EncryptedData;
} RTCRENCRYPTEDPRIVATEKEY;
/** Pointer to the ASN.1 IPRT representation of a PKCS8 encrypted private key. */
typedef RTCRENCRYPTEDPRIVATEKEY *PRTCRENCRYPTEDPRIVATEKEY;
/** Pointer to the const ASN.1 IPRT representation of a PKCS8 encrypted private key. */
typedef RTCRENCRYPTEDPRIVATEKEY const *PCRTCRENCRYPTEDPRIVATEKEY;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRENCRYPTEDPRIVATEKEY, RTDECL, RTCrEncryptedPrivateKey, SeqCore.Asn1Core);

#endif

/** @} */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_crypto_rsa_h */