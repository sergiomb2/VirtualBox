/********************************************************************************/
/*                                                                              */
/*                              TPM Platform I/O                                */
/*                           Written by Ken Goldman                             */
/*                     IBM Thomas J. Watson Research Center                     */
/*            $Id: tpm_platform.h 4621 2011-09-09 20:19:42Z kgoldman $                */
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

/* Interface independent platform I/O Functions */

#ifndef TPM_PLATFORM_H
#define TPM_PLATFORM_H

#include "tpm_types.h"

/* Every platform will need this function, as TPM_MainInit() calls it. */

TPM_RESULT TPM_IO_Init(void);

TPM_RESULT TPM_IO_GetLocality(TPM_MODIFIER_INDICATOR *localityModifier,
			      uint32_t tpm_number);
TPM_RESULT TPM_IO_GetPhysicalPresence(TPM_BOOL *physicalPresence,
				      uint32_t tpm_number);
TPM_RESULT TPM_IO_GPIO_Write(TPM_NV_INDEX nvIndex,
                             uint32_t dataSize,
                             BYTE *data,
			     uint32_t tpm_number);
TPM_RESULT TPM_IO_GPIO_Read(TPM_NV_INDEX nvIndex,
                            uint32_t dataSize,
                            BYTE *data,
			    uint32_t tpm_number);

#endif
