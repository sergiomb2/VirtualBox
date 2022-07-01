; $Id$
;; @file
; IEM - Instruction Implementation in Assembly.
;

;
; Copyright (C) 2011-2022 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;


;*********************************************************************************************************************************
;*  Header Files                                                                                                                 *
;*********************************************************************************************************************************
%include "VBox/asmdefs.mac"
%include "VBox/err.mac"
%include "iprt/x86.mac"


;*********************************************************************************************************************************
;*  Defined Constants And Macros                                                                                                 *
;*********************************************************************************************************************************

;;
; RET XX / RET wrapper for fastcall.
;
%macro RET_FASTCALL 1
%ifdef RT_ARCH_X86
 %ifdef RT_OS_WINDOWS
    ret %1
 %else
    ret
 %endif
%else
    ret
%endif
%endmacro

;;
; NAME for fastcall functions.
;
;; @todo 'global @fastcall@12' is still broken in yasm and requires dollar
;         escaping (or whatever the dollar is good for here).  Thus the ugly
;         prefix argument.
;
%define NAME_FASTCALL(a_Name, a_cbArgs, a_Prefix)   NAME(a_Name)
%ifdef RT_ARCH_X86
 %ifdef RT_OS_WINDOWS
  %undef NAME_FASTCALL
  %define NAME_FASTCALL(a_Name, a_cbArgs, a_Prefix) a_Prefix %+ a_Name %+ @ %+ a_cbArgs
 %endif
%endif

;;
; BEGINPROC for fastcall functions.
;
; @param        1       The function name (C).
; @param        2       The argument size on x86.
;
%macro BEGINPROC_FASTCALL 2
 %ifdef ASM_FORMAT_PE
  export %1=NAME_FASTCALL(%1,%2,$@)
 %endif
 %ifdef __NASM__
  %ifdef ASM_FORMAT_OMF
   export NAME(%1) NAME_FASTCALL(%1,%2,$@)
  %endif
 %endif
 %ifndef ASM_FORMAT_BIN
  global NAME_FASTCALL(%1,%2,$@)
 %endif
NAME_FASTCALL(%1,%2,@):
%endmacro


;
; We employ some macro assembly here to hid the calling convention differences.
;
%ifdef RT_ARCH_AMD64
 %macro PROLOGUE_1_ARGS 0
 %endmacro
 %macro EPILOGUE_1_ARGS 0
        ret
 %endmacro
 %macro EPILOGUE_1_ARGS_EX 0
        ret
 %endmacro

 %macro PROLOGUE_2_ARGS 0
 %endmacro
 %macro EPILOGUE_2_ARGS 0
        ret
 %endmacro
 %macro EPILOGUE_2_ARGS_EX 1
        ret
 %endmacro

 %macro PROLOGUE_3_ARGS 0
 %endmacro
 %macro EPILOGUE_3_ARGS 0
        ret
 %endmacro
 %macro EPILOGUE_3_ARGS_EX 1
        ret
 %endmacro

 %macro PROLOGUE_4_ARGS 0
 %endmacro
 %macro EPILOGUE_4_ARGS 0
        ret
 %endmacro
 %macro EPILOGUE_4_ARGS_EX 1
        ret
 %endmacro

 %ifdef ASM_CALL64_GCC
  %define A0        rdi
  %define A0_32     edi
  %define A0_16     di
  %define A0_8      dil

  %define A1        rsi
  %define A1_32     esi
  %define A1_16     si
  %define A1_8      sil

  %define A2        rdx
  %define A2_32     edx
  %define A2_16     dx
  %define A2_8      dl

  %define A3        rcx
  %define A3_32     ecx
  %define A3_16     cx
 %endif

 %ifdef ASM_CALL64_MSC
  %define A0        rcx
  %define A0_32     ecx
  %define A0_16     cx
  %define A0_8      cl

  %define A1        rdx
  %define A1_32     edx
  %define A1_16     dx
  %define A1_8      dl

  %define A2        r8
  %define A2_32     r8d
  %define A2_16     r8w
  %define A2_8      r8b

  %define A3        r9
  %define A3_32     r9d
  %define A3_16     r9w
 %endif

 %define T0         rax
 %define T0_32      eax
 %define T0_16      ax
 %define T0_8       al

 %define T1         r11
 %define T1_32      r11d
 %define T1_16      r11w
 %define T1_8       r11b

 %define T2         r10                 ; only AMD64
 %define T2_32      r10d
 %define T2_16      r10w
 %define T2_8       r10b

%else
 ; x86
 %macro PROLOGUE_1_ARGS 0
        push    edi
 %endmacro
 %macro EPILOGUE_1_ARGS 0
        pop     edi
        ret     0
 %endmacro
 %macro EPILOGUE_1_ARGS_EX 1
        pop     edi
        ret     %1
 %endmacro

 %macro PROLOGUE_2_ARGS 0
        push    edi
 %endmacro
 %macro EPILOGUE_2_ARGS 0
        pop     edi
        ret     0
 %endmacro
 %macro EPILOGUE_2_ARGS_EX 1
        pop     edi
        ret     %1
 %endmacro

 %macro PROLOGUE_3_ARGS 0
        push    ebx
        mov     ebx, [esp + 4 + 4]
        push    edi
 %endmacro
 %macro EPILOGUE_3_ARGS_EX 1
  %if (%1) < 4
   %error "With three args, at least 4 bytes must be remove from the stack upon return (32-bit)."
  %endif
        pop     edi
        pop     ebx
        ret     %1
 %endmacro
 %macro EPILOGUE_3_ARGS 0
        EPILOGUE_3_ARGS_EX 4
 %endmacro

 %macro PROLOGUE_4_ARGS 0
        push    ebx
        push    edi
        push    esi
        mov     ebx, [esp + 12 + 4 + 0]
        mov     esi, [esp + 12 + 4 + 4]
 %endmacro
 %macro EPILOGUE_4_ARGS_EX 1
  %if (%1) < 8
   %error "With four args, at least 8 bytes must be remove from the stack upon return (32-bit)."
  %endif
        pop     esi
        pop     edi
        pop     ebx
        ret     %1
 %endmacro
 %macro EPILOGUE_4_ARGS 0
        EPILOGUE_4_ARGS_EX 8
 %endmacro

 %define A0         ecx
 %define A0_32      ecx
 %define A0_16       cx
 %define A0_8        cl

 %define A1         edx
 %define A1_32      edx
 %define A1_16      dx
 %define A1_8       dl

 %define A2         ebx
 %define A2_32      ebx
 %define A2_16      bx
 %define A2_8       bl

 %define A3         esi
 %define A3_32      esi
 %define A3_16      si

 %define T0         eax
 %define T0_32      eax
 %define T0_16      ax
 %define T0_8       al

 %define T1         edi
 %define T1_32      edi
 %define T1_16      di
%endif


;;
; Load the relevant flags from [%1] if there are undefined flags (%3).
;
; @remarks      Clobbers T0, stack. Changes EFLAGS.
; @param        A2      The register pointing to the flags.
; @param        1       The parameter (A0..A3) pointing to the eflags.
; @param        2       The set of modified flags.
; @param        3       The set of undefined flags.
;
%macro IEM_MAYBE_LOAD_FLAGS 3
 ;%if (%3) != 0
        pushf                           ; store current flags
        mov     T0_32, [%1]             ; load the guest flags
        and     dword [xSP], ~(%2 | %3) ; mask out the modified and undefined flags
        and     T0_32, (%2 | %3)        ; select the modified and undefined flags.
        or      [xSP], T0               ; merge guest flags with host flags.
        popf                            ; load the mixed flags.
 ;%endif
%endmacro

;;
; Update the flag.
;
; @remarks  Clobbers T0, T1, stack.
; @param        1       The register pointing to the EFLAGS.
; @param        2       The mask of modified flags to save.
; @param        3       The mask of undefined flags to (maybe) save.
;
%macro IEM_SAVE_FLAGS 3
 %if (%2 | %3) != 0
        pushf
        pop     T1
        mov     T0_32, [%1]             ; flags
        and     T0_32, ~(%2 | %3)       ; clear the modified & undefined flags.
        and     T1_32, (%2 | %3)        ; select the modified and undefined flags.
        or      T0_32, T1_32            ; combine the flags.
        mov     [%1], T0_32             ; save the flags.
 %endif
%endmacro

;;
; Calculates the new EFLAGS based on the CPU EFLAGS and fixed clear and set bit masks.
;
; @remarks  Clobbers T0, T1, stack.
; @param        1       The register pointing to the EFLAGS.
; @param        2       The mask of modified flags to save.
; @param        3       Mask of additional flags to always clear
; @param        4       Mask of additional flags to always set.
;
%macro IEM_SAVE_AND_ADJUST_FLAGS 4
 %if (%2 | %3 | %4) != 0
        pushf
        pop     T1
        mov     T0_32, [%1]             ; load flags.
        and     T0_32, ~(%2 | %3)       ; clear the modified and always cleared flags.
        and     T1_32, (%2)             ; select the modified flags.
        or      T0_32, T1_32            ; combine the flags.
  %if (%4) != 0
        or      T0_32, %4               ; add the always set flags.
  %endif
        mov     [%1], T0_32             ; save the result.
 %endif
%endmacro

;;
; Calculates the new EFLAGS based on the CPU EFLAGS (%2), a clear mask (%3),
; signed input (%4[%5]) and parity index (%6).
;
; This is used by MUL and IMUL, where we got result (%4 & %6) in xAX which is
; also T0. So, we have to use T1 for the EFLAGS calculation and save T0/xAX
; while we extract the %2 flags from the CPU EFLAGS or use T2 (only AMD64).
;
; @remarks  Clobbers T0, T1, stack, %6, EFLAGS.
; @param        1       The register pointing to the EFLAGS.
; @param        2       The mask of modified flags to save.
; @param        3       Mask of additional flags to always clear
; @param        4       The result register to set SF by.
; @param        5       The width of the %4 register in bits (8, 16, 32, or 64).
; @param        6       The (full) register containing the parity table index. Will be modified!

%macro IEM_SAVE_FLAGS_ADJUST_AND_CALC_SF_PF 6
 %ifdef RT_ARCH_AMD64
        pushf
        pop     T2
 %else
        push    T0
        pushf
        pop     T0
 %endif
        mov     T1_32, [%1]             ; load flags.
        and     T1_32, ~(%2 | %3 | X86_EFL_PF | X86_EFL_SF)  ; clear the modified, always cleared flags and the two flags we calc.
 %ifdef RT_ARCH_AMD64
        and     T2_32, (%2)             ; select the modified flags.
        or      T1_32, T2_32            ; combine the flags.
 %else
        and     T0_32, (%2)             ; select the modified flags.
        or      T1_32, T0_32            ; combine the flags.
        pop     T0
 %endif

        ; First calculate SF as it's likely to be refereing to the same register as %6 does.
        bt      %4, %5 - 1
        jnc     %%sf_clear
        or      T1_32, X86_EFL_SF
 %%sf_clear:

        ; Parity last.
        and     %6, 0xff
 %ifdef RT_ARCH_AMD64
        lea     T2, [NAME(g_afParity) xWrtRIP]
        or      T1_8, [T2 + %6]
 %else
        or      T1_8, [NAME(g_afParity) + %6]
 %endif

        mov     [%1], T1_32             ; save the result.
%endmacro

;;
; Calculates the new EFLAGS using fixed clear and set bit masks.
;
; @remarks  Clobbers T0.
; @param        1       The register pointing to the EFLAGS.
; @param        2       Mask of additional flags to always clear
; @param        3       Mask of additional flags to always set.
;
%macro IEM_ADJUST_FLAGS 3
 %if (%2 | %3) != 0
        mov     T0_32, [%1]             ; Load flags.
  %if (%2) != 0
        and     T0_32, ~(%2)            ; Remove the always cleared flags.
  %endif
  %if (%3) != 0
        or      T0_32, %3               ; Add the always set flags.
  %endif
        mov     [%1], T0_32             ; Save the result.
 %endif
%endmacro

