/********************************************************************************/
/*                                                                              */
/*                              Libtpms IO Init                                 */
/*                           Written by Ken Goldman, Stefan Berger              */
/*                     IBM Thomas J. Watson Research Center                     */
/*            $Id: tpm_io.c 4564 2011-04-13 19:33:38Z stefanb $                 */
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

#include "tpm_debug.h"
#include "tpm_error.h"
#include "tpm_platform.h"
#include "tpm_types.h"

#include "tpm_library_intern.h"

/* header for this file */
#include "tpm_io.h"

/* TPM_IO_Init initializes the TPM to host interface.

   This is the Unix platform dependent socket version.
*/

TPM_RESULT TPM_IO_Init(void)
{
    TPM_RESULT          rc = 0;
    struct libtpms_callbacks *cbs = TPMLIB_GetCallbacks();

    /* call user-provided function if available */
    if (cbs->tpm_io_init) {
        rc = cbs->tpm_io_init();
        return rc;
    } else {
        /* Below code would start a TCP server socket; we don't want this
           but rather expect all commands via TPMLIB_Process() to be sent
           to the TPM. */
        return TPM_SUCCESS;
    }
}
