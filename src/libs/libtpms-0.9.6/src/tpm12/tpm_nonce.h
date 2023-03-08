/********************************************************************************/
/*                                                                              */
/*                              Nonce Handler                                   */
/*                           Written by Ken Goldman                             */
/*                     IBM Thomas J. Watson Research Center                     */
/*            $Id: tpm_nonce.h 4071 2010-04-29 19:26:45Z kgoldman $             */
/*                                                                              */
/* (c) Copyright IBM Corporation 2006, 2010.					*/
/*										*/
/* All rights reserved.								*/
/* 										*/
/* Redistribution and use in source and binary forms, with or without		*/
/* modification, are permitted provided that the following conditions are	*/
/* met:										*/
/* 										*/
/* Redistributions of source code must retain the above copyright notice,	*/
/* this list of conditions and the following disclaimer.			*/
/* 										*/
/* Redistributions in binary form must reproduce the above copyright		*/
/* notice, this list of conditions and the following disclaimer in the		*/
/* documentation and/or other materials provided with the distribution.		*/
/* 										*/
/* Neither the names of the IBM Corporation nor the names of its		*/
/* contributors may be used to endorse or promote products derived from		*/
/* this software without specific prior written permission.			*/
/* 										*/
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS		*/
/* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT		*/
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR	*/
/* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT		*/
/* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,	*/
/* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT		*/
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,	*/
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY	*/
/* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT		*/
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE	*/
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.		*/
/********************************************************************************/

#ifndef TPM_NONCE_H
#define TPM_NONCE_H

#include "tpm_store.h"
#include "tpm_structures.h"

void       TPM_Nonce_Init(TPM_NONCE tpm_nonce);

TPM_RESULT TPM_Nonce_Generate(TPM_NONCE tpm_nonce);

void       TPM_Nonce_Copy(TPM_NONCE destination,
                          const TPM_NONCE source);
TPM_RESULT TPM_Nonce_Load(TPM_NONCE tpm_nonce,
                          unsigned char **stream,
                          uint32_t *stream_size);
TPM_RESULT TPM_Nonce_Store(TPM_STORE_BUFFER *sbuffer,
                           const TPM_NONCE tpm_nonce);
TPM_RESULT TPM_Nonce_Compare(TPM_NONCE expect, const TPM_NONCE actual);
void       TPM_Nonce_IsZero(TPM_BOOL *isZero, TPM_NONCE tpm_nonce);

#endif