;;
; Calculates the new EFLAGS using fixed clear and set bit masks.
;
; @remarks  Clobbers T0, %4, EFLAGS.
; @param        1       The register pointing to the EFLAGS.
; @param        2       Mask of additional flags to always clear
; @param        3       Mask of additional flags to always set.
; @param        4       The (full) register containing the parity table index. Will be modified!
;
%macro IEM_ADJUST_FLAGS_WITH_PARITY 4
        mov     T0_32, [%1]                 ; Load flags.
        and     T0_32, ~(%2 | X86_EFL_PF)   ; Remove PF and the always cleared flags.
 %if (%3) != 0
        or      T0_32, %3                   ; Add the always set flags.
 %endif
        and     %4, 0xff
 %ifdef RT_ARCH_AMD64
        lea     T2, [NAME(g_afParity) xWrtRIP]
        or      T0_8, [T2 + %4]
 %else
        or      T0_8, [NAME(g_afParity) + %4]
 %endif
        mov     [%1], T0_32             ; Save the result.
%endmacro


;*********************************************************************************************************************************
;*  External Symbols                                                                                                             *
;*********************************************************************************************************************************
extern NAME(g_afParity)


;;
; Macro for implementing a binary operator.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses with locked
; variants, except on 32-bit system where the 64-bit accesses requires hand
; coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       Non-zero if there should be a locked version.
; @param        3       The modified flags.
; @param        4       The undefined flags.
;
%macro IEMIMPL_BIN_OP 4
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      byte [A0], A1_8
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u8

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      word [A0], A1_16
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      dword [A0], A1_32
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      qword [A0], A1
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS_EX 8
ENDPROC iemAImpl_ %+ %1 %+ _u64
  %endif ; RT_ARCH_AMD64

 %if %2 != 0 ; locked versions requested?

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 byte [A0], A1_8
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u8_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 word [A0], A1_16
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 dword [A0], A1_32
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32_locked

  %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 qword [A0], A1
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS_EX 8
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
  %endif ; RT_ARCH_AMD64
 %endif ; locked
%endmacro

;            instr,lock, modified-flags,                                                               undefined flags
IEMIMPL_BIN_OP add,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP adc,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP sub,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP sbb,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP or,   1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF),              X86_EFL_AF
IEMIMPL_BIN_OP xor,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF),              X86_EFL_AF
IEMIMPL_BIN_OP and,  1, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF),              X86_EFL_AF
IEMIMPL_BIN_OP cmp,  0, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_BIN_OP test, 0, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF),              X86_EFL_AF


;;
; Macro for implementing a binary operator, VEX variant with separate input/output.
;
; This will generate code for the 32 and 64 bit accesses, except on 32-bit system
; where the 64-bit accesses requires hand coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the first source register operand in A1, the second source register operand
; in A2 and a pointer to eflags in A3.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
%macro IEMIMPL_VEX_BIN_OP 3
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS           A3, %2, %3
        %1      T0_32, A1_32, A2_32
        mov     [A0], T0_32
        IEM_SAVE_FLAGS                 A3, %2, %3
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS           A3, %2, %3
        %1      T0, A1, A2
        mov     [A0], T0
        IEM_SAVE_FLAGS                 A3, %2, %3
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %endif ; RT_ARCH_AMD64
%endmacro

;                 instr,  modified-flags,                                                                undefined-flags
IEMIMPL_VEX_BIN_OP andn,  (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_CF),                           (X86_EFL_AF | X86_EFL_PF)
IEMIMPL_VEX_BIN_OP bextr, (X86_EFL_OF | X86_EFL_ZF | X86_EFL_CF),                                        (X86_EFL_SF | X86_EFL_AF | X86_EFL_PF)
IEMIMPL_VEX_BIN_OP bzhi,  (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_CF),                           (X86_EFL_AF | X86_EFL_PF)

;;
; Macro for implementing BLSR, BLCMSK and BLSI (fallbacks implemented in C).
;
; This will generate code for the 32 and 64 bit accesses, except on 32-bit system
; where the 64-bit accesses requires hand coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
%macro IEMIMPL_VEX_BIN_OP_2 3
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        mov     T0_32, [A0]
        %1      T0_32, A1_32
        mov     [A0], T0_32
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 12
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        mov     T0, [A0]
        %1      T0, A1
        mov     [A0], T0
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %endif ; RT_ARCH_AMD64
%endmacro

;                  instr,  modified-flags,                                      undefined-flags
IEMIMPL_VEX_BIN_OP_2 blsr,   (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_CF), (X86_EFL_AF | X86_EFL_PF)
IEMIMPL_VEX_BIN_OP_2 blsmsk, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_CF), (X86_EFL_AF | X86_EFL_PF)
IEMIMPL_VEX_BIN_OP_2 blsi,   (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_CF), (X86_EFL_AF | X86_EFL_PF)


;;
; Macro for implementing a binary operator w/o flags, VEX variant with separate input/output.
;
; This will generate code for the 32 and 64 bit accesses, except on 32-bit system
; where the 64-bit accesses requires hand coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the first source register operand in A1, the second source register operand
; in A2 and a pointer to eflags in A3.
;
; @param        1       The instruction mnemonic.
; @param        2       Fallback instruction if applicable.
; @param        3       Whether to emit fallback or not.
;
%macro IEMIMPL_VEX_BIN_OP_NOEFL 3
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        %1      T0_32, A1_32, A2_32
        mov     [A0], T0_32
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %if %3
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32_fallback, 12
        PROLOGUE_3_ARGS
  %ifdef ASM_CALL64_GCC
        mov     cl, A2_8
        %2      A1_32, cl
        mov     [A0], A1_32
  %else
        xchg    A2, A0
        %2      A1_32, cl
        mov     [A2], A1_32
  %endif
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32_fallback
 %endif

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 12
        PROLOGUE_3_ARGS
        %1      T0, A1, A2
        mov     [A0], T0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64

 %if %3
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_fallback, 12
        PROLOGUE_3_ARGS
  %ifdef ASM_CALL64_GCC
        mov     cl, A2_8
        %2      A1, cl
        mov     [A0], A1_32
  %else
        xchg    A2, A0
        %2      A1, cl
        mov     [A2], A1_32
  %endif
        mov     [A0], A1
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64_fallback
  %endif
 %endif ; RT_ARCH_AMD64
%endmacro

;                           instr, fallback instr, emit fallback
IEMIMPL_VEX_BIN_OP_NOEFL    sarx,  sar,            1
IEMIMPL_VEX_BIN_OP_NOEFL    shlx,  shl,            1
IEMIMPL_VEX_BIN_OP_NOEFL    shrx,  shr,            1
IEMIMPL_VEX_BIN_OP_NOEFL    pdep,  nop,            0
IEMIMPL_VEX_BIN_OP_NOEFL    pext,  nop,            0


;
; RORX uses a immediate byte for the shift count, so we only do
; fallback implementation of that one.
;
BEGINPROC_FASTCALL iemAImpl_rorx_u32, 12
        PROLOGUE_3_ARGS
 %ifdef ASM_CALL64_GCC
        mov     cl, A2_8
        ror     A1_32, cl
        mov     [A0], A1_32
 %else
        xchg    A2, A0
        ror     A1_32, cl
        mov     [A2], A1_32
 %endif
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_rorx_u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_rorx_u64, 12
        PROLOGUE_3_ARGS
 %ifdef ASM_CALL64_GCC
        mov     cl, A2_8
        ror     A1, cl
        mov     [A0], A1_32
 %else
        xchg    A2, A0
        ror     A1, cl
        mov     [A2], A1_32
 %endif
        mov     [A0], A1
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_rorx_u64
 %endif ; RT_ARCH_AMD64


;
; MULX
;
BEGINPROC_FASTCALL iemAImpl_mulx_u32, 16
        PROLOGUE_4_ARGS
%ifdef ASM_CALL64_GCC
        ; A2_32 is EDX - prefect
        mulx    T0_32, T1_32, A3_32
        mov     [A1], T1_32 ; Low value first, as we should return the high part if same destination registers.
        mov     [A0], T0_32
%else
        ; A1 is xDX - must switch A1 and A2, so EDX=uSrc1
        xchg    A1, A2
        mulx    T0_32, T1_32, A3_32
        mov     [A2], T1_32 ; Low value first, as we should return the high part if same destination registers.
        mov     [A0], T0_32
%endif
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_mulx_u32


BEGINPROC_FASTCALL iemAImpl_mulx_u32_fallback, 16
        PROLOGUE_4_ARGS
%ifdef ASM_CALL64_GCC
        ; A2_32 is EDX, T0_32 is EAX
        mov     eax, A3_32
        mul     A2_32
        mov     [A1], eax ; Low value first, as we should return the high part if same destination registers.
        mov     [A0], edx
%else
        ; A1 is xDX, T0_32 is EAX - must switch A1 and A2, so EDX=uSrc1
        xchg    A1, A2
        mov     eax, A3_32
        mul     A2_32
        mov     [A2], eax ; Low value first, as we should return the high part if same destination registers.
        mov     [A0], edx
%endif
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_mulx_u32_fallback

%ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_mulx_u64, 16
        PROLOGUE_4_ARGS
%ifdef ASM_CALL64_GCC
        ; A2 is RDX - prefect
        mulx    T0, T1, A3
        mov     [A1], T1 ; Low value first, as we should return the high part if same destination registers.
        mov     [A0], T0
%else
        ; A1 is xDX - must switch A1 and A2, so RDX=uSrc1
        xchg    A1, A2
        mulx    T0, T1, A3
        mov     [A2], T1 ; Low value first, as we should return the high part if same destination registers.
        mov     [A0], T0
%endif
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_mulx_u64


BEGINPROC_FASTCALL iemAImpl_mulx_u64_fallback, 16
        PROLOGUE_4_ARGS
%ifdef ASM_CALL64_GCC
        ; A2 is RDX, T0 is RAX
        mov     rax, A3
        mul     A2
        mov     [A1], rax ; Low value first, as we should return the high part if same destination registers.
        mov     [A0], rdx
%else
        ; A1 is xDX, T0 is RAX - must switch A1 and A2, so RDX=uSrc1
        xchg    A1, A2
        mov     rax, A3
        mul     A2
        mov     [A2], rax ; Low value first, as we should return the high part if same destination registers.
        mov     [A0], rdx
%endif
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_mulx_u64_fallback

%endif


;;
; Macro for implementing a bit operator.
;
; This will generate code for the 16, 32 and 64 bit accesses with locked
; variants, except on 32-bit system where the 64-bit accesses requires hand
; coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       Non-zero if there should be a locked version.
; @param        3       The modified flags.
; @param        4       The undefined flags.
;
%macro IEMIMPL_BIT_OP 4
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      word [A0], A1_16
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      dword [A0], A1_32
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        %1      qword [A0], A1
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS_EX 8
ENDPROC iemAImpl_ %+ %1 %+ _u64
  %endif ; RT_ARCH_AMD64

 %if %2 != 0 ; locked versions requested?

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 word [A0], A1_16
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 dword [A0], A1_32
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32_locked

  %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %3, %4
        lock %1 qword [A0], A1
        IEM_SAVE_FLAGS                 A2, %3, %4
        EPILOGUE_3_ARGS_EX 8
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
  %endif ; RT_ARCH_AMD64
 %endif ; locked
