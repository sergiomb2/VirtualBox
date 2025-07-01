/********************************************************************************/
/*										*/
/*			Include Headers for Internal Routines			*/
/*			     Written by Ken Goldman				*/
/*		       IBM Thomas J. Watson Research Center			*/
/*            $Id: CryptEccCrypt_fp.h 1594 2020-03-26 22:15:48Z kgoldman $	*/
/*										*/
/*  Licenses and Notices							*/
/*										*/
/*  1. Copyright Licenses:							*/
/*										*/
/*  - Trusted Computing Group (TCG) grants to the user of the source code in	*/
/*    this specification (the "Source Code") a worldwide, irrevocable, 		*/
/*    nonexclusive, royalty free, copyright license to reproduce, create 	*/
/*    derivative works, distribute, display and perform the Source Code and	*/
/*    derivative works thereof, and to grant others the rights granted herein.	*/
/*										*/
/*  - The TCG grants to the user of the other parts of the specification 	*/
/*    (other than the Source Code) the rights to reproduce, distribute, 	*/
/*    display, and perform the specification solely for the purpose of 		*/
/*    developing products based on such documents.				*/
/*										*/
/*  2. Source Code Distribution Conditions:					*/
/*										*/
/*  - Redistributions of Source Code must retain the above copyright licenses, 	*/
/*    this list of conditions and the following disclaimers.			*/
/*										*/
/*  - Redistributions in binary form must reproduce the above copyright 	*/
/*    licenses, this list of conditions	and the following disclaimers in the 	*/
/*    documentation and/or other materials provided with the distribution.	*/
/*										*/
/*  3. Disclaimers:								*/
/*										*/
/*  - THE COPYRIGHT LICENSES SET FORTH ABOVE DO NOT REPRESENT ANY FORM OF	*/
/*  LICENSE OR WAIVER, EXPRESS OR IMPLIED, BY ESTOPPEL OR OTHERWISE, WITH	*/
/*  RESPECT TO PATENT RIGHTS HELD BY TCG MEMBERS (OR OTHER THIRD PARTIES)	*/
/*  THAT MAY BE NECESSARY TO IMPLEMENT THIS SPECIFICATION OR OTHERWISE.		*/
/*  Contact TCG Administration (admin@trustedcomputinggroup.org) for 		*/
/*  information on specification licensing rights available through TCG 	*/
/*  membership agreements.							*/
/*										*/
/*  - THIS SPECIFICATION IS PROVIDED "AS IS" WITH NO EXPRESS OR IMPLIED 	*/
/*    WARRANTIES WHATSOEVER, INCLUDING ANY WARRANTY OF MERCHANTABILITY OR 	*/
/*    FITNESS FOR A PARTICULAR PURPOSE, ACCURACY, COMPLETENESS, OR 		*/
/*    NONINFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS, OR ANY WARRANTY 		*/
/*    OTHERWISE ARISING OUT OF ANY PROPOSAL, SPECIFICATION OR SAMPLE.		*/
/*										*/
/*  - Without limitation, TCG and its members and licensors disclaim all 	*/
/*    liability, including liability for infringement of any proprietary 	*/
/*    rights, relating to use of information in this specification and to the	*/
/*    implementation of this specification, and TCG disclaims all liability for	*/
/*    cost of procurement of substitute goods or services, lost profits, loss 	*/
/*    of use, loss of data or any incidental, consequential, direct, indirect, 	*/
/*    or special damages, whether under contract, tort, warranty or otherwise, 	*/
/*    arising in any way out of use or reliance upon this specification or any 	*/
/*    information herein.							*/
/*										*/
/*  (c) Copyright IBM Corp. and others, 2020 - 2022				*/
/*										*/
/********************************************************************************/

#ifndef CRYPTECCCRYPT_FP_H
#define CRYPTECCCRYPT_FP_H

BOOL
CryptEccSelectScheme(
		     OBJECT              *key,           //IN: key containing default scheme
		     TPMT_KDF_SCHEME     *scheme         // IN: a decrypt scheme
		     );

LIB_EXPORT TPM_RC
CryptEccEncrypt(
		OBJECT                  *key,           // IN: public key of recipient
		TPMT_KDF_SCHEME         *scheme,        // IN: scheme to use.
		TPM2B_MAX_BUFFER        *plainText,     // IN: the text to obfuscate
		TPMS_ECC_POINT          *c1,            // OUT: public ephemeral key
		TPM2B_MAX_BUFFER        *c2,            // OUT: obfuscated text
		TPM2B_DIGEST            *c3             // OUT: digest of ephemeral key
		//      and plainText
		);
LIB_EXPORT TPM_RC
CryptEccDecrypt(
		OBJECT                  *key,           // IN: key used for data recovery
		TPMT_KDF_SCHEME         *scheme,        // IN: scheme to use.
		TPM2B_MAX_BUFFER        *plainText,     // OUT: the recovered text
		TPMS_ECC_POINT          *c1,            // IN: public ephemeral key
		TPM2B_MAX_BUFFER        *c2,            // IN: obfuscated text
		TPM2B_DIGEST            *c3             // IN: digest of ephemeral key
		//      and plainText
		);

#endif