%endmacro
IEMIMPL_BIT_OP bt,  0, (X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
IEMIMPL_BIT_OP btc, 1, (X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
IEMIMPL_BIT_OP bts, 1, (X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)
IEMIMPL_BIT_OP btr, 1, (X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF)

;;
; Macro for implementing a bit search operator.
;
; This will generate code for the 16, 32 and 64 bit accesses, except on 32-bit
; system where the 64-bit accesses requires hand coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; In the ZF case the destination register is 'undefined', however it seems that
; both AMD and Intel just leaves it as is.  The undefined EFLAGS differs between
; AMD and Intel and accoridng to https://www.sandpile.org/x86/flags.htm between
; Intel microarchitectures.  We only implement 'intel' and 'amd' variation with
; the behaviour of more recent CPUs (Intel 10980X and AMD 3990X).
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
; @param        4       Non-zero if destination isn't written when ZF=1.  Zero if always written.
;
%macro IEMIMPL_BIT_OP2 4
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0_16, A1_16
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T0_16
.unchanged_dst:
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16 %+ _intel, 12
        PROLOGUE_3_ARGS
        %1      T1_16, A1_16
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T1_16
        IEM_ADJUST_FLAGS_WITH_PARITY    A2, X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_CF | X86_EFL_ZF, 0, T1
        EPILOGUE_3_ARGS
.unchanged_dst:
        IEM_ADJUST_FLAGS                A2, X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_CF, X86_EFL_ZF | X86_EFL_PF
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16_intel

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16 %+ _amd, 12
        PROLOGUE_3_ARGS
        %1      T0_16, A1_16
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T0_16
.unchanged_dst:
        IEM_SAVE_AND_ADJUST_FLAGS       A2, %2, 0, 0    ; Only the ZF flag is modified on AMD Zen 2.
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16_amd


BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0_32, A1_32
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T0_32
.unchanged_dst:
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32 %+ _intel, 12
        PROLOGUE_3_ARGS
        %1      T1_32, A1_32
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T1_32
        IEM_ADJUST_FLAGS_WITH_PARITY    A2, X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_CF | X86_EFL_ZF, 0, T1
        EPILOGUE_3_ARGS
.unchanged_dst:
        IEM_ADJUST_FLAGS                A2, X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_CF, X86_EFL_ZF | X86_EFL_PF
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32_intel

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32 %+ _amd, 12
        PROLOGUE_3_ARGS
        %1      T0_32, A1_32
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T0_32
.unchanged_dst:
        IEM_SAVE_AND_ADJUST_FLAGS       A2, %2, 0, 0    ; Only the ZF flag is modified on AMD Zen 2.
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32_amd


 %ifdef RT_ARCH_AMD64

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0, A1
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T0
.unchanged_dst:
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS_EX 8
ENDPROC iemAImpl_ %+ %1 %+ _u64

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64 %+ _intel, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T1, A1
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T1
        IEM_ADJUST_FLAGS_WITH_PARITY    A2, X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_CF | X86_EFL_ZF, 0, T1
        EPILOGUE_3_ARGS
.unchanged_dst:
        IEM_ADJUST_FLAGS                A2, X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_CF, X86_EFL_ZF | X86_EFL_PF
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64_intel

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64 %+ _amd, 16
        PROLOGUE_3_ARGS
        %1      T0, A1
%if %4 != 0
        jz      .unchanged_dst
%endif
        mov     [A0], T0
.unchanged_dst:
        IEM_SAVE_AND_ADJUST_FLAGS       A2, %2, 0, 0    ; Only the ZF flag is modified on AMD Zen 2.
        EPILOGUE_3_ARGS_EX 8
ENDPROC iemAImpl_ %+ %1 %+ _u64_amd

 %endif ; RT_ARCH_AMD64
%endmacro

IEMIMPL_BIT_OP2 bsf,   (X86_EFL_ZF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 1
IEMIMPL_BIT_OP2 bsr,   (X86_EFL_ZF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 1
IEMIMPL_BIT_OP2 tzcnt, (X86_EFL_ZF | X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF), 0
IEMIMPL_BIT_OP2 lzcnt, (X86_EFL_ZF | X86_EFL_CF), (X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF), 0


;;
; Macro for implementing POPCNT.
;
; This will generate code for the 16, 32 and 64 bit accesses, except on 32-bit
; system where the 64-bit accesses requires hand coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; ASSUMES Intel and AMD set EFLAGS the same way.
;
; ASSUMES the instruction does not support memory destination.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
%macro IEMIMPL_BIT_OP3 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0_16, A1_16
        mov     [A0], T0_16
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0_32, A1_32
        mov     [A0], T0_32
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS           A2, %2, %3
        %1      T0, A1
        mov     [A0], T0
        IEM_SAVE_FLAGS                 A2, %2, %3
        EPILOGUE_3_ARGS_EX 8
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %endif ; RT_ARCH_AMD64
%endmacro
IEMIMPL_BIT_OP3 popcnt, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_OF | X86_EFL_SF | X86_EFL_AF | X86_EFL_PF), 0


;
; IMUL is also a similar but yet different case (no lock, no mem dst).
; The rDX:rAX variant of imul is handled together with mul further down.
;
BEGINCODE
; @param        1       EFLAGS that are modified.
; @param        2       Undefined EFLAGS.
; @param        3       Function suffix.
; @param        4       EFLAGS variation: 0 for native, 1 for intel (ignored),
;                       2 for AMD (set AF, clear PF, ZF and SF).
%macro IEMIMPL_IMUL_TWO 4
BEGINPROC_FASTCALL iemAImpl_imul_two_u16 %+ %3, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS                    A2, %1, %2
        imul    A1_16, word [A0]
        mov     [A0], A1_16
 %if %4 != 1
        IEM_SAVE_FLAGS                          A2, %1, %2
 %else
        IEM_SAVE_FLAGS_ADJUST_AND_CALC_SF_PF    A2, %1, X86_EFL_AF | X86_EFL_ZF, A1_16, 16, A1
 %endif
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_imul_two_u16 %+ %3

BEGINPROC_FASTCALL iemAImpl_imul_two_u32 %+ %3, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS                    A2, %1, %2
        imul    A1_32, dword [A0]
        mov     [A0], A1_32
 %if %4 != 1
        IEM_SAVE_FLAGS                          A2, %1, %2
 %else
        IEM_SAVE_FLAGS_ADJUST_AND_CALC_SF_PF    A2, %1, X86_EFL_AF | X86_EFL_ZF, A1_32, 32, A1
 %endif
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_imul_two_u32 %+ %3

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_imul_two_u64 %+ %3, 16
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS                    A2, %1, %2
        imul    A1, qword [A0]
        mov     [A0], A1
 %if %4 != 1
        IEM_SAVE_FLAGS                          A2, %1, %2
 %else
        IEM_SAVE_FLAGS_ADJUST_AND_CALC_SF_PF    A2, %1, X86_EFL_AF | X86_EFL_ZF, A1, 64, A1
 %endif
        EPILOGUE_3_ARGS_EX 8
ENDPROC iemAImpl_imul_two_u64 %+ %3
 %endif ; RT_ARCH_AMD64
%endmacro
IEMIMPL_IMUL_TWO X86_EFL_OF | X86_EFL_CF, X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF,       , 0
IEMIMPL_IMUL_TWO X86_EFL_OF | X86_EFL_CF, 0,                                                 _intel, 1
IEMIMPL_IMUL_TWO X86_EFL_OF | X86_EFL_CF, 0,                                                 _amd,   2


;
; XCHG for memory operands.  This implies locking.  No flag changes.
;
; Each function takes two arguments, first the pointer to the memory,
; then the pointer to the register.  They all return void.
;
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_xchg_u8_locked, 8
        PROLOGUE_2_ARGS
        mov     T0_8, [A1]
        xchg    [A0], T0_8
        mov     [A1], T0_8
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_xchg_u8_locked

BEGINPROC_FASTCALL iemAImpl_xchg_u16_locked, 8
        PROLOGUE_2_ARGS
        mov     T0_16, [A1]
        xchg    [A0], T0_16
        mov     [A1], T0_16
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_xchg_u16_locked

BEGINPROC_FASTCALL iemAImpl_xchg_u32_locked, 8
        PROLOGUE_2_ARGS
        mov     T0_32, [A1]
        xchg    [A0], T0_32
        mov     [A1], T0_32
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_xchg_u32_locked

%ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_xchg_u64_locked, 8
        PROLOGUE_2_ARGS
        mov     T0, [A1]
        xchg    [A0], T0
        mov     [A1], T0
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_xchg_u64_locked
%endif

; Unlocked variants for fDisregardLock mode.

BEGINPROC_FASTCALL iemAImpl_xchg_u8_unlocked, 8
        PROLOGUE_2_ARGS
        mov     T0_8, [A1]
        mov     T1_8, [A0]
        mov     [A0], T0_8
        mov     [A1], T1_8
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_xchg_u8_unlocked

BEGINPROC_FASTCALL iemAImpl_xchg_u16_unlocked, 8
        PROLOGUE_2_ARGS
        mov     T0_16, [A1]
        mov     T1_16, [A0]
        mov     [A0], T0_16
        mov     [A1], T1_16
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_xchg_u16_unlocked

BEGINPROC_FASTCALL iemAImpl_xchg_u32_unlocked, 8
        PROLOGUE_2_ARGS
        mov     T0_32, [A1]
        mov     T1_32, [A0]
        mov     [A0], T0_32
        mov     [A1], T1_32
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_xchg_u32_unlocked

%ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_xchg_u64_unlocked, 8
        PROLOGUE_2_ARGS
        mov     T0, [A1]
        mov     T1, [A0]
        mov     [A0], T0
        mov     [A1], T1
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_xchg_u64_unlocked
%endif


;
; XADD for memory operands.
;
; Each function takes three arguments, first the pointer to the
; memory/register, then the pointer to the register, and finally a pointer to
; eflags.  They all return void.
;
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_xadd_u8, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_8, [A1]
        xadd    [A0], T0_8
        mov     [A1], T0_8
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_xadd_u8

BEGINPROC_FASTCALL iemAImpl_xadd_u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_16, [A1]
        xadd    [A0], T0_16
        mov     [A1], T0_16
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_xadd_u16

BEGINPROC_FASTCALL iemAImpl_xadd_u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_32, [A1]
        xadd    [A0], T0_32
        mov     [A1], T0_32
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_xadd_u32

%ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_xadd_u64, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0, [A1]
        xadd    [A0], T0
        mov     [A1], T0
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_xadd_u64
%endif ; RT_ARCH_AMD64

BEGINPROC_FASTCALL iemAImpl_xadd_u8_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_8, [A1]
        lock xadd [A0], T0_8
        mov     [A1], T0_8
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_xadd_u8_locked

BEGINPROC_FASTCALL iemAImpl_xadd_u16_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_16, [A1]
        lock xadd [A0], T0_16
        mov     [A1], T0_16
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_xadd_u16_locked

BEGINPROC_FASTCALL iemAImpl_xadd_u32_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0_32, [A1]
        lock xadd [A0], T0_32
        mov     [A1], T0_32
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_xadd_u32_locked

%ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_xadd_u64_locked, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        mov     T0, [A1]
        lock xadd [A0], T0
        mov     [A1], T0
        IEM_SAVE_FLAGS       A2, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_xadd_u64_locked
%endif ; RT_ARCH_AMD64


;
; CMPXCHG8B.
;
; These are tricky register wise, so the code is duplicated for each calling
; convention.
;
; WARNING! This code make ASSUMPTIONS about which registers T1 and T0 are mapped to!
;
; C-proto:
; IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg8b,(uint64_t *pu64Dst, PRTUINT64U pu64EaxEdx, PRTUINT64U pu64EbxEcx,
;                                             uint32_t *pEFlags));
;
; Note! Identical to iemAImpl_cmpxchg16b.
;
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_cmpxchg8b, 16
%ifdef RT_ARCH_AMD64
 %ifdef ASM_CALL64_MSC
        push    rbx

        mov     r11, rdx                ; pu64EaxEdx (is also T1)
        mov     r10, rcx                ; pu64Dst

        mov     ebx, [r8]
        mov     ecx, [r8 + 4]
        IEM_MAYBE_LOAD_FLAGS r9, (X86_EFL_ZF), 0 ; clobbers T0 (eax)
        mov     eax, [r11]
        mov     edx, [r11 + 4]

        lock cmpxchg8b [r10]

        mov     [r11], eax
        mov     [r11 + 4], edx
        IEM_SAVE_FLAGS       r9, (X86_EFL_ZF), 0 ; clobbers T0+T1 (eax, r11)

        pop     rbx
        ret
 %else
        push    rbx

        mov     r10, rcx                ; pEFlags
        mov     r11, rdx                ; pu64EbxEcx (is also T1)

        mov     ebx, [r11]
        mov     ecx, [r11 + 4]
        IEM_MAYBE_LOAD_FLAGS r10, (X86_EFL_ZF), 0 ; clobbers T0 (eax)
        mov     eax, [rsi]
        mov     edx, [rsi + 4]

        lock cmpxchg8b [rdi]

        mov     [rsi], eax
        mov     [rsi + 4], edx
        IEM_SAVE_FLAGS       r10, (X86_EFL_ZF), 0 ; clobbers T0+T1 (eax, r11)

        pop     rbx
        ret

 %endif
%else
        push    esi
        push    edi
        push    ebx
        push    ebp

        mov     edi, ecx                ; pu64Dst
        mov     esi, edx                ; pu64EaxEdx
        mov     ecx, [esp + 16 + 4 + 0] ; pu64EbxEcx
        mov     ebp, [esp + 16 + 4 + 4] ; pEFlags

        mov     ebx, [ecx]
        mov     ecx, [ecx + 4]
        IEM_MAYBE_LOAD_FLAGS ebp, (X86_EFL_ZF), 0  ; clobbers T0 (eax)
        mov     eax, [esi]
        mov     edx, [esi + 4]

        lock cmpxchg8b [edi]

        mov     [esi], eax
        mov     [esi + 4], edx
        IEM_SAVE_FLAGS       ebp, (X86_EFL_ZF), 0 ; clobbers T0+T1 (eax, edi)

        pop     ebp
        pop     ebx
        pop     edi
        pop     esi
        ret     8
%endif
ENDPROC iemAImpl_cmpxchg8b

BEGINPROC_FASTCALL iemAImpl_cmpxchg8b_locked, 16
        ; Lazy bird always lock prefixes cmpxchg8b.
        jmp     NAME_FASTCALL(iemAImpl_cmpxchg8b,16,$@)
ENDPROC iemAImpl_cmpxchg8b_locked

%ifdef RT_ARCH_AMD64

;
; CMPXCHG16B.
;
; These are tricky register wise, so the code is duplicated for each calling
; convention.
;
; WARNING! This code make ASSUMPTIONS about which registers T1 and T0 are mapped to!
;
; C-proto:
; IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b,(PRTUINT128U pu128Dst, PRTUINT128U pu1284RaxRdx, PRTUINT128U pu128RbxRcx,
;                                              uint32_t *pEFlags));
;
; Note! Identical to iemAImpl_cmpxchg8b.
;
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_cmpxchg16b, 16
 %ifdef ASM_CALL64_MSC
        push    rbx

        mov     r11, rdx                ; pu64RaxRdx (is also T1)
        mov     r10, rcx                ; pu64Dst

        mov     rbx, [r8]
        mov     rcx, [r8 + 8]
        IEM_MAYBE_LOAD_FLAGS r9, (X86_EFL_ZF), 0 ; clobbers T0 (eax)
        mov     rax, [r11]
        mov     rdx, [r11 + 8]

        lock cmpxchg16b [r10]

        mov     [r11], rax
        mov     [r11 + 8], rdx
        IEM_SAVE_FLAGS       r9, (X86_EFL_ZF), 0 ; clobbers T0+T1 (eax, r11)

        pop     rbx
        ret
 %else
        push    rbx

        mov     r10, rcx                ; pEFlags
        mov     r11, rdx                ; pu64RbxRcx (is also T1)

        mov     rbx, [r11]
        mov     rcx, [r11 + 8]
        IEM_MAYBE_LOAD_FLAGS r10, (X86_EFL_ZF), 0 ; clobbers T0 (eax)
        mov     rax, [rsi]
        mov     rdx, [rsi + 8]

        lock cmpxchg16b [rdi]

        mov     [rsi], rax
        mov     [rsi + 8], rdx
        IEM_SAVE_FLAGS       r10, (X86_EFL_ZF), 0 ; clobbers T0+T1 (eax, r11)

        pop     rbx
        ret

 %endif
ENDPROC iemAImpl_cmpxchg16b

BEGINPROC_FASTCALL iemAImpl_cmpxchg16b_locked, 16
        ; Lazy bird always lock prefixes cmpxchg16b.
        jmp     NAME_FASTCALL(iemAImpl_cmpxchg16b,16,$@)
ENDPROC iemAImpl_cmpxchg16b_locked

%endif ; RT_ARCH_AMD64


;
; CMPXCHG.
;
; WARNING! This code make ASSUMPTIONS about which registers T1 and T0 are mapped to!
;
; C-proto:
; IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg,(uintX_t *puXDst, uintX_t puEax, uintX_t uReg, uint32_t *pEFlags));
;
BEGINCODE
%macro IEMIMPL_CMPXCHG 2
BEGINPROC_FASTCALL iemAImpl_cmpxchg_u8 %+ %2, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0 (eax)
        mov     al, [A1]
        %1 cmpxchg [A0], A2_8
        mov     [A1], al
        IEM_SAVE_FLAGS       A3, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0+T1 (eax, r11/edi)
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_cmpxchg_u8 %+ %2

BEGINPROC_FASTCALL iemAImpl_cmpxchg_u16 %+ %2, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0 (eax)
        mov     ax, [A1]
        %1 cmpxchg [A0], A2_16
        mov     [A1], ax
        IEM_SAVE_FLAGS       A3, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0+T1 (eax, r11/edi)
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_cmpxchg_u16 %+ %2

BEGINPROC_FASTCALL iemAImpl_cmpxchg_u32 %+ %2, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0 (eax)
        mov     eax, [A1]
        %1 cmpxchg [A0], A2_32
        mov     [A1], eax
        IEM_SAVE_FLAGS       A3, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0+T1 (eax, r11/edi)
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_cmpxchg_u32 %+ %2

BEGINPROC_FASTCALL iemAImpl_cmpxchg_u64 %+ %2, 16
%ifdef RT_ARCH_AMD64
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0 (eax)
        mov     rax, [A1]
        %1 cmpxchg [A0], A2
        mov     [A1], rax
        IEM_SAVE_FLAGS       A3, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0+T1 (eax, r11/edi)
        EPILOGUE_4_ARGS
%else
        ;
        ; Must use cmpxchg8b here. See also iemAImpl_cmpxchg8b.
        ;
        push    esi
        push    edi
        push    ebx
        push    ebp

        mov     edi, ecx                ; pu64Dst
        mov     esi, edx                ; pu64Rax
        mov     ecx, [esp + 16 + 4 + 0] ; pu64Reg - Note! Pointer on 32-bit hosts!
        mov     ebp, [esp + 16 + 4 + 4] ; pEFlags

        mov     ebx, [ecx]
        mov     ecx, [ecx + 4]
        IEM_MAYBE_LOAD_FLAGS ebp, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0  ; clobbers T0 (eax)
        mov     eax, [esi]
        mov     edx, [esi + 4]

        lock cmpxchg8b [edi]

        ; cmpxchg8b doesn't set CF, PF, AF, SF and OF, so we have to do that.
        jz      .cmpxchg8b_not_equal
        cmp     eax, eax                ; just set the other flags.
.store:
        mov     [esi], eax
        mov     [esi + 4], edx
        IEM_SAVE_FLAGS       ebp, (X86_EFL_ZF | X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_SF | X86_EFL_OF), 0 ; clobbers T0+T1 (eax, edi)

        pop     ebp
        pop     ebx
        pop     edi
        pop     esi
        ret     8

.cmpxchg8b_not_equal:
        cmp     [esi + 4], edx          ;; @todo FIXME - verify 64-bit compare implementation
        jne     .store
        cmp     [esi], eax
        jmp     .store

%endif
ENDPROC iemAImpl_cmpxchg_u64 %+ %2
%endmacro ; IEMIMPL_CMPXCHG

IEMIMPL_CMPXCHG , ,
IEMIMPL_CMPXCHG lock, _locked

;;
; Macro for implementing a unary operator.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses with locked
; variants, except on 32-bit system where the 64-bit accesses requires hand
; coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the source register operand in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
%macro IEMIMPL_UNARY_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        %1      byte [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u8

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8_locked, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        lock %1 byte [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u8_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        %1      word [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16_locked, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        lock %1 word [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16_locked

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        %1      dword [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32_locked, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        lock %1 dword [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32_locked

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        %1      qword [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64_locked, 8
        PROLOGUE_2_ARGS
        IEM_MAYBE_LOAD_FLAGS A1, %2, %3
        lock %1 qword [A0]
        IEM_SAVE_FLAGS       A1, %2, %3
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64_locked
 %endif ; RT_ARCH_AMD64

%endmacro

IEMIMPL_UNARY_OP inc, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF), 0
IEMIMPL_UNARY_OP dec, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF), 0
IEMIMPL_UNARY_OP neg, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0
IEMIMPL_UNARY_OP not, 0, 0


;
; BSWAP. No flag changes.
;
; Each function takes one argument, pointer to the value to bswap
; (input/output). They all return void.
;
BEGINPROC_FASTCALL iemAImpl_bswap_u16, 4
        PROLOGUE_1_ARGS
        mov     T0_32, [A0]             ; just in case any of the upper bits are used.
        db 66h
        bswap   T0_32
        mov     [A0], T0_32
        EPILOGUE_1_ARGS
ENDPROC iemAImpl_bswap_u16

BEGINPROC_FASTCALL iemAImpl_bswap_u32, 4
        PROLOGUE_1_ARGS
        mov     T0_32, [A0]
        bswap   T0_32
        mov     [A0], T0_32
        EPILOGUE_1_ARGS
ENDPROC iemAImpl_bswap_u32

BEGINPROC_FASTCALL iemAImpl_bswap_u64, 4
%ifdef RT_ARCH_AMD64
        PROLOGUE_1_ARGS
        mov     T0, [A0]
        bswap   T0
        mov     [A0], T0
        EPILOGUE_1_ARGS
%else
        PROLOGUE_1_ARGS
        mov     T0, [A0]
        mov     T1, [A0 + 4]
        bswap   T0
        bswap   T1
        mov     [A0 + 4], T0
        mov     [A0], T1
        EPILOGUE_1_ARGS
%endif
ENDPROC iemAImpl_bswap_u64


;;
; Macro for implementing a shift operation.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses, except on
; 32-bit system where the 64-bit accesses requires hand coding.
;
; All the functions takes a pointer to the destination memory operand in A0,
; the shift count in A1 and a pointer to eflags in A2.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
; Makes ASSUMPTIONS about A0, A1 and A2 assignments.
;
; @note the _intel and _amd variants are implemented in C.
;
%macro IEMIMPL_SHIFT_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     cl, A1_8
        %1      byte [A0], cl
 %else
        xchg    A1, A0
        %1      byte [A1], cl
 %endif
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u8

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     cl, A1_8
        %1      word [A0], cl
 %else
        xchg    A1, A0
        %1      word [A1], cl
 %endif
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     cl, A1_8
        %1      dword [A0], cl
 %else
        xchg    A1, A0
        %1      dword [A1], cl
 %endif
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
  %ifdef ASM_CALL64_GCC
        mov     cl, A1_8
        %1      qword [A0], cl
  %else
        xchg    A1, A0
        %1      qword [A1], cl
  %endif
        IEM_SAVE_FLAGS       A2, %2, %3
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %endif ; RT_ARCH_AMD64

%endmacro

IEMIMPL_SHIFT_OP rol, (X86_EFL_OF | X86_EFL_CF), 0
IEMIMPL_SHIFT_OP ror, (X86_EFL_OF | X86_EFL_CF), 0
IEMIMPL_SHIFT_OP rcl, (X86_EFL_OF | X86_EFL_CF), 0
IEMIMPL_SHIFT_OP rcr, (X86_EFL_OF | X86_EFL_CF), 0
IEMIMPL_SHIFT_OP shl, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)
IEMIMPL_SHIFT_OP shr, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)
IEMIMPL_SHIFT_OP sar, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)


;;
; Macro for implementing a double precision shift operation.
;
; This will generate code for the 16, 32 and 64 bit accesses, except on
; 32-bit system where the 64-bit accesses requires hand coding.
;
; The functions takes the destination operand (r/m) in A0, the source (reg) in
; A1, the shift count in A2 and a pointer to the eflags variable/register in A3.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
;
; Makes ASSUMPTIONS about A0, A1, A2 and A3 assignments.
;
; @note the _intel and _amd variants are implemented in C.
;
%macro IEMIMPL_SHIFT_DBL_OP 3
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
 %ifdef ASM_CALL64_GCC
        xchg    A3, A2
        %1      [A0], A1_16, cl
        xchg    A3, A2
 %else
        xchg    A0, A2
        %1      [A2], A1_16, cl
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
 %ifdef ASM_CALL64_GCC
        xchg    A3, A2
        %1      [A0], A1_32, cl
        xchg    A3, A2
 %else
        xchg    A0, A2
        %1      [A2], A1_32, cl
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32

 %ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 20
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
 %ifdef ASM_CALL64_GCC
        xchg    A3, A2
        %1      [A0], A1, cl
        xchg    A3, A2
 %else
        xchg    A0, A2
        %1      [A2], A1, cl
 %endif
        IEM_SAVE_FLAGS       A3, %2, %3
        EPILOGUE_4_ARGS_EX 12
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %endif ; RT_ARCH_AMD64

%endmacro

IEMIMPL_SHIFT_DBL_OP shld, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)
IEMIMPL_SHIFT_DBL_OP shrd, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_PF | X86_EFL_CF), (X86_EFL_AF)


;;
; Macro for implementing a multiplication operations.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses, except on
; 32-bit system where the 64-bit accesses requires hand coding.
;
; The 8-bit function only operates on AX, so it takes no DX pointer.  The other
; functions takes a pointer to rAX in A0, rDX in A1, the operand in A2 and a
; pointer to eflags in A3.
;
; The functions all return 0 so the caller can be used for div/idiv as well as
; for the mul/imul implementation.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
; @param        4       Name suffix.
; @param        5       EFLAGS behaviour: 0 for native, 1 for intel and 2 for AMD.
;
; Makes ASSUMPTIONS about A0, A1, A2, A3, T0 and T1 assignments.
;
%macro IEMIMPL_MUL_OP 5
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8 %+ %4, 12
        PROLOGUE_3_ARGS
        IEM_MAYBE_LOAD_FLAGS                     A2, %2, %3
        mov     al, [A0]
        %1      A1_8
        mov     [A0], ax
 %if %5 != 1
        IEM_SAVE_FLAGS                           A2, %2, %3
 %else
        IEM_SAVE_FLAGS_ADJUST_AND_CALC_SF_PF     A2, %2, X86_EFL_AF | X86_EFL_ZF, ax, 8, xAX
 %endif
        xor     eax, eax
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u8 %+ %4

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16 %+ %4, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS                     A3, %2, %3
        mov     ax, [A0]
 %ifdef ASM_CALL64_GCC
        %1      A2_16
        mov     [A0], ax
        mov     [A1], dx
 %else
        mov     T1, A1
        %1      A2_16
        mov     [A0], ax
        mov     [T1], dx
 %endif
 %if %5 != 1
        IEM_SAVE_FLAGS                           A3, %2, %3
 %else
        IEM_SAVE_FLAGS_ADJUST_AND_CALC_SF_PF     A3, %2, X86_EFL_AF | X86_EFL_ZF, ax, 16, xAX
 %endif
        xor     eax, eax
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u16 %+ %4

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32 %+ %4, 16
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS                     A3, %2, %3
        mov     eax, [A0]
 %ifdef ASM_CALL64_GCC
        %1      A2_32
        mov     [A0], eax
        mov     [A1], edx
 %else
        mov     T1, A1
        %1      A2_32
        mov     [A0], eax
        mov     [T1], edx
 %endif
 %if %5 != 1
        IEM_SAVE_FLAGS                           A3, %2, %3
 %else
        IEM_SAVE_FLAGS_ADJUST_AND_CALC_SF_PF     A3, %2, X86_EFL_AF | X86_EFL_ZF, eax, 32, xAX
 %endif
        xor     eax, eax
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u32 %+ %4

 %ifdef RT_ARCH_AMD64 ; The 32-bit host version lives in IEMAllAImplC.cpp.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64 %+ %4, 20
        PROLOGUE_4_ARGS
        IEM_MAYBE_LOAD_FLAGS                     A3, %2, %3
        mov     rax, [A0]
  %ifdef ASM_CALL64_GCC
        %1      A2
        mov     [A0], rax
        mov     [A1], rdx
  %else
        mov     T1, A1
        %1      A2
        mov     [A0], rax
        mov     [T1], rdx
  %endif
  %if %5 != 1
        IEM_SAVE_FLAGS                           A3, %2, %3
  %else
        IEM_SAVE_FLAGS_ADJUST_AND_CALC_SF_PF     A3, %2, X86_EFL_AF | X86_EFL_ZF, rax, 64, xAX
  %endif
        xor     eax, eax
        EPILOGUE_4_ARGS_EX 12
ENDPROC iemAImpl_ %+ %1 %+ _u64 %+ %4
 %endif ; !RT_ARCH_AMD64

%endmacro

IEMIMPL_MUL_OP mul,  (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF),       , 0
IEMIMPL_MUL_OP mul,  (X86_EFL_OF | X86_EFL_CF), 0,                                                   _intel, 1
IEMIMPL_MUL_OP mul,  (X86_EFL_OF | X86_EFL_CF), 0,                                                   _amd,   2
IEMIMPL_MUL_OP imul, (X86_EFL_OF | X86_EFL_CF), (X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF),       , 0
IEMIMPL_MUL_OP imul, (X86_EFL_OF | X86_EFL_CF), 0,                                                   _intel, 1
IEMIMPL_MUL_OP imul, (X86_EFL_OF | X86_EFL_CF), 0,                                                   _amd,   2


BEGINCODE
;;
; Worker function for negating a 32-bit number in T1:T0
; @uses None (T0,T1)
BEGINPROC   iemAImpl_negate_T0_T1_u32
        push    0
        push    0
        xchg    T0_32, [xSP]
        xchg    T1_32, [xSP + xCB]
        sub     T0_32, [xSP]
        sbb     T1_32, [xSP + xCB]
        add     xSP, xCB*2
        ret
ENDPROC     iemAImpl_negate_T0_T1_u32

%ifdef RT_ARCH_AMD64
;;
; Worker function for negating a 64-bit number in T1:T0
; @uses None (T0,T1)
BEGINPROC   iemAImpl_negate_T0_T1_u64
        push    0
        push    0
        xchg    T0, [xSP]
        xchg    T1, [xSP + xCB]
        sub     T0, [xSP]
        sbb     T1, [xSP + xCB]
        add     xSP, xCB*2
        ret
ENDPROC     iemAImpl_negate_T0_T1_u64
%endif


;;
; Macro for implementing a division operations.
;
; This will generate code for the 8, 16, 32 and 64 bit accesses, except on
; 32-bit system where the 64-bit accesses requires hand coding.
;
; The 8-bit function only operates on AX, so it takes no DX pointer.  The other
; functions takes a pointer to rAX in A0, rDX in A1, the operand in A2 and a
; pointer to eflags in A3.
;
; The functions all return 0 on success and -1 if a divide error should be
; raised by the caller.
;
; @param        1       The instruction mnemonic.
; @param        2       The modified flags.
; @param        3       The undefined flags.
; @param        4       1 if signed, 0 if unsigned.
; @param        5       Function suffix.
; @param        6       EFLAGS variation: 0 for native, 1 for intel (ignored),
;                       2 for AMD (set AF, clear PF, ZF and SF).
;
; Makes ASSUMPTIONS about A0, A1, A2, A3, T0 and T1 assignments.
;
%macro IEMIMPL_DIV_OP 6
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u8 %+ %5, 12
        PROLOGUE_3_ARGS

        ; div by chainsaw check.
        test    A1_8, A1_8
        jz      .div_zero

        ; Overflow check - unsigned division is simple to verify, haven't
        ; found a simple way to check signed division yet unfortunately.
 %if %4 == 0
        cmp     [A0 + 1], A1_8
        jae     .div_overflow
 %else
        mov     T0_16, [A0]             ; T0 = dividend
        mov     T1, A1                  ; T1 = saved divisor (because of missing T1_8 in 32-bit)
        test    A1_8, A1_8
        js      .divisor_negative
        test    T0_16, T0_16
        jns     .both_positive
        neg     T0_16
.one_of_each:                           ; OK range is 2^(result-with - 1) + (divisor - 1).
        push    T0                      ; Start off like unsigned below.
        shr     T0_16, 7
        cmp     T0_8, A1_8
        pop     T0
        jb      .div_no_overflow
        ja      .div_overflow
        and     T0_8, 0x7f              ; Special case for covering (divisor - 1).
        cmp     T0_8, A1_8
        jae     .div_overflow
        jmp     .div_no_overflow

.divisor_negative:
        neg     A1_8
        test    T0_16, T0_16
        jns     .one_of_each
        neg     T0_16
.both_positive:                         ; Same as unsigned shifted by sign indicator bit.
        shr     T0_16, 7
        cmp     T0_8, A1_8
        jae     .div_overflow
.div_no_overflow:
        mov     A1, T1                  ; restore divisor
 %endif

        IEM_MAYBE_LOAD_FLAGS A2, %2, %3
        mov     ax, [A0]
        %1      A1_8
        mov     [A0], ax
 %if %6 == 2 ; AMD64 3990X: Set AF and clear PF, ZF and SF.
        IEM_ADJUST_FLAGS    A2, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF, X86_EFL_AF
 %else
        IEM_SAVE_FLAGS      A2, %2, %3
 %endif
        xor     eax, eax

.return:
        EPILOGUE_3_ARGS

.div_zero:
.div_overflow:
        mov     eax, -1
        jmp     .return
ENDPROC iemAImpl_ %+ %1 %+ _u8 %+ %5

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u16 %+ %5, 16
        PROLOGUE_4_ARGS

        ; div by chainsaw check.
        test    A2_16, A2_16
        jz      .div_zero

        ; Overflow check - unsigned division is simple to verify, haven't
        ; found a simple way to check signed division yet unfortunately.
 %if %4 == 0
        cmp     [A1], A2_16
        jae     .div_overflow
 %else
        mov     T0_16, [A1]
        shl     T0_32, 16
        mov     T0_16, [A0]              ; T0 = dividend
        mov     T1, A2                   ; T1 = divisor
        test    T1_16, T1_16
        js      .divisor_negative
        test    T0_32, T0_32
        jns     .both_positive
        neg     T0_32
.one_of_each:                           ; OK range is 2^(result-with - 1) + (divisor - 1).
        push    T0                      ; Start off like unsigned below.
        shr     T0_32, 15
        cmp     T0_16, T1_16
        pop     T0
        jb      .div_no_overflow
        ja      .div_overflow
        and     T0_16, 0x7fff           ; Special case for covering (divisor - 1).
        cmp     T0_16, T1_16
        jae     .div_overflow
        jmp     .div_no_overflow

.divisor_negative:
        neg     T1_16
        test    T0_32, T0_32
        jns     .one_of_each
        neg     T0_32
.both_positive:                         ; Same as unsigned shifted by sign indicator bit.
        shr     T0_32, 15
        cmp     T0_16, T1_16
        jae     .div_overflow
.div_no_overflow:
 %endif

        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
 %ifdef ASM_CALL64_GCC
        mov     T1, A2
        mov     ax, [A0]
        mov     dx, [A1]
        %1      T1_16
        mov     [A0], ax
        mov     [A1], dx
 %else
        mov     T1, A1
        mov     ax, [A0]
        mov     dx, [T1]
        %1      A2_16
        mov     [A0], ax
        mov     [T1], dx
 %endif
 %if %6 == 2 ; AMD64 3990X: Set AF and clear PF, ZF and SF.
        IEM_ADJUST_FLAGS    A3, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF, X86_EFL_AF
 %else
        IEM_SAVE_FLAGS      A3, %2, %3
 %endif
        xor     eax, eax

.return:
        EPILOGUE_4_ARGS

.div_zero:
.div_overflow:
        mov     eax, -1
        jmp     .return
ENDPROC iemAImpl_ %+ %1 %+ _u16 %+ %5

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u32 %+ %5, 16
        PROLOGUE_4_ARGS

        ; div by chainsaw check.
        test    A2_32, A2_32
        jz      .div_zero

        ; Overflow check - unsigned division is simple to verify, haven't
        ; found a simple way to check signed division yet unfortunately.
 %if %4 == 0
        cmp     [A1], A2_32
        jae     .div_overflow
 %else
        push    A2                      ; save A2 so we modify it (we out of regs on x86).
        mov     T0_32, [A0]             ; T0 = dividend low
        mov     T1_32, [A1]             ; T1 = dividend high
        test    A2_32, A2_32
        js      .divisor_negative
        test    T1_32, T1_32
        jns     .both_positive
        call    NAME(iemAImpl_negate_T0_T1_u32)
.one_of_each:                           ; OK range is 2^(result-with - 1) + (divisor - 1).
        push    T0                      ; Start off like unsigned below.
        shl     T1_32, 1
        shr     T0_32, 31
        or      T1_32, T0_32
        cmp     T1_32, A2_32
        pop     T0
        jb      .div_no_overflow
        ja      .div_overflow
        and     T0_32, 0x7fffffff       ; Special case for covering (divisor - 1).
        cmp     T0_32, A2_32
        jae     .div_overflow
        jmp     .div_no_overflow

.divisor_negative:
        neg     A2_32
        test    T1_32, T1_32
        jns     .one_of_each
        call    NAME(iemAImpl_negate_T0_T1_u32)
.both_positive:                         ; Same as unsigned shifted by sign indicator bit.
        shl     T1_32, 1
        shr     T0_32, 31
        or      T1_32, T0_32
        cmp     T1_32, A2_32
        jae     .div_overflow
.div_no_overflow:
        pop     A2
 %endif

        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
        mov     eax, [A0]
 %ifdef ASM_CALL64_GCC
        mov     T1, A2
        mov     eax, [A0]
        mov     edx, [A1]
        %1      T1_32
        mov     [A0], eax
        mov     [A1], edx
 %else
        mov     T1, A1
        mov     eax, [A0]
        mov     edx, [T1]
        %1      A2_32
        mov     [A0], eax
        mov     [T1], edx
 %endif
 %if %6 == 2 ; AMD64 3990X: Set AF and clear PF, ZF and SF.
        IEM_ADJUST_FLAGS    A3, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF, X86_EFL_AF
 %else
        IEM_SAVE_FLAGS      A3, %2, %3
 %endif
        xor     eax, eax

.return:
        EPILOGUE_4_ARGS

.div_overflow:
 %if %4 != 0
        pop     A2
 %endif
.div_zero:
        mov     eax, -1
        jmp     .return
ENDPROC iemAImpl_ %+ %1 %+ _u32 %+ %5

 %ifdef RT_ARCH_AMD64 ; The 32-bit host version lives in IEMAllAImplC.cpp.
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64 %+ %5, 20
        PROLOGUE_4_ARGS

        test    A2, A2
        jz      .div_zero
  %if %4 == 0
        cmp     [A1], A2
        jae     .div_overflow
  %else
        push    A2                      ; save A2 so we modify it (we out of regs on x86).
        mov     T0, [A0]                ; T0 = dividend low
        mov     T1, [A1]                ; T1 = dividend high
        test    A2, A2
        js      .divisor_negative
        test    T1, T1
        jns     .both_positive
        call    NAME(iemAImpl_negate_T0_T1_u64)
.one_of_each:                           ; OK range is 2^(result-with - 1) + (divisor - 1).
        push    T0                      ; Start off like unsigned below.
        shl     T1, 1
        shr     T0, 63
        or      T1, T0
        cmp     T1, A2
        pop     T0
        jb      .div_no_overflow
        ja      .div_overflow
        mov     T1, 0x7fffffffffffffff
        and     T0, T1                  ; Special case for covering (divisor - 1).
        cmp     T0, A2
        jae     .div_overflow
        jmp     .div_no_overflow

.divisor_negative:
        neg     A2
        test    T1, T1
        jns     .one_of_each
        call    NAME(iemAImpl_negate_T0_T1_u64)
.both_positive:                         ; Same as unsigned shifted by sign indicator bit.
        shl     T1, 1
        shr     T0, 63
        or      T1, T0
        cmp     T1, A2
        jae     .div_overflow
.div_no_overflow:
        pop     A2
  %endif

        IEM_MAYBE_LOAD_FLAGS A3, %2, %3
        mov     rax, [A0]
  %ifdef ASM_CALL64_GCC
        mov     T1, A2
        mov     rax, [A0]
        mov     rdx, [A1]
        %1      T1
        mov     [A0], rax
        mov     [A1], rdx
  %else
        mov     T1, A1
        mov     rax, [A0]
        mov     rdx, [T1]
        %1      A2
        mov     [A0], rax
        mov     [T1], rdx
  %endif
  %if %6 == 2 ; AMD64 3990X: Set AF and clear PF, ZF and SF.
        IEM_ADJUST_FLAGS    A3, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF, X86_EFL_AF
  %else
        IEM_SAVE_FLAGS      A3, %2, %3
  %endif
        xor     eax, eax

.return:
        EPILOGUE_4_ARGS_EX 12

.div_overflow:
  %if %4 != 0
        pop     A2
  %endif
.div_zero:
        mov     eax, -1
        jmp     .return
ENDPROC iemAImpl_ %+ %1 %+ _u64 %+ %5
 %endif ; !RT_ARCH_AMD64

%endmacro

IEMIMPL_DIV_OP div,  0, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 0,       , 0
IEMIMPL_DIV_OP div,  0, 0,                                                                             0, _intel, 1
IEMIMPL_DIV_OP div,  0, 0,                                                                             0, _amd,   2
IEMIMPL_DIV_OP idiv, 0, (X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF), 1,       , 0
IEMIMPL_DIV_OP idiv, 0, 0,                                                                             1, _intel, 1
IEMIMPL_DIV_OP idiv, 0, 0,                                                                             1, _amd,   2


;;
; Macro for implementing memory fence operation.
;
; No return value, no operands or anything.
;
; @param        1      The instruction.
;
%macro IEMIMPL_MEM_FENCE 1
BEGINCODE
BEGINPROC_FASTCALL iemAImpl_ %+ %1, 0
        %1
        ret
ENDPROC iemAImpl_ %+ %1
%endmacro

IEMIMPL_MEM_FENCE lfence
IEMIMPL_MEM_FENCE sfence
IEMIMPL_MEM_FENCE mfence

;;
; Alternative for non-SSE2 host.
;
BEGINPROC_FASTCALL iemAImpl_alt_mem_fence, 0
        push    xAX
        xchg    xAX, [xSP]
        add     xSP, xCB
        ret
ENDPROC iemAImpl_alt_mem_fence


;;
; Initialize the FPU for the actual instruction being emulated, this means
; loading parts of the guest's control word and status word.
;
; @uses     24 bytes of stack. T0, T1
; @param    1       Expression giving the address of the FXSTATE of the guest.
;
%macro FPU_LD_FXSTATE_FCW_AND_SAFE_FSW 1
        fnstenv [xSP]

        ; FCW - for exception, precision and rounding control.
        movzx   T0, word [%1 + X86FXSTATE.FCW]
        and     T0, X86_FCW_MASK_ALL | X86_FCW_PC_MASK | X86_FCW_RC_MASK
        mov     [xSP + X86FSTENV32P.FCW], T0_16

        ; FSW - for undefined C0, C1, C2, and C3.
        movzx   T1, word [%1 + X86FXSTATE.FSW]
        and     T1, X86_FSW_C_MASK
        movzx   T0, word [xSP + X86FSTENV32P.FSW]
        and     T0, X86_FSW_TOP_MASK
        or      T0, T1
        mov     [xSP + X86FSTENV32P.FSW], T0_16

        fldenv  [xSP]
%endmacro


;;
; Initialize the FPU for the actual instruction being emulated, this means
; loading parts of the guest's control word, status word, and update the
; tag word for the top register if it's empty.
;
; ASSUMES actual TOP=7
;
; @uses     24 bytes of stack.  T0, T1
; @param    1       Expression giving the address of the FXSTATE of the guest.
;
%macro FPU_LD_FXSTATE_FCW_AND_SAFE_FSW_AND_FTW_0 1
        fnstenv [xSP]

        ; FCW - for exception, precision and rounding control.
        movzx   T0_32, word [%1 + X86FXSTATE.FCW]
        and     T0_32, X86_FCW_MASK_ALL | X86_FCW_PC_MASK | X86_FCW_RC_MASK
        mov     [xSP + X86FSTENV32P.FCW], T0_16

        ; FSW - for undefined C0, C1, C2, and C3.
        movzx   T1_32, word [%1 + X86FXSTATE.FSW]
        and     T1_32, X86_FSW_C_MASK
        movzx   T0_32, word [xSP + X86FSTENV32P.FSW]
        and     T0_32, X86_FSW_TOP_MASK
        or      T0_32, T1_32
        mov     [xSP + X86FSTENV32P.FSW], T0_16

        ; FTW - Only for ST0 (in/out).
        movzx   T1_32, word [%1 + X86FXSTATE.FSW]
        shr     T1_32, X86_FSW_TOP_SHIFT
        and     T1_32, X86_FSW_TOP_SMASK
        bt      [%1 + X86FXSTATE.FTW], T1_16     ; Empty if FTW bit is clear.  Fixed register order.
        jc      %%st0_not_empty
        or      word [xSP + X86FSTENV32P.FTW], 0c000h ; TOP=7, so set TAG(7)=3
%%st0_not_empty:

        fldenv  [xSP]
%endmacro


;;
; Need to move this as well somewhere better?
;
struc IEMFPURESULT
    .r80Result  resw 5
    .FSW        resw 1
endstruc


;;
; Need to move this as well somewhere better?
;
struc IEMFPURESULTTWO
    .r80Result1 resw 5
    .FSW        resw 1
    .r80Result2 resw 5
endstruc


;
;---------------------- 16-bit signed integer operations ----------------------
;


;;
; Converts a 16-bit floating point value to a 80-bit one (fpu register).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 16-bit floating point value to convert.
;
BEGINPROC_FASTCALL iemAImpl_fild_r80_from_i16, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fild    word [A2]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_fild_r80_from_i16


;;
; Store a 80-bit floating point value (register) as a 16-bit signed integer (memory).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 16-bit signed integer value.
; @param    A3      Pointer to the 80-bit value.
;
BEGINPROC_FASTCALL iemAImpl_fist_r80_to_i16, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fistp   word [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fist_r80_to_i16


;;
; Store a 80-bit floating point value (register) as a 16-bit signed integer
; (memory) with truncation.
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 16-bit signed integer value.
; @param    A3      Pointer to the 80-bit value.
;
BEGINPROC_FASTCALL iemAImpl_fistt_r80_to_i16, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fisttp  word [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fistt_r80_to_i16


;;
; FPU instruction working on one 80-bit and one 16-bit signed integer value.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 80-bit value.
; @param    A3      Pointer to the 16-bit value.
;
%macro IEMIMPL_FPU_R80_BY_I16 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_i16, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      word [A3]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_i16
%endmacro

IEMIMPL_FPU_R80_BY_I16 fiadd
IEMIMPL_FPU_R80_BY_I16 fimul
IEMIMPL_FPU_R80_BY_I16 fisub
IEMIMPL_FPU_R80_BY_I16 fisubr
IEMIMPL_FPU_R80_BY_I16 fidiv
IEMIMPL_FPU_R80_BY_I16 fidivr


;;
; FPU instruction working on one 80-bit and one 16-bit signed integer value,
; only returning FSW.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to store the output FSW.
; @param    A2      Pointer to the 80-bit value.
; @param    A3      Pointer to the 64-bit value.
;
%macro IEMIMPL_FPU_R80_BY_I16_FSW 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_i16, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      word [A3]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_i16
%endmacro

IEMIMPL_FPU_R80_BY_I16_FSW ficom



;
;---------------------- 32-bit signed integer operations ----------------------
;


;;
; Converts a 32-bit floating point value to a 80-bit one (fpu register).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 32-bit floating point value to convert.
;
BEGINPROC_FASTCALL iemAImpl_fild_r80_from_i32, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fild    dword [A2]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_fild_r80_from_i32


;;
; Store a 80-bit floating point value (register) as a 32-bit signed integer (memory).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 32-bit signed integer value.
; @param    A3      Pointer to the 80-bit value.
;
BEGINPROC_FASTCALL iemAImpl_fist_r80_to_i32, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fistp   dword [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fist_r80_to_i32


;;
; Store a 80-bit floating point value (register) as a 32-bit signed integer
; (memory) with truncation.
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 32-bit signed integer value.
; @param    A3      Pointer to the 80-bit value.
;
BEGINPROC_FASTCALL iemAImpl_fistt_r80_to_i32, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fisttp  dword [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fistt_r80_to_i32


;;
; FPU instruction working on one 80-bit and one 32-bit signed integer value.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 80-bit value.
; @param    A3      Pointer to the 32-bit value.
;
%macro IEMIMPL_FPU_R80_BY_I32 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_i32, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      dword [A3]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_i32
%endmacro

IEMIMPL_FPU_R80_BY_I32 fiadd
IEMIMPL_FPU_R80_BY_I32 fimul
IEMIMPL_FPU_R80_BY_I32 fisub
IEMIMPL_FPU_R80_BY_I32 fisubr
IEMIMPL_FPU_R80_BY_I32 fidiv
IEMIMPL_FPU_R80_BY_I32 fidivr


;;
; FPU instruction working on one 80-bit and one 32-bit signed integer value,
; only returning FSW.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to store the output FSW.
; @param    A2      Pointer to the 80-bit value.
; @param    A3      Pointer to the 64-bit value.
;
%macro IEMIMPL_FPU_R80_BY_I32_FSW 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_i32, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      dword [A3]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_i32
%endmacro

IEMIMPL_FPU_R80_BY_I32_FSW ficom



;
;---------------------- 64-bit signed integer operations ----------------------
;


;;
; Converts a 64-bit floating point value to a 80-bit one (fpu register).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 64-bit floating point value to convert.
;
BEGINPROC_FASTCALL iemAImpl_fild_r80_from_i64, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fild    qword [A2]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_fild_r80_from_i64


;;
; Store a 80-bit floating point value (register) as a 64-bit signed integer (memory).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 64-bit signed integer value.
; @param    A3      Pointer to the 80-bit value.
;
BEGINPROC_FASTCALL iemAImpl_fist_r80_to_i64, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fistp   qword [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fist_r80_to_i64


;;
; Store a 80-bit floating point value (register) as a 64-bit signed integer
; (memory) with truncation.
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 64-bit signed integer value.
; @param    A3      Pointer to the 80-bit value.
;
BEGINPROC_FASTCALL iemAImpl_fistt_r80_to_i64, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fisttp  qword [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fistt_r80_to_i64



;
;---------------------- 32-bit floating point operations ----------------------
;

;;
; Converts a 32-bit floating point value to a 80-bit one (fpu register).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 32-bit floating point value to convert.
;
BEGINPROC_FASTCALL iemAImpl_fld_r80_from_r32, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fld     dword [A2]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_fld_r80_from_r32


;;
; Store a 80-bit floating point value (register) as a 32-bit one (memory).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 32-bit value.
; @param    A3      Pointer to the 80-bit value.
;
BEGINPROC_FASTCALL iemAImpl_fst_r80_to_r32, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fst     dword [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fst_r80_to_r32


;;
; FPU instruction working on one 80-bit and one 32-bit floating point value.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 80-bit value.
; @param    A3      Pointer to the 32-bit value.
;
%macro IEMIMPL_FPU_R80_BY_R32 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_r32, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      dword [A3]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_r32
%endmacro

IEMIMPL_FPU_R80_BY_R32 fadd
IEMIMPL_FPU_R80_BY_R32 fmul
IEMIMPL_FPU_R80_BY_R32 fsub
IEMIMPL_FPU_R80_BY_R32 fsubr
IEMIMPL_FPU_R80_BY_R32 fdiv
IEMIMPL_FPU_R80_BY_R32 fdivr


;;
; FPU instruction working on one 80-bit and one 32-bit floating point value,
; only returning FSW.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to store the output FSW.
; @param    A2      Pointer to the 80-bit value.
; @param    A3      Pointer to the 64-bit value.
;
%macro IEMIMPL_FPU_R80_BY_R32_FSW 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_r32, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      dword [A3]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_r32
%endmacro

IEMIMPL_FPU_R80_BY_R32_FSW fcom



;
;---------------------- 64-bit floating point operations ----------------------
;

;;
; Converts a 64-bit floating point value to a 80-bit one (fpu register).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 64-bit floating point value to convert.
;
BEGINPROC_FASTCALL iemAImpl_fld_r80_from_r64, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fld     qword [A2]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_fld_r80_from_r64


;;
; Store a 80-bit floating point value (register) as a 64-bit one (memory).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 64-bit value.
; @param    A3      Pointer to the 80-bit value.
;
BEGINPROC_FASTCALL iemAImpl_fst_r80_to_r64, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fst     qword [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fst_r80_to_r64


;;
; FPU instruction working on one 80-bit and one 64-bit floating point value.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 80-bit value.
; @param    A3      Pointer to the 64-bit value.
;
%macro IEMIMPL_FPU_R80_BY_R64 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_r64, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      qword [A3]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_r64
%endmacro

IEMIMPL_FPU_R80_BY_R64 fadd
IEMIMPL_FPU_R80_BY_R64 fmul
IEMIMPL_FPU_R80_BY_R64 fsub
IEMIMPL_FPU_R80_BY_R64 fsubr
IEMIMPL_FPU_R80_BY_R64 fdiv
IEMIMPL_FPU_R80_BY_R64 fdivr

;;
; FPU instruction working on one 80-bit and one 64-bit floating point value,
; only returning FSW.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to store the output FSW.
; @param    A2      Pointer to the 80-bit value.
; @param    A3      Pointer to the 64-bit value.
;
%macro IEMIMPL_FPU_R80_BY_R64_FSW 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_r64, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      qword [A3]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_r64
%endmacro

IEMIMPL_FPU_R80_BY_R64_FSW fcom



;
;---------------------- 80-bit floating point operations ----------------------
;

;;
; Loads a 80-bit floating point register value from memory.
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 80-bit floating point value to load.
;
BEGINPROC_FASTCALL iemAImpl_fld_r80_from_r80, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fld     tword [A2]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_fld_r80_from_r80


;;
; Store a 80-bit floating point register to memory
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 80-bit value.
; @param    A3      Pointer to the 80-bit register value.
;
BEGINPROC_FASTCALL iemAImpl_fst_r80_to_r80, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fstp    tword [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fst_r80_to_r80


;;
; Loads an 80-bit floating point register value in BCD format from memory.
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 80-bit BCD value to load.
;
BEGINPROC_FASTCALL iemAImpl_fld_r80_from_d80, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fbld    tword [A2]

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_fld_r80_from_d80


;;
; Store a 80-bit floating point register to memory as BCD
;
; @param    A0      FPU context (fxsave).
; @param    A1      Where to return the output FSW.
; @param    A2      Where to store the 80-bit BCD value.
; @param    A3      Pointer to the 80-bit register value.
;
BEGINPROC_FASTCALL iemAImpl_fst_r80_to_d80, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        fbstp   tword [A2]

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_fst_r80_to_d80


;;
; FPU instruction working on two 80-bit floating point values.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the first 80-bit value (ST0)
; @param    A3      Pointer to the second 80-bit value (STn).
;
%macro IEMIMPL_FPU_R80_BY_R80 2
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_r80, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      %2

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_r80
%endmacro

IEMIMPL_FPU_R80_BY_R80 fadd,   {st0, st1}
IEMIMPL_FPU_R80_BY_R80 fmul,   {st0, st1}
IEMIMPL_FPU_R80_BY_R80 fsub,   {st0, st1}
IEMIMPL_FPU_R80_BY_R80 fsubr,  {st0, st1}
IEMIMPL_FPU_R80_BY_R80 fdiv,   {st0, st1}
IEMIMPL_FPU_R80_BY_R80 fdivr,  {st0, st1}
IEMIMPL_FPU_R80_BY_R80 fprem,  {}
IEMIMPL_FPU_R80_BY_R80 fprem1, {}
IEMIMPL_FPU_R80_BY_R80 fscale, {}


;;
; FPU instruction working on two 80-bit floating point values, ST1 and ST0,
; storing the result in ST1 and popping the stack.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the first 80-bit value (ST1).
; @param    A3      Pointer to the second 80-bit value (ST0).
;
%macro IEMIMPL_FPU_R80_BY_R80_ST1_ST0_POP 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_r80, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        fld     tword [A3]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_r80
%endmacro

IEMIMPL_FPU_R80_BY_R80_ST1_ST0_POP fpatan
IEMIMPL_FPU_R80_BY_R80_ST1_ST0_POP fyl2x
IEMIMPL_FPU_R80_BY_R80_ST1_ST0_POP fyl2xp1


;;
; FPU instruction working on two 80-bit floating point values, only
; returning FSW.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a uint16_t for the resulting FSW.
; @param    A2      Pointer to the first 80-bit value.
; @param    A3      Pointer to the second 80-bit value.
;
%macro IEMIMPL_FPU_R80_BY_R80_FSW 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_r80, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      st0, st1

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_r80
%endmacro

IEMIMPL_FPU_R80_BY_R80_FSW fcom
IEMIMPL_FPU_R80_BY_R80_FSW fucom


;;
; FPU instruction working on two 80-bit floating point values,
; returning FSW and EFLAGS (eax).
;
; @param    1       The instruction
;
; @returns  EFLAGS in EAX.
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a uint16_t for the resulting FSW.
; @param    A2      Pointer to the first 80-bit value.
; @param    A3      Pointer to the second 80-bit value.
;
%macro IEMIMPL_FPU_R80_BY_R80_EFL 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_by_r80, 16
        PROLOGUE_4_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A3]
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1      st1

        fnstsw  word  [A1]
        pushf
        pop     xAX

        fninit
        add     xSP, 20h
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_by_r80
%endmacro

IEMIMPL_FPU_R80_BY_R80_EFL fcomi
IEMIMPL_FPU_R80_BY_R80_EFL fucomi


;;
; FPU instruction working on one 80-bit floating point value.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
; @param    A2      Pointer to the 80-bit value.
;
%macro IEMIMPL_FPU_R80 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80
%endmacro

IEMIMPL_FPU_R80 fchs
IEMIMPL_FPU_R80 fabs
IEMIMPL_FPU_R80 f2xm1
IEMIMPL_FPU_R80 fsqrt
IEMIMPL_FPU_R80 frndint
IEMIMPL_FPU_R80 fsin
IEMIMPL_FPU_R80 fcos


;;
; FPU instruction working on one 80-bit floating point value, only
; returning FSW.
;
; @param    1       The instruction
; @param    2       Non-zero to also restore FTW.
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a uint16_t for the resulting FSW.
; @param    A2      Pointer to the 80-bit value.
;
%macro IEMIMPL_FPU_R80_FSW 2
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
%if %2 != 0
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW_AND_FTW_0 A0
%else
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
%endif
        %1

        fnstsw  word  [A1]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80
%endmacro

IEMIMPL_FPU_R80_FSW ftst, 0
IEMIMPL_FPU_R80_FSW fxam, 1 ; No #IS or any other FP exceptions.



;;
; FPU instruction loading a 80-bit floating point constant.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULT for the output.
;
%macro IEMIMPL_FPU_R80_CONST 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1, 8
        PROLOGUE_2_ARGS
        sub     xSP, 20h

        fninit
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1

        fnstsw  word  [A1 + IEMFPURESULT.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULT.r80Result]

        fninit
        add     xSP, 20h
        EPILOGUE_2_ARGS
ENDPROC iemAImpl_ %+ %1 %+
%endmacro

IEMIMPL_FPU_R80_CONST fld1
IEMIMPL_FPU_R80_CONST fldl2t
IEMIMPL_FPU_R80_CONST fldl2e
IEMIMPL_FPU_R80_CONST fldpi
IEMIMPL_FPU_R80_CONST fldlg2
IEMIMPL_FPU_R80_CONST fldln2
IEMIMPL_FPU_R80_CONST fldz


;;
; FPU instruction working on one 80-bit floating point value, outputing two.
;
; @param    1       The instruction
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to a IEMFPURESULTTWO for the output.
; @param    A2      Pointer to the 80-bit value.
;
%macro IEMIMPL_FPU_R80_R80 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _r80_r80, 12
        PROLOGUE_3_ARGS
        sub     xSP, 20h

        fninit
        fld     tword [A2]
        FPU_LD_FXSTATE_FCW_AND_SAFE_FSW A0
        %1

        fnstsw  word  [A1 + IEMFPURESULTTWO.FSW]
        fnclex
        fstp    tword [A1 + IEMFPURESULTTWO.r80Result2]
        fnclex
        fstp    tword [A1 + IEMFPURESULTTWO.r80Result1]

        fninit
        add     xSP, 20h
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _r80_r80
%endmacro

IEMIMPL_FPU_R80_R80 fptan
IEMIMPL_FPU_R80_R80 fxtract
IEMIMPL_FPU_R80_R80 fsincos




;---------------------- SSE and MMX Operations ----------------------

;; @todo what do we need to do for MMX?
%macro IEMIMPL_MMX_PROLOGUE 0
%endmacro
%macro IEMIMPL_MMX_EPILOGUE 0
%endmacro

;; @todo what do we need to do for SSE?
%macro IEMIMPL_SSE_PROLOGUE 0
%endmacro
%macro IEMIMPL_SSE_EPILOGUE 0
%endmacro

;; @todo what do we need to do for AVX?
%macro IEMIMPL_AVX_PROLOGUE 0
%endmacro
%macro IEMIMPL_AVX_EPILOGUE 0
%endmacro


;;
; Media instruction working on two full sized registers.
;
; @param    1       The instruction
; @param    2       Whether there is an MMX variant (1) or not (0).
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to the first media register size operand (input/output).
; @param    A2      Pointer to the second media register size operand (input).
;
%macro IEMIMPL_MEDIA_F2 2
%if %2 != 0
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 12
        PROLOGUE_3_ARGS
        IEMIMPL_MMX_PROLOGUE

        movq    mm0, [A1]
        movq    mm1, [A2]
        %1      mm0, mm1
        movq    [A1], mm0

        IEMIMPL_MMX_EPILOGUE
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64
%endif

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u128, 12
        PROLOGUE_3_ARGS
        IEMIMPL_SSE_PROLOGUE

        movdqu   xmm0, [A1]
        movdqu   xmm1, [A2]
        %1       xmm0, xmm1
        movdqu   [A1], xmm0

        IEMIMPL_SSE_EPILOGUE
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u128
%endmacro

IEMIMPL_MEDIA_F2 pand,    1
IEMIMPL_MEDIA_F2 pandn,   1
IEMIMPL_MEDIA_F2 por,     1
IEMIMPL_MEDIA_F2 pxor,    1
IEMIMPL_MEDIA_F2 pcmpeqb, 1
IEMIMPL_MEDIA_F2 pcmpeqw, 1
IEMIMPL_MEDIA_F2 pcmpeqd, 1
IEMIMPL_MEDIA_F2 pcmpeqq, 0
IEMIMPL_MEDIA_F2 pcmpgtb, 1
IEMIMPL_MEDIA_F2 pcmpgtw, 1
IEMIMPL_MEDIA_F2 pcmpgtd, 1
IEMIMPL_MEDIA_F2 pcmpgtq, 0
IEMIMPL_MEDIA_F2 paddb,   1
IEMIMPL_MEDIA_F2 paddw,   1
IEMIMPL_MEDIA_F2 paddd,   1
IEMIMPL_MEDIA_F2 paddq,   1
IEMIMPL_MEDIA_F2 psubb,   1
IEMIMPL_MEDIA_F2 psubw,   1
IEMIMPL_MEDIA_F2 psubd,   1
IEMIMPL_MEDIA_F2 psubq,   1


;;
; Media instruction working on one full sized and one half sized register (lower half).
;
; @param    1       The instruction
; @param    2       1 if MMX is included, 0 if not.
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to the first full sized media register operand (input/output).
; @param    A2      Pointer to the second half sized media register operand (input).
;
%macro IEMIMPL_MEDIA_F1L1 2
 %if %2 != 0
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 12
        PROLOGUE_3_ARGS
        IEMIMPL_MMX_PROLOGUE

        movq    mm0, [A1]
        movd    mm1, [A2]
        %1      mm0, mm1
        movq    [A1], mm0

        IEMIMPL_MMX_EPILOGUE
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %endif

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u128, 12
        PROLOGUE_3_ARGS
        IEMIMPL_SSE_PROLOGUE

        movdqu   xmm0, [A1]
        movq     xmm1, [A2]
        %1       xmm0, xmm1
        movdqu   [A1], xmm0

        IEMIMPL_SSE_EPILOGUE
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u128
%endmacro

IEMIMPL_MEDIA_F1L1 punpcklbw,  1
IEMIMPL_MEDIA_F1L1 punpcklwd,  1
IEMIMPL_MEDIA_F1L1 punpckldq,  1
IEMIMPL_MEDIA_F1L1 punpcklqdq, 0


;;
; Media instruction working on one full sized and one half sized register (high half).
;
; @param    1       The instruction
; @param    2       1 if MMX is included, 0 if not.
;
; @param    A0      FPU context (fxsave).
; @param    A1      Pointer to the first full sized media register operand (input/output).
; @param    A2      Pointer to the second full sized media register operand, where we
;                   will only use the upper half (input).
;
%macro IEMIMPL_MEDIA_F1H1 2
 %if %2 != 0
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u64, 12
        PROLOGUE_3_ARGS
        IEMIMPL_MMX_PROLOGUE

        movq    mm0, [A1]
        movq    mm1, [A2]
        %1      mm0, mm1
        movq    [A1], mm0

        IEMIMPL_MMX_EPILOGUE
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u64
 %endif

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u128, 12
        PROLOGUE_3_ARGS
        IEMIMPL_SSE_PROLOGUE

        movdqu   xmm0, [A1]
        movdqu   xmm1, [A2]
        %1       xmm0, xmm1
        movdqu   [A1], xmm0

        IEMIMPL_SSE_EPILOGUE
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u128
%endmacro

IEMIMPL_MEDIA_F1L1 punpckhbw,  1
IEMIMPL_MEDIA_F1L1 punpckhwd,  1
IEMIMPL_MEDIA_F1L1 punpckhdq,  1
IEMIMPL_MEDIA_F1L1 punpckhqdq, 0


;
; Shufflers with evil 8-bit immediates.
;

BEGINPROC_FASTCALL iemAImpl_pshufw, 16
        PROLOGUE_4_ARGS
        IEMIMPL_MMX_PROLOGUE

        movq    mm0, [A1]
        movq    mm1, [A2]
        lea     T0, [A3 + A3*4]         ; sizeof(pshufw+ret) == 5
        lea     T1, [.imm0 xWrtRIP]
        lea     T1, [T1 + T0]
        call    T1
        movq    [A1], mm0

        IEMIMPL_MMX_EPILOGUE
        EPILOGUE_4_ARGS
%assign bImm 0
%rep 256
.imm %+ bImm:
       pshufw    mm0, mm1, bImm
       ret
 %assign bImm bImm + 1
%endrep
.immEnd:                                ; 256*5 == 0x500
dw 0xfaff  + (.immEnd - .imm0)          ; will cause warning if entries are too big.
dw 0x104ff - (.immEnd - .imm0)          ; will cause warning if entries are small big.
ENDPROC iemAImpl_pshufw


%macro IEMIMPL_MEDIA_SSE_PSHUFXX 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1, 16
        PROLOGUE_4_ARGS
        IEMIMPL_SSE_PROLOGUE

        movdqu  xmm0, [A1]
        movdqu  xmm1, [A2]
        lea     T1, [.imm0 xWrtRIP]
        lea     T0, [A3 + A3*2]         ; sizeof(pshufXX+ret) == 6: (A3 * 3) *2
        lea     T1, [T1 + T0*2]
        call    T1
        movdqu  [A1], xmm0

        IEMIMPL_SSE_EPILOGUE
        EPILOGUE_4_ARGS
 %assign bImm 0
 %rep 256
.imm %+ bImm:
       %1       xmm0, xmm1, bImm
       ret
  %assign bImm bImm + 1
 %endrep
.immEnd:                                ; 256*6 == 0x600
dw 0xf9ff  + (.immEnd - .imm0)          ; will cause warning if entries are too big.
dw 0x105ff - (.immEnd - .imm0)          ; will cause warning if entries are small big.
ENDPROC iemAImpl_ %+ %1
%endmacro

IEMIMPL_MEDIA_SSE_PSHUFXX pshufhw
IEMIMPL_MEDIA_SSE_PSHUFXX pshuflw
IEMIMPL_MEDIA_SSE_PSHUFXX pshufd


;
; Move byte mask.
;

BEGINPROC_FASTCALL iemAImpl_pmovmskb_u64, 12
        PROLOGUE_3_ARGS
        IEMIMPL_MMX_PROLOGUE

        mov     T0,  [A1]
        movq    mm1, [A2]
        pmovmskb T0, mm1
        mov     [A1], T0
%ifdef RT_ARCH_X86
        mov     dword [A1 + 4], 0
%endif
        IEMIMPL_MMX_EPILOGUE
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_pmovmskb_u64

BEGINPROC_FASTCALL iemAImpl_pmovmskb_u128, 12
        PROLOGUE_3_ARGS
        IEMIMPL_SSE_PROLOGUE

        mov     T0,  [A1]
        movdqu  xmm1, [A2]
        pmovmskb T0, xmm1
        mov     [A1], T0
%ifdef RT_ARCH_X86
        mov     dword [A1 + 4], 0
%endif
        IEMIMPL_SSE_EPILOGUE
        EPILOGUE_3_ARGS
ENDPROC iemAImpl_pmovmskb_u128


;;
; Media instruction working on two full sized source registers and one destination (AVX).
;
; @param    1       The instruction
;
; @param    A0      Pointer to the extended CPU/FPU state (X86XSAVEAREA).
; @param    A1      Pointer to the destination media register size operand (output).
; @param    A2      Pointer to the first source media register size operand (input).
; @param    A3      Pointer to the second source media register size operand (input).
;
%macro IEMIMPL_MEDIA_F3 1
BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u128, 16
        PROLOGUE_4_ARGS
        IEMIMPL_AVX_PROLOGUE

        vmovdqu  xmm0, [A2]
        vmovdqu  xmm1, [A3]
        %1       xmm0, xmm0, xmm1
        vmovdqu  [A1], xmm0

        IEMIMPL_AVX_PROLOGUE
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u128

BEGINPROC_FASTCALL iemAImpl_ %+ %1 %+ _u256, 16
        PROLOGUE_4_ARGS
        IEMIMPL_SSE_PROLOGUE

        vmovdqu  ymm0, [A2]
        vmovdqu  ymm1, [A3]
        %1       ymm0, ymm0, ymm1
        vmovdqu  [A1], ymm0

        IEMIMPL_AVX_PROLOGUE
        EPILOGUE_4_ARGS
ENDPROC iemAImpl_ %+ %1 %+ _u256
%endmacro

IEMIMPL_MEDIA_F3 vpand
IEMIMPL_MEDIA_F3 vpandn
IEMIMPL_MEDIA_F3 vpor
IEMIMPL_MEDIA_F3 vpxor
IEMIMPL_MEDIA_F3 vpcmpeqb
IEMIMPL_MEDIA_F3 vpcmpeqw
IEMIMPL_MEDIA_F3 vpcmpeqd
IEMIMPL_MEDIA_F3 vpcmpeqq
IEMIMPL_MEDIA_F3 vpcmpgtb
IEMIMPL_MEDIA_F3 vpcmpgtw
IEMIMPL_MEDIA_F3 vpcmpgtd
IEMIMPL_MEDIA_F3 vpcmpgtq
IEMIMPL_MEDIA_F3 vpaddb
IEMIMPL_MEDIA_F3 vpaddw
IEMIMPL_MEDIA_F3 vpaddd
IEMIMPL_MEDIA_F3 vpaddq
IEMIMPL_MEDIA_F3 vpsubb
IEMIMPL_MEDIA_F3 vpsubw
IEMIMPL_MEDIA_F3 vpsubd
IEMIMPL_MEDIA_F3 vpsubq


;
; The SSE 4.2 crc32
;
; @param    1       The instruction
;
; @param    A1      Pointer to the 32-bit destination.
; @param    A2      The source operand, sized according to the suffix.
;

BEGINPROC_FASTCALL iemAImpl_crc32_u8, 8
        PROLOGUE_2_ARGS

        mov     T0_32, [A0]
        crc32   T0_32, A1_8
        mov     [A0], T0_32

        EPILOGUE_2_ARGS
ENDPROC iemAImpl_crc32_u8

BEGINPROC_FASTCALL iemAImpl_crc32_u16, 8
        PROLOGUE_2_ARGS

        mov     T0_32, [A0]
        crc32   T0_32, A1_16
        mov     [A0], T0_32

        EPILOGUE_2_ARGS
ENDPROC iemAImpl_crc32_u16

BEGINPROC_FASTCALL iemAImpl_crc32_u32, 8
        PROLOGUE_2_ARGS

        mov     T0_32, [A0]
        crc32   T0_32, A1_32
        mov     [A0], T0_32

        EPILOGUE_2_ARGS
ENDPROC iemAImpl_crc32_u32

%ifdef RT_ARCH_AMD64
BEGINPROC_FASTCALL iemAImpl_crc32_u64, 8
        PROLOGUE_2_ARGS

        mov     T0_32, [A0]
        crc32   T0, A1
        mov     [A0], T0_32

        EPILOGUE_2_ARGS
ENDPROC iemAImpl_crc32_u64
%endif

