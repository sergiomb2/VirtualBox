/* $Id$ */
/** @file
 * DevVMWare - VMWare SVGA device - VGPU10+ (DX) shader utilities.
 */

/*
 * Copyright (C) 2020-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VMSVGA
#include <VBox/AssertGuest.h>
#include <VBox/log.h>

#include <iprt/asm.h>
#include <iprt/md5.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include "DevVGA-SVGA3d-dx-shader.h"


/*
 *
 * DXBC shader binary format definitions.
 *
 */

/* DXBC container header. */
typedef struct DXBCHeader
{
    uint32_t u32DXBC;                   /* 0x43425844 = 'D', 'X', 'B', 'C' */
    uint8_t  au8Hash[16];               /* Modified MD5 hash. See dxbcHash. */
    uint32_t u32Version;                /* 1 */
    uint32_t cbTotal;                   /* Total size in bytes. Including the header. */
    uint32_t cBlob;                     /* Number of entries in aBlobOffset array. */
    uint32_t aBlobOffset[1];            /* Offsets of blobs from the start of DXBC header. */
} DXBCHeader;

#define DXBC_MAGIC RT_MAKE_U32_FROM_U8('D', 'X', 'B', 'C')

/* DXBC blob header. */
typedef struct DXBCBlobHeader
{
    uint32_t u32BlobType;               /* FourCC code. DXBC_BLOB_TYPE_* */
    uint32_t cbBlob;                    /* Size of the blob excluding the blob header. 4 bytes aligned. */
    /* Followed by the blob's data. */
} DXBCBlobHeader;

/* DXBC blob types. */
#define DXBC_BLOB_TYPE_ISGN RT_MAKE_U32_FROM_U8('I', 'S', 'G', 'N')
#define DXBC_BLOB_TYPE_OSGN RT_MAKE_U32_FROM_U8('O', 'S', 'G', 'N')
#define DXBC_BLOB_TYPE_SHDR RT_MAKE_U32_FROM_U8('S', 'H', 'D', 'R')
/** @todo More... */

/* 'SHDR' blob data format. */
typedef struct DXBCBlobSHDR
{
    VGPU10ProgramToken programToken;
    uint32_t cToken;                    /* Number of 32 bit tokens including programToken and cToken. */
    uint32_t au32Token[1];              /* cToken - 2 number of tokens. */
} DXBCBlobSHDR;

/* Element of an input or output signature. */
typedef struct DXBCBlobIOSGNElement
{
    uint32_t offElementName;            /* Offset of the semantic's name relative to the start of the blob data. */
    uint32_t idxSemantic;               /* Semantic index. */
    uint32_t enmSystemValue;            /* SVGA3dDXSignatureSemanticName */
    uint32_t enmComponentType;          /* 1 - unsigned, 2 - integer, 3 - float. */
    uint32_t idxRegister;               /* Shader register index. Elements must be sorted by register index. */
    uint32_t mask  : 8;                 /* Component mask. Lower 4 bits represent X, Y, Z, W channels. */
    uint32_t mask2 : 8;                 /* Which components are used in the shader. */
    uint32_t pad : 16;
} DXBCBlobIOSGNElement;

/* 'ISGN' and 'OSGN' blob data format. */
typedef struct DXBCBlobIOSGN
{
    uint32_t cElement;                  /* Number of signature elements. */
    uint32_t offElement;                /* Offset of the first element from the start of the blob. Equals to 8. */
    DXBCBlobIOSGNElement aElement[1];   /* Signature elements. Size is cElement. */
    /* Followed by ASCIIZ semantic names. */
} DXBCBlobIOSGN;


/*
 * VGPU10 shader parser definitions.
 */

/* Parsed info about an operand index. */
typedef struct VGPUOperandIndex
{
    uint32_t indexRepresentation;       /* VGPU10_OPERAND_INDEX_REPRESENTATION */
    uint64_t aOperandIndex[2];          /* Needs up to 2 qwords. */
} VGPUOperandIndex;

/* Parsed info about an operand. */
typedef struct VGPUOperand
{
    uint32_t numComponents   : 2;       /* VGPU10_OPERAND_NUM_COMPONENTS */
    uint32_t selectionMode   : 2;       /* VGPU10_OPERAND_4_COMPONENT_SELECTION_MODE */
    uint32_t mask            : 4;       /* 4-bits X, Y, Z, W mask for VGPU10_OPERAND_4_COMPONENT_MASK_MODE. */
    uint32_t operandType     : 8;       /* VGPU10_OPERAND_TYPE */
    uint32_t indexDimension  : 2;       /* VGPU10_OPERAND_INDEX_DIMENSION */
    VGPUOperandIndex aOperandIndex[VGPU10_OPERAND_INDEX_3D]; /* Up to 3. */
    uint32_t aImm[4];                   /* Immediate values for VGPU10_OPERAND_TYPE_IMMEDIATE* */
} VGPUOperand;

/* Parsed info about an opcode. */
typedef struct VGPUOpcode
{
    uint32_t cOpcodeToken;              /* Number of tokens for this operation. */
    uint32_t opcodeType;                /* VGPU10_OPCODE_* */
    uint32_t semanticName;              /* SVGA3dDXSignatureSemanticName for system value declarations. */
    uint32_t cOperand;                  /* Nunber of operands. */
    VGPUOperand aOperand[8];            /* 8 should be enough for everyone. */
} VGPUOpcode;

typedef struct VGPUOpcodeInfo
{
    uint32_t cOperand;                  /* Number of operands for this opcode. */
} VGPUOpcodeInfo;

static VGPUOpcodeInfo const g_aOpcodeInfo[] =
{
    { 3 },                              /* VGPU10_OPCODE_ADD */
    { 3 },                              /* VGPU10_OPCODE_AND */
    { 0 },                              /* VGPU10_OPCODE_BREAK */
    { 1 },                              /* VGPU10_OPCODE_BREAKC */
    { 1 },                              /* VGPU10_OPCODE_CALL */
    { 2 },                              /* VGPU10_OPCODE_CALLC */
    { 1 },                              /* VGPU10_OPCODE_CASE */
    { 0 },                              /* VGPU10_OPCODE_CONTINUE */
    { 1 },                              /* VGPU10_OPCODE_CONTINUEC */
    { 0 },                              /* VGPU10_OPCODE_CUT */
    { 0 },                              /* VGPU10_OPCODE_DEFAULT */
    { 2 },                              /* VGPU10_OPCODE_DERIV_RTX */
    { 2 },                              /* VGPU10_OPCODE_DERIV_RTY */
    { 1 },                              /* VGPU10_OPCODE_DISCARD */
    { 3 },                              /* VGPU10_OPCODE_DIV */
    { 3 },                              /* VGPU10_OPCODE_DP2 */
    { 3 },                              /* VGPU10_OPCODE_DP3 */
    { 3 },                              /* VGPU10_OPCODE_DP4 */
    { 0 },                              /* VGPU10_OPCODE_ELSE */
    { 0 },                              /* VGPU10_OPCODE_EMIT */
    { 0 },                              /* VGPU10_OPCODE_EMITTHENCUT */
    { 0 },                              /* VGPU10_OPCODE_ENDIF */
    { 0 },                              /* VGPU10_OPCODE_ENDLOOP */
    { 0 },                              /* VGPU10_OPCODE_ENDSWITCH */
    { 3 },                              /* VGPU10_OPCODE_EQ */
    { 2 },                              /* VGPU10_OPCODE_EXP */
    { 2 },                              /* VGPU10_OPCODE_FRC */
    { 2 },                              /* VGPU10_OPCODE_FTOI */
    { 2 },                              /* VGPU10_OPCODE_FTOU */
    { 3 },                              /* VGPU10_OPCODE_GE */
    { 3 },                              /* VGPU10_OPCODE_IADD */
    { 1 },                              /* VGPU10_OPCODE_IF */
    { 3 },                              /* VGPU10_OPCODE_IEQ */
    { 3 },                              /* VGPU10_OPCODE_IGE */
    { 3 },                              /* VGPU10_OPCODE_ILT */
    { 4 },                              /* VGPU10_OPCODE_IMAD */
    { 3 },                              /* VGPU10_OPCODE_IMAX */
    { 3 },                              /* VGPU10_OPCODE_IMIN */
    { 4 },                              /* VGPU10_OPCODE_IMUL */
    { 3 },                              /* VGPU10_OPCODE_INE */
    { 2 },                              /* VGPU10_OPCODE_INEG */
    { 3 },                              /* VGPU10_OPCODE_ISHL */
    { 3 },                              /* VGPU10_OPCODE_ISHR */
    { 2 },                              /* VGPU10_OPCODE_ITOF */
    { 1 },                              /* VGPU10_OPCODE_LABEL */
    { 3 },                              /* VGPU10_OPCODE_LD */
    { 4 },                              /* VGPU10_OPCODE_LD_MS */
    { 2 },                              /* VGPU10_OPCODE_LOG */
    { 0 },                              /* VGPU10_OPCODE_LOOP */
    { 3 },                              /* VGPU10_OPCODE_LT */
    { 4 },                              /* VGPU10_OPCODE_MAD */
    { 3 },                              /* VGPU10_OPCODE_MIN */
    { 3 },                              /* VGPU10_OPCODE_MAX */
    { UINT32_MAX },                     /* VGPU10_OPCODE_CUSTOMDATA: special opcode */
    { 2 },                              /* VGPU10_OPCODE_MOV */
    { 4 },                              /* VGPU10_OPCODE_MOVC */
    { 3 },                              /* VGPU10_OPCODE_MUL */
    { 3 },                              /* VGPU10_OPCODE_NE */
    { 0 },                              /* VGPU10_OPCODE_NOP */
    { 2 },                              /* VGPU10_OPCODE_NOT */
    { 3 },                              /* VGPU10_OPCODE_OR */
    { 3 },                              /* VGPU10_OPCODE_RESINFO */
    { 0 },                              /* VGPU10_OPCODE_RET */
    { 1 },                              /* VGPU10_OPCODE_RETC */
    { 2 },                              /* VGPU10_OPCODE_ROUND_NE */
    { 2 },                              /* VGPU10_OPCODE_ROUND_NI */
    { 2 },                              /* VGPU10_OPCODE_ROUND_PI */
    { 2 },                              /* VGPU10_OPCODE_ROUND_Z */
    { 2 },                              /* VGPU10_OPCODE_RSQ */
    { 4 },                              /* VGPU10_OPCODE_SAMPLE */
    { 5 },                              /* VGPU10_OPCODE_SAMPLE_C */
    { 5 },                              /* VGPU10_OPCODE_SAMPLE_C_LZ */
    { 5 },                              /* VGPU10_OPCODE_SAMPLE_L */
    { 6 },                              /* VGPU10_OPCODE_SAMPLE_D */
    { 5 },                              /* VGPU10_OPCODE_SAMPLE_B */
    { 2 },                              /* VGPU10_OPCODE_SQRT */
    { 1 },                              /* VGPU10_OPCODE_SWITCH */
    { 3 },                              /* VGPU10_OPCODE_SINCOS */
    { 4 },                              /* VGPU10_OPCODE_UDIV */
    { 3 },                              /* VGPU10_OPCODE_ULT */
    { 3 },                              /* VGPU10_OPCODE_UGE */
    { 4 },                              /* VGPU10_OPCODE_UMUL */
    { 4 },                              /* VGPU10_OPCODE_UMAD */
    { 3 },                              /* VGPU10_OPCODE_UMAX */
    { 3 },                              /* VGPU10_OPCODE_UMIN */
    { 3 },                              /* VGPU10_OPCODE_USHR */
    { 2 },                              /* VGPU10_OPCODE_UTOF */
    { 3 },                              /* VGPU10_OPCODE_XOR */
    { 1 },                              /* VGPU10_OPCODE_DCL_RESOURCE */
    { 1 },                              /* VGPU10_OPCODE_DCL_CONSTANT_BUFFER */
    { 1 },                              /* VGPU10_OPCODE_DCL_SAMPLER */
    { 1 },                              /* VGPU10_OPCODE_DCL_INDEX_RANGE */
    { 0 },                              /* VGPU10_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY */
    { 0 },                              /* VGPU10_OPCODE_DCL_GS_INPUT_PRIMITIVE */
    { 0 },                              /* VGPU10_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT */
    { 1 },                              /* VGPU10_OPCODE_DCL_INPUT */
    { 1 },                              /* VGPU10_OPCODE_DCL_INPUT_SGV */
    { 1 },                              /* VGPU10_OPCODE_DCL_INPUT_SIV */
    { 1 },                              /* VGPU10_OPCODE_DCL_INPUT_PS */
    { 1 },                              /* VGPU10_OPCODE_DCL_INPUT_PS_SGV */
    { 1 },                              /* VGPU10_OPCODE_DCL_INPUT_PS_SIV */
    { 1 },                              /* VGPU10_OPCODE_DCL_OUTPUT */
    { 1 },                              /* VGPU10_OPCODE_DCL_OUTPUT_SGV */
    { 1 },                              /* VGPU10_OPCODE_DCL_OUTPUT_SIV */
    { 0 },                              /* VGPU10_OPCODE_DCL_TEMPS */
    { 0 },                              /* VGPU10_OPCODE_DCL_INDEXABLE_TEMP */
    { 0 },                              /* VGPU10_OPCODE_DCL_GLOBAL_FLAGS */
    { UINT32_MAX },                     /* VGPU10_OPCODE_VMWARE: special opcode */
    { 4 },                              /* VGPU10_OPCODE_LOD */
    { 4 },                              /* VGPU10_OPCODE_GATHER4 */
    { 3 },                              /* VGPU10_OPCODE_SAMPLE_POS */
    { 2 },                              /* VGPU10_OPCODE_SAMPLE_INFO */
    { UINT32_MAX },                     /* VGPU10_OPCODE_RESERVED1: special opcode */
    { 0 },                              /* VGPU10_OPCODE_HS_DECLS */
    { 0 },                              /* VGPU10_OPCODE_HS_CONTROL_POINT_PHASE */
    { 0 },                              /* VGPU10_OPCODE_HS_FORK_PHASE */
    { 0 },                              /* VGPU10_OPCODE_HS_JOIN_PHASE */
    { 1 },                              /* VGPU10_OPCODE_EMIT_STREAM */
    { 1 },                              /* VGPU10_OPCODE_CUT_STREAM */
    { 1 },                              /* VGPU10_OPCODE_EMITTHENCUT_STREAM */
    { 1 },                              /* VGPU10_OPCODE_INTERFACE_CALL */
    { 2 },                              /* VGPU10_OPCODE_BUFINFO */
    { 2 },                              /* VGPU10_OPCODE_DERIV_RTX_COARSE */
    { 2 },                              /* VGPU10_OPCODE_DERIV_RTX_FINE */
    { 2 },                              /* VGPU10_OPCODE_DERIV_RTY_COARSE */
    { 2 },                              /* VGPU10_OPCODE_DERIV_RTY_FINE */
    { 5 },                              /* VGPU10_OPCODE_GATHER4_C */
    { 5 },                              /* VGPU10_OPCODE_GATHER4_PO */
    { 6 },                              /* VGPU10_OPCODE_GATHER4_PO_C */
    { 2 },                              /* VGPU10_OPCODE_RCP */
    { 2 },                              /* VGPU10_OPCODE_F32TOF16 */
    { 2 },                              /* VGPU10_OPCODE_F16TOF32 */
    { 4 },                              /* VGPU10_OPCODE_UADDC */
    { 4 },                              /* VGPU10_OPCODE_USUBB */
    { 2 },                              /* VGPU10_OPCODE_COUNTBITS */
    { 2 },                              /* VGPU10_OPCODE_FIRSTBIT_HI */
    { 2 },                              /* VGPU10_OPCODE_FIRSTBIT_LO */
    { 2 },                              /* VGPU10_OPCODE_FIRSTBIT_SHI */
    { 4 },                              /* VGPU10_OPCODE_UBFE */
    { 4 },                              /* VGPU10_OPCODE_IBFE */
    { 5 },                              /* VGPU10_OPCODE_BFI */
    { 2 },                              /* VGPU10_OPCODE_BFREV */
    { 5 },                              /* VGPU10_OPCODE_SWAPC */
    { 1 },                              /* VGPU10_OPCODE_DCL_STREAM */
    { 0 },                              /* VGPU10_OPCODE_DCL_FUNCTION_BODY */
    { 0 },                              /* VGPU10_OPCODE_DCL_FUNCTION_TABLE */
    { 0 },                              /* VGPU10_OPCODE_DCL_INTERFACE */
    { 0 },                              /* VGPU10_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT */
    { 0 },                              /* VGPU10_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT */
    { 0 },                              /* VGPU10_OPCODE_DCL_TESS_DOMAIN */
    { 0 },                              /* VGPU10_OPCODE_DCL_TESS_PARTITIONING */
    { 0 },                              /* VGPU10_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE */
    { 0 },                              /* VGPU10_OPCODE_DCL_HS_MAX_TESSFACTOR */
    { 0 },                              /* VGPU10_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT */
    { 0 },                              /* VGPU10_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT */
    { 0 },                              /* VGPU10_OPCODE_DCL_THREAD_GROUP */
    { 1 },                              /* VGPU10_OPCODE_DCL_UAV_TYPED */
    { 1 },                              /* VGPU10_OPCODE_DCL_UAV_RAW */
    { 1 },                              /* VGPU10_OPCODE_DCL_UAV_STRUCTURED */
    { 1 },                              /* VGPU10_OPCODE_DCL_TGSM_RAW */
    { 1 },                              /* VGPU10_OPCODE_DCL_TGSM_STRUCTURED */
    { 1 },                              /* VGPU10_OPCODE_DCL_RESOURCE_RAW */
    { 1 },                              /* VGPU10_OPCODE_DCL_RESOURCE_STRUCTURED */
    { 3 },                              /* VGPU10_OPCODE_LD_UAV_TYPED */
    { 3 },                              /* VGPU10_OPCODE_STORE_UAV_TYPED */
    { 3 },                              /* VGPU10_OPCODE_LD_RAW */
    { 3 },                              /* VGPU10_OPCODE_STORE_RAW */
    { 4 },                              /* VGPU10_OPCODE_LD_STRUCTURED */
    { 4 },                              /* VGPU10_OPCODE_STORE_STRUCTURED */
    { 3 },                              /* VGPU10_OPCODE_ATOMIC_AND */
    { 3 },                              /* VGPU10_OPCODE_ATOMIC_OR */
    { 3 },                              /* VGPU10_OPCODE_ATOMIC_XOR */
    { 4 },                              /* VGPU10_OPCODE_ATOMIC_CMP_STORE */
    { 3 },                              /* VGPU10_OPCODE_ATOMIC_IADD */
    { 3 },                              /* VGPU10_OPCODE_ATOMIC_IMAX */
    { 3 },                              /* VGPU10_OPCODE_ATOMIC_IMIN */
    { 3 },                              /* VGPU10_OPCODE_ATOMIC_UMAX */
    { 3 },                              /* VGPU10_OPCODE_ATOMIC_UMIN */
    { 2 },                              /* VGPU10_OPCODE_IMM_ATOMIC_ALLOC */
    { 2 },                              /* VGPU10_OPCODE_IMM_ATOMIC_CONSUME */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_IADD */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_AND */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_OR */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_XOR */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_EXCH */
    { 5 },                              /* VGPU10_OPCODE_IMM_ATOMIC_CMP_EXCH */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_IMAX */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_IMIN */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_UMAX */
    { 4 },                              /* VGPU10_OPCODE_IMM_ATOMIC_UMIN */
    { 0 },                              /* VGPU10_OPCODE_SYNC */
    { 3 },                              /* VGPU10_OPCODE_DADD */
    { 3 },                              /* VGPU10_OPCODE_DMAX */
    { 3 },                              /* VGPU10_OPCODE_DMIN */
    { 3 },                              /* VGPU10_OPCODE_DMUL */
    { 3 },                              /* VGPU10_OPCODE_DEQ */
    { 3 },                              /* VGPU10_OPCODE_DGE */
    { 3 },                              /* VGPU10_OPCODE_DLT */
    { 3 },                              /* VGPU10_OPCODE_DNE */
    { 2 },                              /* VGPU10_OPCODE_DMOV */
    { 4 },                              /* VGPU10_OPCODE_DMOVC */
    { 2 },                              /* VGPU10_OPCODE_DTOF */
    { 2 },                              /* VGPU10_OPCODE_FTOD */
    { 3 },                              /* VGPU10_OPCODE_EVAL_SNAPPED */
    { 3 },                              /* VGPU10_OPCODE_EVAL_SAMPLE_INDEX */
    { 2 },                              /* VGPU10_OPCODE_EVAL_CENTROID */
    { 0 },                              /* VGPU10_OPCODE_DCL_GS_INSTANCE_COUNT */
    { 0 },                              /* VGPU10_OPCODE_ABORT */
    { 0 },                              /* VGPU10_OPCODE_DEBUG_BREAK */
    { 0 },                              /* VGPU10_OPCODE_RESERVED0 */
    { 3 },                              /* VGPU10_OPCODE_DDIV */
    { 4 },                              /* VGPU10_OPCODE_DFMA */
    { 2 },                              /* VGPU10_OPCODE_DRCP */
    { 4 },                              /* VGPU10_OPCODE_MSAD */
    { 2 },                              /* VGPU10_OPCODE_DTOI */
    { 2 },                              /* VGPU10_OPCODE_DTOU */
    { 2 },                              /* VGPU10_OPCODE_ITOD */
    { 2 },                              /* VGPU10_OPCODE_UTOD */
};
AssertCompile(RT_ELEMENTS(g_aOpcodeInfo) == VGPU10_NUM_OPCODES);

#ifdef LOG_ENABLED
/*
 *
 * Helpers to translate a VGPU10 shader constant to a string.
 *
 */

#define SVGA_CASE_ID2STR(idx) case idx: return #idx

static const char *dxbcOpcodeToString(uint32_t opcodeType)
{
    VGPU10_OPCODE_TYPE enm = (VGPU10_OPCODE_TYPE)opcodeType;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ADD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_AND);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_BREAK);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_BREAKC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_CALL);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_CALLC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_CASE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_CONTINUE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_CONTINUEC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_CUT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DEFAULT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DERIV_RTX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DERIV_RTY);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DISCARD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DIV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DP2);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DP3);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DP4);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ELSE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EMIT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EMITTHENCUT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ENDIF);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ENDLOOP);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ENDSWITCH);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EQ);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EXP);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_FRC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_FTOI);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_FTOU);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_GE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IADD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IF);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IEQ);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IGE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ILT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMAD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMAX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMUL);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_INE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_INEG);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ISHL);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ISHR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ITOF);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LABEL);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LD_MS);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LOG);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LOOP);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_MAD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_MIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_MAX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_CUSTOMDATA);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_MOV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_MOVC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_MUL);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_NE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_NOP);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_NOT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_OR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_RESINFO);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_RET);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_RETC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ROUND_NE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ROUND_NI);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ROUND_PI);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ROUND_Z);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_RSQ);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SAMPLE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SAMPLE_C);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SAMPLE_C_LZ);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SAMPLE_L);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SAMPLE_D);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SAMPLE_B);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SQRT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SWITCH);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SINCOS);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UDIV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ULT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UGE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UMUL);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UMAD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UMAX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UMIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_USHR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UTOF);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_XOR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_RESOURCE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_CONSTANT_BUFFER);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_SAMPLER);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INDEX_RANGE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_GS_INPUT_PRIMITIVE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INPUT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INPUT_SGV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INPUT_SIV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INPUT_PS);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INPUT_PS_SGV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INPUT_PS_SIV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_OUTPUT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_OUTPUT_SGV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_OUTPUT_SIV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_TEMPS);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INDEXABLE_TEMP);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_GLOBAL_FLAGS);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_VMWARE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LOD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_GATHER4);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SAMPLE_POS);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SAMPLE_INFO);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_RESERVED1);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_HS_DECLS);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_HS_CONTROL_POINT_PHASE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_HS_FORK_PHASE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_HS_JOIN_PHASE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EMIT_STREAM);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_CUT_STREAM);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EMITTHENCUT_STREAM);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_INTERFACE_CALL);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_BUFINFO);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DERIV_RTX_COARSE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DERIV_RTX_FINE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DERIV_RTY_COARSE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DERIV_RTY_FINE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_GATHER4_C);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_GATHER4_PO);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_GATHER4_PO_C);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_RCP);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_F32TOF16);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_F16TOF32);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UADDC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_USUBB);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_COUNTBITS);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_FIRSTBIT_HI);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_FIRSTBIT_LO);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_FIRSTBIT_SHI);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UBFE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IBFE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_BFI);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_BFREV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SWAPC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_STREAM);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_FUNCTION_BODY);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_FUNCTION_TABLE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INTERFACE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_TESS_DOMAIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_TESS_PARTITIONING);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_HS_MAX_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_THREAD_GROUP);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_UAV_TYPED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_UAV_RAW);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_UAV_STRUCTURED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_TGSM_RAW);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_TGSM_STRUCTURED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_RESOURCE_RAW);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_RESOURCE_STRUCTURED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LD_UAV_TYPED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_STORE_UAV_TYPED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LD_RAW);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_STORE_RAW);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_LD_STRUCTURED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_STORE_STRUCTURED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_AND);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_OR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_XOR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_CMP_STORE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_IADD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_IMAX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_IMIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_UMAX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ATOMIC_UMIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_ALLOC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_CONSUME);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_IADD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_AND);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_OR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_XOR);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_EXCH);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_CMP_EXCH);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_IMAX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_IMIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_UMAX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_IMM_ATOMIC_UMIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_SYNC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DADD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DMAX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DMIN);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DMUL);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DEQ);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DGE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DLT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DNE);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DMOV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DMOVC);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DTOF);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_FTOD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EVAL_SNAPPED);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EVAL_SAMPLE_INDEX);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_EVAL_CENTROID);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DCL_GS_INSTANCE_COUNT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ABORT);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DEBUG_BREAK);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_RESERVED0);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DDIV);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DFMA);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DRCP);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_MSAD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DTOI);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_DTOU);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_ITOD);
        SVGA_CASE_ID2STR(VGPU10_OPCODE_UTOD);
        SVGA_CASE_ID2STR(VGPU10_NUM_OPCODES);
    }
    return NULL;
}


static const char *dxbcShaderTypeToString(uint32_t value)
{
    VGPU10_PROGRAM_TYPE enm = (VGPU10_PROGRAM_TYPE)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_PIXEL_SHADER);
        SVGA_CASE_ID2STR(VGPU10_VERTEX_SHADER);
        SVGA_CASE_ID2STR(VGPU10_GEOMETRY_SHADER);
        SVGA_CASE_ID2STR(VGPU10_HULL_SHADER);
        SVGA_CASE_ID2STR(VGPU10_DOMAIN_SHADER);
        SVGA_CASE_ID2STR(VGPU10_COMPUTE_SHADER);
    }
    return NULL;
}


static const char *dxbcCustomDataClassToString(uint32_t value)
{
    VGPU10_CUSTOMDATA_CLASS enm = (VGPU10_CUSTOMDATA_CLASS)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_CUSTOMDATA_COMMENT);
        SVGA_CASE_ID2STR(VGPU10_CUSTOMDATA_DEBUGINFO);
        SVGA_CASE_ID2STR(VGPU10_CUSTOMDATA_OPAQUE);
        SVGA_CASE_ID2STR(VGPU10_CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER);
    }
    return NULL;
}


static const char *dxbcSystemNameToString(uint32_t value)
{
    VGPU10_SYSTEM_NAME enm = (VGPU10_SYSTEM_NAME)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_NAME_UNDEFINED);
        SVGA_CASE_ID2STR(VGPU10_NAME_POSITION);
        SVGA_CASE_ID2STR(VGPU10_NAME_CLIP_DISTANCE);
        SVGA_CASE_ID2STR(VGPU10_NAME_CULL_DISTANCE);
        SVGA_CASE_ID2STR(VGPU10_NAME_RENDER_TARGET_ARRAY_INDEX);
        SVGA_CASE_ID2STR(VGPU10_NAME_VIEWPORT_ARRAY_INDEX);
        SVGA_CASE_ID2STR(VGPU10_NAME_VERTEX_ID);
        SVGA_CASE_ID2STR(VGPU10_NAME_PRIMITIVE_ID);
        SVGA_CASE_ID2STR(VGPU10_NAME_INSTANCE_ID);
        SVGA_CASE_ID2STR(VGPU10_NAME_IS_FRONT_FACE);
        SVGA_CASE_ID2STR(VGPU10_NAME_SAMPLE_INDEX);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_TRI_INSIDE_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_LINE_DETAIL_TESSFACTOR);
        SVGA_CASE_ID2STR(VGPU10_NAME_FINAL_LINE_DENSITY_TESSFACTOR);
    }
    return NULL;
}


static const char *dxbcOperandTypeToString(uint32_t value)
{
    VGPU10_OPERAND_TYPE enm = (VGPU10_OPERAND_TYPE)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_TEMP);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_OUTPUT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INDEXABLE_TEMP);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_IMMEDIATE32);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_IMMEDIATE64);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_SAMPLER);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_RESOURCE);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_CONSTANT_BUFFER);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_IMMEDIATE_CONSTANT_BUFFER);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_LABEL);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_PRIMITIVEID);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_OUTPUT_DEPTH);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_NULL);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_RASTERIZER);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_OUTPUT_COVERAGE_MASK);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_STREAM);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_FUNCTION_BODY);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_FUNCTION_TABLE);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INTERFACE);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_FUNCTION_INPUT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_FUNCTION_OUTPUT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_FORK_INSTANCE_ID);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_JOIN_INSTANCE_ID);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_CONTROL_POINT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_OUTPUT_CONTROL_POINT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_PATCH_CONSTANT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_DOMAIN_POINT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_THIS_POINTER);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_UAV);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_THREAD_ID);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_THREAD_GROUP_ID);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_COVERAGE_MASK);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_INPUT_GS_INSTANCE_ID);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_TYPE_CYCLE_COUNTER);
        SVGA_CASE_ID2STR(VGPU10_NUM_OPERANDS);
    }
    return NULL;
}


static const char *dxbcOperandNumComponentsToString(uint32_t value)
{
    VGPU10_OPERAND_NUM_COMPONENTS enm = (VGPU10_OPERAND_NUM_COMPONENTS)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_OPERAND_0_COMPONENT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_1_COMPONENT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_4_COMPONENT);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_N_COMPONENT);
    }
    return NULL;
}


static const char *dxbcOperandComponentModeToString(uint32_t value)
{
    VGPU10_OPERAND_4_COMPONENT_SELECTION_MODE enm = (VGPU10_OPERAND_4_COMPONENT_SELECTION_MODE)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_OPERAND_4_COMPONENT_MASK_MODE);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_4_COMPONENT_SWIZZLE_MODE);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_4_COMPONENT_SELECT_1_MODE);
    }
    return NULL;
}


static const char *dxbcOperandComponentNameToString(uint32_t value)
{
    VGPU10_COMPONENT_NAME enm = (VGPU10_COMPONENT_NAME)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_COMPONENT_X);
        SVGA_CASE_ID2STR(VGPU10_COMPONENT_Y);
        SVGA_CASE_ID2STR(VGPU10_COMPONENT_Z);
        SVGA_CASE_ID2STR(VGPU10_COMPONENT_W);
    }
    return NULL;
}


static const char *dxbcOperandIndexDimensionToString(uint32_t value)
{
    VGPU10_OPERAND_INDEX_DIMENSION enm = (VGPU10_OPERAND_INDEX_DIMENSION)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_0D);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_1D);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_2D);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_3D);
    }
    return NULL;
}


static const char *dxbcOperandIndexRepresentationToString(uint32_t value)
{
    VGPU10_OPERAND_INDEX_REPRESENTATION enm = (VGPU10_OPERAND_INDEX_REPRESENTATION)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_IMMEDIATE32);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_IMMEDIATE64);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_RELATIVE);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE);
        SVGA_CASE_ID2STR(VGPU10_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE);
    }
    return NULL;
}


static const char *dxbcInterpolationModeToString(uint32_t value)
{
    VGPU10_INTERPOLATION_MODE enm = (VGPU10_INTERPOLATION_MODE)value;
    switch (enm)
    {
        SVGA_CASE_ID2STR(VGPU10_INTERPOLATION_UNDEFINED);
        SVGA_CASE_ID2STR(VGPU10_INTERPOLATION_CONSTANT);
        SVGA_CASE_ID2STR(VGPU10_INTERPOLATION_LINEAR);
        SVGA_CASE_ID2STR(VGPU10_INTERPOLATION_LINEAR_CENTROID);
        SVGA_CASE_ID2STR(VGPU10_INTERPOLATION_LINEAR_NOPERSPECTIVE);
        SVGA_CASE_ID2STR(VGPU10_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID);
        SVGA_CASE_ID2STR(VGPU10_INTERPOLATION_LINEAR_SAMPLE);
        SVGA_CASE_ID2STR(VGPU10_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE);
    }
    return NULL;
}
#endif /* LOG_ENABLED */

/*
 * MD5 from IPRT (alt-md5.cpp) for DXBC hash calculation.
 * DXBC hash function uses a different padding for the data, see dxbcHash.
 * Therefore RTMd5Final is not needed. Two functions have been renamed: dxbcRTMd5Update dxbcRTMd5Init.
 */


/* The four core functions - F1 is optimized somewhat */
/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))


/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
    ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )


/**
 * The core of the MD5 algorithm, this alters an existing MD5 hash to reflect
 * the addition of 16 longwords of new data.  RTMd5Update blocks the data and
 * converts bytes into longwords for this routine.
 */
static void rtMd5Transform(uint32_t buf[4], uint32_t const in[16])
{
    uint32_t a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    /*      fn, w, x, y, z, data,                 s) */
    MD5STEP(F1, a, b, c, d, in[ 0] + 0xd76aa478,  7);
    MD5STEP(F1, d, a, b, c, in[ 1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[ 2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[ 3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[ 4] + 0xf57c0faf,  7);
    MD5STEP(F1, d, a, b, c, in[ 5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[ 6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[ 7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[ 8] + 0x698098d8,  7);
    MD5STEP(F1, d, a, b, c, in[ 9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122,  7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[ 1] + 0xf61e2562,  5);
    MD5STEP(F2, d, a, b, c, in[ 6] + 0xc040b340,  9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[ 0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[ 5] + 0xd62f105d,  5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453,  9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[ 4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[ 9] + 0x21e1cde6,  5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6,  9);
    MD5STEP(F2, c, d, a, b, in[ 3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[ 8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905,  5);
    MD5STEP(F2, d, a, b, c, in[ 2] + 0xfcefa3f8,  9);
    MD5STEP(F2, c, d, a, b, in[ 7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[ 5] + 0xfffa3942,  4);
    MD5STEP(F3, d, a, b, c, in[ 8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[ 1] + 0xa4beea44,  4);
    MD5STEP(F3, d, a, b, c, in[ 4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[ 7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6,  4);
    MD5STEP(F3, d, a, b, c, in[ 0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[ 3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[ 6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[ 9] + 0xd9d4d039,  4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[ 2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[ 0] + 0xf4292244,  6);
    MD5STEP(F4, d, a, b, c, in[ 7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[ 5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3,  6);
    MD5STEP(F4, d, a, b, c, in[ 3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[ 1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[ 8] + 0x6fa87e4f,  6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[ 6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[ 4] + 0xf7537e82,  6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[ 2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[ 9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}


#ifdef RT_BIG_ENDIAN
/*
 * Note: this code is harmless on little-endian machines.
 */
static void rtMd5ByteReverse(uint32_t *buf, unsigned int longs)
{
    uint32_t t;
    do
    {
        t = *buf;
        t = RT_LE2H_U32(t);
        *buf = t;
        buf++;
    } while (--longs);
}
#else   /* little endian - do nothing */
# define rtMd5ByteReverse(buf, len) do { /* Nothing */ } while (0)
#endif


/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void dxbcRTMd5Init(PRTMD5CONTEXT pCtx)
{
    pCtx->AltPrivate.buf[0] = 0x67452301;
    pCtx->AltPrivate.buf[1] = 0xefcdab89;
    pCtx->AltPrivate.buf[2] = 0x98badcfe;
    pCtx->AltPrivate.buf[3] = 0x10325476;

    pCtx->AltPrivate.bits[0] = 0;
    pCtx->AltPrivate.bits[1] = 0;
}


/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
/** @todo Optimize this, because len is always a multiple of 64. */
static void dxbcRTMd5Update(PRTMD5CONTEXT pCtx, const void *pvBuf, size_t len)
{
    const uint8_t  *buf = (const uint8_t *)pvBuf;
    uint32_t        t;

    /* Update bitcount */
    t = pCtx->AltPrivate.bits[0];
    if ((pCtx->AltPrivate.bits[0] = t + ((uint32_t) len << 3)) < t)
    pCtx->AltPrivate.bits[1]++; /* Carry from low to high */
    pCtx->AltPrivate.bits[1] += (uint32_t)(len >> 29);

    t = (t >> 3) & 0x3f;        /* Bytes already in shsInfo->data */

    /* Handle any leading odd-sized chunks */
    if (t)
    {
        uint8_t *p = (uint8_t *) pCtx->AltPrivate.in + t;

        t = 64 - t;
        if (len < t)
        {
            memcpy(p, buf, len);
            return;
        }
        memcpy(p, buf, t);
        rtMd5ByteReverse(pCtx->AltPrivate.in, 16);
        rtMd5Transform(pCtx->AltPrivate.buf, pCtx->AltPrivate.in);
        buf += t;
        len -= t;
    }

    /* Process data in 64-byte chunks */
#ifndef RT_BIG_ENDIAN
    if (!((uintptr_t)buf & 0x3))
    {
        while (len >= 64) {
            rtMd5Transform(pCtx->AltPrivate.buf, (uint32_t const *)buf);
            buf += 64;
            len -= 64;
        }
    }
    else
#endif
    {
        while (len >= 64) {
            memcpy(pCtx->AltPrivate.in, buf, 64);
            rtMd5ByteReverse(pCtx->AltPrivate.in, 16);
            rtMd5Transform(pCtx->AltPrivate.buf, pCtx->AltPrivate.in);
            buf += 64;
            len -= 64;
        }
    }

    /* Handle any remaining bytes of data */
    memcpy(pCtx->AltPrivate.in, buf, len);
}


static void dxbcHash(void const *pvData, uint32_t cbData, uint8_t pabDigest[RTMD5HASHSIZE])
{
    size_t const kBlockSize = 64;
    uint8_t au8BlockBuffer[kBlockSize];

    static uint8_t const s_au8Padding[kBlockSize] =
    {
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    RTMD5CONTEXT Ctx;
    PRTMD5CONTEXT const pCtx = &Ctx;
    dxbcRTMd5Init(pCtx);

    uint8_t const *pu8Data = (uint8_t *)pvData;
    size_t cbRemaining = cbData;

    size_t const cbCompleteBlocks = cbData & ~ (kBlockSize - 1);
    dxbcRTMd5Update(pCtx, pu8Data, cbCompleteBlocks);
    pu8Data += cbCompleteBlocks;
    cbRemaining -= cbCompleteBlocks;

    /* Custom padding. */
    if (cbRemaining >= kBlockSize - 2 * sizeof(uint32_t))
    {
        /* Two additional blocks. */
        memcpy(&au8BlockBuffer[0],           pu8Data,      cbRemaining);
        memcpy(&au8BlockBuffer[cbRemaining], s_au8Padding, kBlockSize - cbRemaining);
        dxbcRTMd5Update(pCtx, au8BlockBuffer, kBlockSize);

        memset(&au8BlockBuffer[sizeof(uint32_t)], 0, kBlockSize - 2 * sizeof(uint32_t));
    }
    else
    {
        /* One additional block. */
        memcpy(&au8BlockBuffer[sizeof(uint32_t)],               pu8Data,      cbRemaining);
        memcpy(&au8BlockBuffer[sizeof(uint32_t) + cbRemaining], s_au8Padding, kBlockSize - cbRemaining - 2 * sizeof(uint32_t));
    }

    /* Set the first and last dwords of the last block. */
    *(uint32_t *)&au8BlockBuffer[0]                             = cbData << 3;
    *(uint32_t *)&au8BlockBuffer[kBlockSize - sizeof(uint32_t)] = (cbData << 1) | 1;
    dxbcRTMd5Update(pCtx, au8BlockBuffer, kBlockSize);

    AssertCompile(sizeof(pCtx->AltPrivate.buf) == RTMD5HASHSIZE);
    memcpy(pabDigest, pCtx->AltPrivate.buf, RTMD5HASHSIZE);
}


/*
 *
 * Shader token reader.
 *
 */

typedef struct DXBCTokenReader
{
    uint32_t const *pToken; /* Next token to read. */
    uint32_t cToken; /* How many tokens total. */
    uint32_t cRemainingToken; /* How many tokens remain. */
} DXBCTokenReader;


DECLINLINE(uint32_t) dxbcTokenReaderByteOffset(DXBCTokenReader *r)
{
    return (r->cToken - r->cRemainingToken) * 4;
}


DECLINLINE(uint32_t) dxbcTokenReaderRemaining(DXBCTokenReader *r)
{
    return r->cRemainingToken;
}


DECLINLINE(bool) dxbcTokenReaderCanRead(DXBCTokenReader *r, uint32_t cToken)
{
    return cToken <= r->cRemainingToken;
}


DECLINLINE(void) dxbcTokenReaderSkip(DXBCTokenReader *r, uint32_t cToken)
{
    AssertReturnVoid(r->cRemainingToken >= cToken);
    r->cRemainingToken -= cToken;
    r->pToken += cToken;
}


DECLINLINE(uint32_t) dxbcTokenReaderRead32(DXBCTokenReader *r)
{
    AssertReturn(r->cRemainingToken, 0);
    --r->cRemainingToken;
    return *(r->pToken++);
}


DECLINLINE(uint64_t) dxbcTokenReaderRead64(DXBCTokenReader *r)
{
    uint64_t const u64Low = dxbcTokenReaderRead32(r);
    uint64_t const u64High = dxbcTokenReaderRead32(r);
    return u64Low + (u64High << 32);
}


/*
 *
 * Byte writer.
 *
 */

typedef struct DXBCByteWriter
{
    uint8_t *pu8ByteCodeBegin; /* First byte of the buffer. */
    uint8_t *pu8ByteCodePtr;   /* Next free byte. */
    uint32_t cbAllocated;      /* How many bytes allocated in the buffer. */
    uint32_t cbRemaining;      /* How many bytes remain in the buffer. */
} DXBCByteWriter;


DECLINLINE(void *) dxbcByteWriterPtr(DXBCByteWriter *w)
{
    return w->pu8ByteCodePtr;
}


DECLINLINE(uint32_t) dxbcByteWriterSize(DXBCByteWriter *w)
{
    return (uint32_t)(w->pu8ByteCodePtr - w->pu8ByteCodeBegin);
}


DECLINLINE(void) dxbcByteWriterCommit(DXBCByteWriter *w, uint32_t cbCommit)
{
    Assert(cbCommit < w->cbRemaining);
    cbCommit = RT_MIN(cbCommit, w->cbRemaining);
    w->pu8ByteCodePtr += cbCommit;
    w->cbRemaining -= cbCommit;
}


DECLINLINE(bool) dxbcByteWriterCanWrite(DXBCByteWriter *w, uint32_t cbMore)
{
    if (cbMore <= w->cbRemaining)
        return true;

    /* Do not allow to allocate more than 2 * SVGA3D_MAX_SHADER_MEMORY_BYTES */
    uint32_t const cbMax = 2 * SVGA3D_MAX_SHADER_MEMORY_BYTES;
    AssertReturn(cbMore < cbMax && RT_ALIGN_32(cbMore, 4096) <= cbMax - w->cbAllocated, false);

    uint32_t cbNew = w->cbAllocated + RT_ALIGN_32(cbMore, 4096);
    void *pvNew = RTMemAllocZ(cbNew);
    if (!pvNew)
        return false;

    uint32_t const cbCurrent = dxbcByteWriterSize(w);
    memcpy(pvNew, w->pu8ByteCodeBegin, cbCurrent);
    RTMemFree(w->pu8ByteCodeBegin);

    w->pu8ByteCodeBegin = (uint8_t *)pvNew;
    w->pu8ByteCodePtr   = w->pu8ByteCodeBegin + cbCurrent;
    w->cbAllocated      = cbNew;
    w->cbRemaining      = cbNew - cbCurrent;

    return true;
}


DECLINLINE(bool) dxbcByteWriterInit(DXBCByteWriter *w, uint32_t cbInitial)
{
    RT_ZERO(*w);
    return dxbcByteWriterCanWrite(w, cbInitial);
}


DECLINLINE(void) dxbcByteWriterReset(DXBCByteWriter *w)
{
    RTMemFree(w->pu8ByteCodeBegin);
    RT_ZERO(*w);
}


DECLINLINE(void) dxbcByteWriterFetchData(DXBCByteWriter *w, void **ppv, uint32_t *pcb)
{
    *ppv = w->pu8ByteCodeBegin;
    *pcb = dxbcByteWriterSize(w);

    w->pu8ByteCodeBegin = NULL;
    dxbcByteWriterReset(w);
}


/*
 *
 * VGPU10 shader parser.
 *
 */

/* Parse an instruction operand. */
static int dxbcParseOperand(DXBCTokenReader *r, VGPUOperand *pOperand)
{
    RT_ZERO(*pOperand);

    ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);

    VGPU10OperandToken0 operand0;
    operand0.value = dxbcTokenReaderRead32(r);

    Log6(("    %s(%d)  %s(%d)  %s(%d)  %s(%d)\n",
          dxbcOperandNumComponentsToString(operand0.numComponents), operand0.numComponents,
          dxbcOperandComponentModeToString(operand0.selectionMode), operand0.selectionMode,
          dxbcOperandTypeToString(operand0.operandType), operand0.operandType,
          dxbcOperandIndexDimensionToString(operand0.indexDimension), operand0.indexDimension));

    ASSERT_GUEST_RETURN(operand0.numComponents <= VGPU10_OPERAND_4_COMPONENT, VERR_INVALID_PARAMETER);
    if (   operand0.operandType != VGPU10_OPERAND_TYPE_IMMEDIATE32
        && operand0.operandType != VGPU10_OPERAND_TYPE_IMMEDIATE64)
    {
        if (operand0.numComponents == VGPU10_OPERAND_4_COMPONENT)
        {
            ASSERT_GUEST_RETURN(operand0.selectionMode <= VGPU10_OPERAND_4_COMPONENT_SELECT_1_MODE, VERR_INVALID_PARAMETER);
            switch (operand0.selectionMode)
            {
                case VGPU10_OPERAND_4_COMPONENT_MASK_MODE:
                    Log6(("    Mask %#x\n", operand0.mask));
                    break;
                case VGPU10_OPERAND_4_COMPONENT_SWIZZLE_MODE:
                    Log6(("    Swizzle %s(%d)  %s(%d)  %s(%d)  %s(%d)\n",
                          dxbcOperandComponentNameToString(operand0.swizzleX), operand0.swizzleX,
                          dxbcOperandComponentNameToString(operand0.swizzleY), operand0.swizzleY,
                          dxbcOperandComponentNameToString(operand0.swizzleZ), operand0.swizzleZ,
                          dxbcOperandComponentNameToString(operand0.swizzleW), operand0.swizzleW));
                    break;
                case VGPU10_OPERAND_4_COMPONENT_SELECT_1_MODE:
                    Log6(("    Select %s(%d)\n",
                          dxbcOperandComponentNameToString(operand0.selectMask), operand0.selectMask));
                    break;
                default: /* Never happens. */
                    break;
            }
        }
    }

    if (operand0.extended)
    {
        ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);

        VGPU10OperandToken1 operand1;
        operand1.value = dxbcTokenReaderRead32(r);
    }

    ASSERT_GUEST_RETURN(operand0.indexDimension <= VGPU10_OPERAND_INDEX_3D, VERR_INVALID_PARAMETER);
    ASSERT_GUEST_RETURN(operand0.operandType < VGPU10_NUM_OPERANDS, VERR_INVALID_PARAMETER);

    if (   operand0.operandType == VGPU10_OPERAND_TYPE_IMMEDIATE32
        || operand0.operandType == VGPU10_OPERAND_TYPE_IMMEDIATE64)
    {
        uint32_t cComponent = 0;
        if (operand0.numComponents == VGPU10_OPERAND_4_COMPONENT)
            cComponent = 4;
        else if (operand0.numComponents == VGPU10_OPERAND_1_COMPONENT)
            cComponent = 1;

        for (uint32_t i = 0; i < cComponent; ++i)
        {
            ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
            pOperand->aImm[i] = dxbcTokenReaderRead32(r);
        }
    }

    pOperand->numComponents  = operand0.numComponents;
    pOperand->selectionMode  = operand0.selectionMode;
    pOperand->mask           = operand0.mask;
    pOperand->operandType    = operand0.operandType;
    pOperand->indexDimension = operand0.indexDimension;

    /* 'indexDimension' tells the number of indices. 'i' is the array index, i.e. i = 0 for 1D, etc. */
    for (uint32_t i = 0; i < operand0.indexDimension; ++i)
    {
        if (i == 0)                                          /* VGPU10_OPERAND_INDEX_1D */
            pOperand->aOperandIndex[i].indexRepresentation = operand0.index0Representation;
        else if (i == 1)                                     /* VGPU10_OPERAND_INDEX_2D */
            pOperand->aOperandIndex[i].indexRepresentation = operand0.index1Representation;
        else                                                 /* VGPU10_OPERAND_INDEX_3D */
            continue; /* Skip because it is "rarely if ever used" and is not supported by VGPU10. */

        uint32_t const indexRepresentation = pOperand->aOperandIndex[i].indexRepresentation;
        switch (indexRepresentation)
        {
            case VGPU10_OPERAND_INDEX_IMMEDIATE32:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                pOperand->aOperandIndex[i].aOperandIndex[0] = dxbcTokenReaderRead32(r);
                break;
            }
            case VGPU10_OPERAND_INDEX_IMMEDIATE64:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 2), VERR_INVALID_PARAMETER);
                pOperand->aOperandIndex[i].aOperandIndex[0] = dxbcTokenReaderRead64(r);
                break;
            }
            case VGPU10_OPERAND_INDEX_RELATIVE:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                pOperand->aOperandIndex[i].aOperandIndex[0] = dxbcTokenReaderRead32(r);
                break;
            }
            case VGPU10_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 2), VERR_INVALID_PARAMETER);
                pOperand->aOperandIndex[i].aOperandIndex[0] = dxbcTokenReaderRead32(r);
                pOperand->aOperandIndex[i].aOperandIndex[1] = dxbcTokenReaderRead32(r);
                break;
            }
            case VGPU10_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 3), VERR_INVALID_PARAMETER);
                pOperand->aOperandIndex[i].aOperandIndex[0] = dxbcTokenReaderRead64(r);
                pOperand->aOperandIndex[i].aOperandIndex[1] = dxbcTokenReaderRead32(r);
                break;
            }
            default:
                ASSERT_GUEST_FAILED_RETURN(VERR_INVALID_PARAMETER);
        }
        Log6(("    [operand index %d] %s(%d): %#llx, %#llx\n",
              i, dxbcOperandIndexRepresentationToString(indexRepresentation), indexRepresentation, pOperand->aOperandIndex[i].aOperandIndex[0], pOperand->aOperandIndex[i].aOperandIndex[1]));
    }
    return VINF_SUCCESS;
}


/* Parse an instruction. */
static int dxbcParseOpcode(DXBCTokenReader *r, VGPUOpcode *pOpcode)
{
    RT_ZERO(*pOpcode);
    ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);

    VGPU10OpcodeToken0 opcode;
    opcode.value = dxbcTokenReaderRead32(r);

    pOpcode->opcodeType = opcode.opcodeType;
    ASSERT_GUEST_RETURN(pOpcode->opcodeType < VGPU10_NUM_OPCODES, VERR_INVALID_PARAMETER);

    uint32_t const cOperand = g_aOpcodeInfo[pOpcode->opcodeType].cOperand;
    if (cOperand != UINT32_MAX)
    {
        Log6(("[%#x] %s length %d %s\n",
              dxbcTokenReaderByteOffset(r), dxbcOpcodeToString(pOpcode->opcodeType), opcode.instructionLength, dxbcInterpolationModeToString(opcode.interpolationMode)));

        ASSERT_GUEST_RETURN(cOperand < RT_ELEMENTS(pOpcode->aOperand), VERR_INVALID_PARAMETER);

        pOpcode->cOpcodeToken = opcode.instructionLength;
        if (opcode.extended)
        {
            ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
            if (   pOpcode->opcodeType == VGPU10_OPCODE_DCL_FUNCTION_BODY
                || pOpcode->opcodeType == VGPU10_OPCODE_DCL_FUNCTION_TABLE
                || pOpcode->opcodeType == VGPU10_OPCODE_DCL_INTERFACE
                || pOpcode->opcodeType == VGPU10_OPCODE_INTERFACE_CALL
                || pOpcode->opcodeType == VGPU10_OPCODE_DCL_THREAD_GROUP)
            {
                /* "next DWORD contains ... the actual instruction length in DWORD since it may not fit into 7 bits" */
                pOpcode->cOpcodeToken = dxbcTokenReaderRead32(r);
            }
            else
                AssertFailedReturn(VERR_NOT_IMPLEMENTED); /** @todo Anything else special for extended opcodes. */
        }

        ASSERT_GUEST_RETURN(pOpcode->cOpcodeToken >= 1 && pOpcode->cOpcodeToken < 256, VERR_INVALID_PARAMETER);
        ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, pOpcode->cOpcodeToken - 1), VERR_INVALID_PARAMETER);

        /* Additional tokens before operands. */
        switch (pOpcode->opcodeType)
        {
            case VGPU10_OPCODE_INTERFACE_CALL:
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* Function index */
                break;

            default:
                break;
        }

        /* Operands. */
        for (uint32_t iOperand = 0; iOperand < cOperand; ++iOperand)
        {
            Log6(("  [operand %d]\n", iOperand));
            int rc = dxbcParseOperand(r, &pOpcode->aOperand[iOperand]);
            ASSERT_GUEST_RETURN(RT_SUCCESS(rc), VERR_INVALID_PARAMETER);
        }

        pOpcode->cOperand = cOperand;

        /* Additional tokens after operands. */
        switch (pOpcode->opcodeType)
        {
            case VGPU10_OPCODE_DCL_INPUT_SIV:
            case VGPU10_OPCODE_DCL_INPUT_SGV:
            case VGPU10_OPCODE_DCL_INPUT_PS_SIV:
            case VGPU10_OPCODE_DCL_INPUT_PS_SGV:
            case VGPU10_OPCODE_DCL_OUTPUT_SIV:
            case VGPU10_OPCODE_DCL_OUTPUT_SGV:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);

                VGPU10NameToken name;
                name.value = dxbcTokenReaderRead32(r);
                Log6(("  %s(%d)\n",
                      dxbcSystemNameToString(name.name), name.name));
                pOpcode->semanticName = name.name;
                break;
            }
            case VGPU10_OPCODE_DCL_RESOURCE:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* ResourceReturnTypeToken */
                break;
            }
            case VGPU10_OPCODE_DCL_TEMPS:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* number of temps */
                break;
            }
            case VGPU10_OPCODE_DCL_INDEXABLE_TEMP:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 3), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 3); /* register index; number of registers; number of components */
                break;
            }
            case VGPU10_OPCODE_DCL_INDEX_RANGE:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* count of registers */
                break;
            }
            case VGPU10_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* maximum number of primitives */
                break;
            }
            case VGPU10_OPCODE_DCL_GS_INSTANCE_COUNT:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* number of instances */
                break;
            }
            case VGPU10_OPCODE_DCL_HS_MAX_TESSFACTOR:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* maximum TessFactor */
                break;
            }
            case VGPU10_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
            case VGPU10_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* number of instances of the current fork/join phase program to execute */
                break;
            }
            case VGPU10_OPCODE_DCL_THREAD_GROUP:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 3), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 3); /* Thread Group dimensions as UINT32: x, y, z */
                break;
            }
            case VGPU10_OPCODE_DCL_UAV_TYPED:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* ResourceReturnTypeToken */
                break;
            }
            case VGPU10_OPCODE_DCL_UAV_STRUCTURED:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* byte stride */
                break;
            }
            case VGPU10_OPCODE_DCL_TGSM_RAW:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* element count */
                break;
            }
            case VGPU10_OPCODE_DCL_TGSM_STRUCTURED:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 2), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 2); /* struct byte stride; struct count */
                break;
            }
            case VGPU10_OPCODE_DCL_RESOURCE_STRUCTURED:
            {
                ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
                dxbcTokenReaderSkip(r, 1); /* struct byte stride */
                break;
            }
            default:
                break;
        }
    }
    else
    {
        /* Special opcodes. */
        if (pOpcode->opcodeType == VGPU10_OPCODE_CUSTOMDATA)
        {
            Log6(("[%#x] %s %s\n",
                  dxbcTokenReaderByteOffset(r), dxbcOpcodeToString(pOpcode->opcodeType), dxbcCustomDataClassToString(opcode.customDataClass)));

            ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, 1), VERR_INVALID_PARAMETER);
            pOpcode->cOpcodeToken = dxbcTokenReaderRead32(r);

            if (pOpcode->cOpcodeToken < 2)
               pOpcode->cOpcodeToken = 2;
            ASSERT_GUEST_RETURN(dxbcTokenReaderCanRead(r, pOpcode->cOpcodeToken - 2), VERR_INVALID_PARAMETER);

            dxbcTokenReaderSkip(r, pOpcode->cOpcodeToken - 2);
        }
        else if (pOpcode->opcodeType == VGPU10_OPCODE_VMWARE)
        {
            /** @todo implement */
            ASSERT_GUEST_FAILED_RETURN(VERR_INVALID_PARAMETER);
        }
        else
            ASSERT_GUEST_FAILED_RETURN(VERR_INVALID_PARAMETER);

        // pOpcode->cOperand = 0;
    }

    return VINF_SUCCESS;
}


/*
 * Parse and verify the shader byte code. Extract input and output signatures into pInfo.
 */
int DXShaderParse(void const *pvShaderCode, uint32_t cbShaderCode, DXShaderInfo *pInfo)
{
    if (pInfo)
        RT_ZERO(*pInfo);

    ASSERT_GUEST_RETURN(cbShaderCode <= SVGA3D_MAX_SHADER_MEMORY_BYTES, VERR_INVALID_PARAMETER);
    ASSERT_GUEST_RETURN((cbShaderCode & 0x3) == 0, VERR_INVALID_PARAMETER); /* Aligned to the token size. */
    ASSERT_GUEST_RETURN(cbShaderCode >= 8, VERR_INVALID_PARAMETER); /* At least program and length tokens. */

    uint32_t const *paToken = (uint32_t *)pvShaderCode;

    VGPU10ProgramToken const *pProgramToken = (VGPU10ProgramToken *)&paToken[0];
    ASSERT_GUEST_RETURN(   pProgramToken->majorVersion >= 4
                        && pProgramToken->programType <= VGPU10_COMPUTE_SHADER, VERR_INVALID_PARAMETER);
    if (pInfo)
        pInfo->enmProgramType = (VGPU10_PROGRAM_TYPE)pProgramToken->programType;

    uint32_t const cToken = paToken[1];
    Log6(("Shader version %d.%d type %s(%d) Length %d\n",
          pProgramToken->majorVersion, pProgramToken->minorVersion, dxbcShaderTypeToString(pProgramToken->programType), pProgramToken->programType, cToken));
    ASSERT_GUEST_RETURN(cbShaderCode / 4 == cToken, VERR_INVALID_PARAMETER); /* Declared length should be equal to the actual. */

    DXBCTokenReader parser;
    RT_ZERO(parser);

    DXBCTokenReader *r = &parser;
    r->pToken = &paToken[2];
    r->cToken = r->cRemainingToken = cToken - 2;

    while (dxbcTokenReaderCanRead(r, 1))
    {
        VGPUOpcode opcode;
        int rc = dxbcParseOpcode(r, &opcode);
        ASSERT_GUEST_RETURN(RT_SUCCESS(rc), VERR_INVALID_PARAMETER);

        if (pInfo)
        {
            /* Fetch signatures. */
            SVGA3dDXSignatureEntry *pSignatureEntry = NULL;
            switch (opcode.opcodeType)
            {
                case VGPU10_OPCODE_DCL_INPUT:
                case VGPU10_OPCODE_DCL_INPUT_PS:
                case VGPU10_OPCODE_DCL_INPUT_SIV:
                    ASSERT_GUEST_RETURN(pInfo->cInputSignature < RT_ELEMENTS(pInfo->aInputSignature), VERR_INVALID_PARAMETER);
                    pSignatureEntry = &pInfo->aInputSignature[pInfo->cInputSignature++];
                    break;
                case VGPU10_OPCODE_DCL_OUTPUT:
                case VGPU10_OPCODE_DCL_OUTPUT_SIV:
                    ASSERT_GUEST_RETURN(pInfo->cOutputSignature < RT_ELEMENTS(pInfo->aOutputSignature), VERR_INVALID_PARAMETER);
                    pSignatureEntry = &pInfo->aOutputSignature[pInfo->cOutputSignature++];
                    break;
                default:
                    break;
            }

            if (pSignatureEntry)
            {
                pSignatureEntry->registerIndex = opcode.aOperand[0].aOperandIndex[0].aOperandIndex[0];
                pSignatureEntry->semanticName  = opcode.semanticName;
                pSignatureEntry->mask          = opcode.aOperand[0].mask;
                pSignatureEntry->componentType = SVGADX_SIGNATURE_REGISTER_COMPONENT_UNKNOWN; /// @todo Proper value? Seems that it is not important.
                pSignatureEntry->minPrecision  = SVGADX_SIGNATURE_MIN_PRECISION_DEFAULT;
            }
        }
    }

#ifdef LOG_ENABLED
    if (pInfo->cInputSignature)
    {
        Log6(("Input signatures:\n"));
        for (uint32_t i = 0; i < pInfo->cInputSignature; ++i)
            Log6(("  [%u]: %u %u 0x%X\n", i, pInfo->aInputSignature[i].registerIndex, pInfo->aInputSignature[i].semanticName, pInfo->aInputSignature[i].mask));
    }
    if (pInfo->cOutputSignature)
    {
        Log6(("Output signatures:\n"));
        for (uint32_t i = 0; i < pInfo->cOutputSignature; ++i)
            Log6(("  [%u]: %u %u 0x%X\n", i, pInfo->aOutputSignature[i].registerIndex, pInfo->aOutputSignature[i].semanticName, pInfo->aOutputSignature[i].mask));
    }
    if (pInfo->cPatchConstantSignature)
    {
        Log6(("Patch constant signatures:\n"));
        for (uint32_t i = 0; i < pInfo->cPatchConstantSignature; ++i)
            Log6(("  [%u]: %u %u 0x%X\n", i, pInfo->aPatchConstantSignature[i].registerIndex, pInfo->aPatchConstantSignature[i].semanticName, pInfo->aPatchConstantSignature[i].mask));
    }
#endif

    return VINF_SUCCESS;
}


#if 0 // Unused. Replaced with dxbcSemanticInfo.
static char const *dxbcSemanticName(SVGA3dDXSignatureSemanticName enmSemanticName)
{
    /* https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics#system-value-semantics */
    switch (enmSemanticName)
    {
        case SVGADX_SIGNATURE_SEMANTIC_NAME_POSITION:                          return "SV_Position";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_CLIP_DISTANCE:                     return "SV_ClipDistance";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_CULL_DISTANCE:                     return "SV_CullDistance";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_RENDER_TARGET_ARRAY_INDEX:         return "SV_RenderTargetArrayIndex";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_VIEWPORT_ARRAY_INDEX:              return "SV_ViewportArrayIndex";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_VERTEX_ID:                         return "SV_VertexID";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_PRIMITIVE_ID:                      return "SV_PrimitiveID";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_INSTANCE_ID:                       return "SV_InstanceID";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_IS_FRONT_FACE:                     return "SV_IsFrontFace";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_SAMPLE_INDEX:                      return "SV_SampleIndex";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR: return "SV_FinalQuadUeq0EdgeTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR: return "SV_FinalQuadVeq0EdgeTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR: return "SV_FinalQuadUeq1EdgeTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR: return "SV_FinalQuadVeq1EdgeTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR:    return "SV_FinalQuadUInsideTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR:    return "SV_FinalQuadVInsideTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR:  return "SV_FinalTriUeq0EdgeTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR:  return "SV_FinalTriVeq0EdgeTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR:  return "SV_FinalTriWeq0EdgeTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_INSIDE_TESSFACTOR:       return "SV_FinalTriInsideTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_LINE_DETAIL_TESSFACTOR:      return "SV_FinalLineDetailTessFactor";
        case SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_LINE_DENSITY_TESSFACTOR:     return "SV_FinalLineDensityTessFactor";
        default:
            Assert(enmSemanticName == SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED);
            break;
    }
    /* Generic. Arbitrary name. It does not have any meaning. */
    return "ATTRIB";
}
#endif


/* https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics#system-value-semantics
 * Type:
 * 0 - undefined
 * 1 - unsigned int
 * 2 - signed int
 * 3 - float
 */
typedef struct VGPUSemanticInfo
{
    char const *pszName;
    uint32_t u32Type;
} VGPUSemanticInfo;

static VGPUSemanticInfo const g_aSemanticInfo[SVGADX_SIGNATURE_SEMANTIC_NAME_MAX] =
{
    { "ATTRIB",                         0 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED                          0
    { "SV_Position",                    3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_POSITION                           1
    { "SV_ClipDistance",                3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_CLIP_DISTANCE                      2
    { "SV_CullDistance",                3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_CULL_DISTANCE                      3
    { "SV_RenderTargetArrayIndex",      1 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_RENDER_TARGET_ARRAY_INDEX          4
    { "SV_ViewportArrayIndex",          1 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_VIEWPORT_ARRAY_INDEX               5
    { "SV_VertexID",                    1 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_VERTEX_ID                          6
    { "SV_PrimitiveID",                 1 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_PRIMITIVE_ID                       7
    { "SV_InstanceID",                  1 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_INSTANCE_ID                        8
    { "SV_IsFrontFace",                 1 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_IS_FRONT_FACE                      9
    { "SV_SampleIndex",                 1 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_SAMPLE_INDEX                       10
    /** @todo  Is this a correct name for all TessFactors? */
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR  11
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR  12
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR  13
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR  14
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR     15
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR     16
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR   17
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR   18
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR   19
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_TRI_INSIDE_TESSFACTOR        20
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_LINE_DETAIL_TESSFACTOR       21
    { "SV_TessFactor",                  3 }, // SVGADX_SIGNATURE_SEMANTIC_NAME_FINAL_LINE_DENSITY_TESSFACTOR      22
};

static VGPUSemanticInfo const g_SemanticPSOutput =
    { "SV_TARGET",                      3 }; // SVGADX_SIGNATURE_SEMANTIC_NAME_UNDEFINED                          0


static VGPUSemanticInfo const *dxbcSemanticInfo(DXShaderInfo const *pInfo, SVGA3dDXSignatureSemanticName enmSemanticName, uint32_t u32BlobType)
{
    if (enmSemanticName < RT_ELEMENTS(g_aSemanticInfo))
    {
        if (   enmSemanticName == 0
            && pInfo->enmProgramType == VGPU10_PIXEL_SHADER
            && u32BlobType == DXBC_BLOB_TYPE_OSGN)
            return &g_SemanticPSOutput;
        return &g_aSemanticInfo[enmSemanticName];
    }
    return &g_aSemanticInfo[0];
}


static int dxbcCreateIOSGNBlob(DXShaderInfo const *pInfo, DXBCHeader *pHdr, uint32_t u32BlobType,
                               uint32_t cSignature, SVGA3dDXSignatureEntry const *paSignature, DXBCByteWriter *w)
{
    /* aIdxSignature contains signature indices. aIdxSignature[0] = signature index for register 0. */
    uint32_t aIdxSignature[32];
    memset(aIdxSignature, 0xFF, sizeof(aIdxSignature));
    AssertReturn(cSignature <= RT_ELEMENTS(aIdxSignature), VERR_INTERNAL_ERROR);
    for (uint32_t i = 0; i < cSignature; ++i)
    {
        SVGA3dDXSignatureEntry const *src = &paSignature[i];
        ASSERT_GUEST_RETURN(src->registerIndex < RT_ELEMENTS(aIdxSignature), VERR_INVALID_PARAMETER);
        ASSERT_GUEST_RETURN(aIdxSignature[src->registerIndex] == 0xFFFFFFFF, VERR_INVALID_PARAMETER);
        aIdxSignature[src->registerIndex] = i;
    }

    uint32_t cbBlob = RT_UOFFSETOF(DXBCBlobIOSGN, aElement[cSignature])
                    + cSignature * RT_SIZEOFMEMB(DXBCBlobIOSGN, aElement[0]);
    if (!dxbcByteWriterCanWrite(w, sizeof(DXBCBlobHeader) + cbBlob))
        return VERR_NO_MEMORY;

    DXBCBlobHeader *pHdrBlob = (DXBCBlobHeader *)dxbcByteWriterPtr(w);
    pHdrBlob->u32BlobType = u32BlobType;
    // pHdrBlob->cbBlob = 0;

    DXBCBlobIOSGN *pHdrISGN = (DXBCBlobIOSGN *)&pHdrBlob[1];
    pHdrISGN->cElement = cSignature;
    pHdrISGN->offElement = RT_UOFFSETOF(DXBCBlobIOSGN, aElement[0]);

    uint32_t aSemanticIdx[SVGADX_SIGNATURE_SEMANTIC_NAME_MAX];
    RT_ZERO(aSemanticIdx);
    uint32_t iSignature = 0;
    for (uint32_t iReg = 0; iReg < RT_ELEMENTS(aIdxSignature); ++iReg)
    {
        if (aIdxSignature[iReg] == 0xFFFFFFFF) /* This register is unused. */
            continue;

        AssertReturn(iSignature < cSignature, VERR_INTERNAL_ERROR);

        SVGA3dDXSignatureEntry const *src = &paSignature[aIdxSignature[iReg]];
        DXBCBlobIOSGNElement *dst = &pHdrISGN->aElement[iSignature];

        ASSERT_GUEST_RETURN(src->semanticName < SVGADX_SIGNATURE_SEMANTIC_NAME_MAX, VERR_INVALID_PARAMETER);
        VGPUSemanticInfo const *pSemanticInfo = dxbcSemanticInfo(pInfo, src->semanticName, u32BlobType);

        dst->offElementName   = cbBlob; /* Offset of the semantic's name relative to the start of the blob (without hdr). */
        dst->idxSemantic      = aSemanticIdx[src->semanticName]++;
        dst->enmSystemValue   = src->semanticName;
        dst->enmComponentType = src->componentType ? src->componentType : pSemanticInfo->u32Type;
        dst->idxRegister      = src->registerIndex;
        dst->mask             = src->mask;
        if (u32BlobType == DXBC_BLOB_TYPE_OSGN)
            dst->mask2            = 0;
        else
            dst->mask2            = src->mask;

        /* Figure out the semantic name for this element. */
        char const * const pszElementName = pSemanticInfo->pszName;
        uint32_t const cbElementName = (uint32_t)strlen(pszElementName) + 1;

        if (!dxbcByteWriterCanWrite(w, cbBlob + cbElementName))
            return VERR_NO_MEMORY;

        char *pszElementNameDst = (char *)pHdrISGN + dst->offElementName;
        memcpy(pszElementNameDst, pszElementName, cbElementName);

        cbBlob += cbElementName;
        ++iSignature;
    }

    /* Blobs are 4 bytes aligned. Commit the blob data. */
    cbBlob = RT_ALIGN_32(cbBlob, 4);
    pHdrBlob->cbBlob = cbBlob;
    pHdr->cbTotal += cbBlob + sizeof(DXBCBlobHeader);
    dxbcByteWriterCommit(w, cbBlob + sizeof(DXBCBlobHeader));
    return VINF_SUCCESS;
}


static int dxbcCreateSHDRBlob(DXBCHeader *pHdr, uint32_t u32BlobType,
                              void const *pvShader, uint32_t cbShader, DXBCByteWriter *w)
{
    uint32_t cbBlob = cbShader;
    if (!dxbcByteWriterCanWrite(w, sizeof(DXBCBlobHeader) + cbBlob))
        return VERR_NO_MEMORY;

    DXBCBlobHeader *pHdrBlob = (DXBCBlobHeader *)dxbcByteWriterPtr(w);
    pHdrBlob->u32BlobType = u32BlobType;
    // pHdrBlob->cbBlob = 0;

    memcpy(&pHdrBlob[1], pvShader, cbShader);

    /* Blobs are 4 bytes aligned. Commit the blob data. */
    cbBlob = RT_ALIGN_32(cbBlob, 4);
    pHdrBlob->cbBlob = cbBlob;
    pHdr->cbTotal += cbBlob + sizeof(DXBCBlobHeader);
    dxbcByteWriterCommit(w, cbBlob + sizeof(DXBCBlobHeader));
    return VINF_SUCCESS;
}


/*
 * Create a DXBC container with signature and shader code data blobs.
 */
static int dxbcCreateFromInfo(DXShaderInfo const *pInfo, void const *pvShader, uint32_t cbShader, DXBCByteWriter *w)
{
    int rc;

    /* Create a DXBC container with ISGN, OSGN and SHDR blobs. */
    uint32_t const cBlob = 3;
    uint32_t const cbHdr = RT_UOFFSETOF(DXBCHeader, aBlobOffset[cBlob]); /* Header with blob offsets. */
    if (!dxbcByteWriterCanWrite(w, cbHdr))
        return VERR_NO_MEMORY;

    /* Container header. */
    DXBCHeader *pHdr = (DXBCHeader *)dxbcByteWriterPtr(w);
    pHdr->u32DXBC    = DXBC_MAGIC;
    // RT_ZERO(pHdr->au8Hash);
    pHdr->u32Version = 1;
    pHdr->cbTotal    = cbHdr;
    pHdr->cBlob      = cBlob;
    //RT_ZERO(pHdr->aBlobOffset);
    dxbcByteWriterCommit(w, cbHdr);

    /* Blobs. */
    uint32_t iBlob = 0;

    pHdr->aBlobOffset[iBlob++] = dxbcByteWriterSize(w);
    rc = dxbcCreateIOSGNBlob(pInfo, pHdr, DXBC_BLOB_TYPE_ISGN, pInfo->cInputSignature, &pInfo->aInputSignature[0], w);
    AssertRCReturn(rc, rc);

    pHdr->aBlobOffset[iBlob++] = dxbcByteWriterSize(w);
    rc = dxbcCreateIOSGNBlob(pInfo, pHdr, DXBC_BLOB_TYPE_OSGN, pInfo->cOutputSignature, &pInfo->aOutputSignature[0], w);
    AssertRCReturn(rc, rc);

    pHdr->aBlobOffset[iBlob++] = dxbcByteWriterSize(w);
    rc = dxbcCreateSHDRBlob(pHdr, DXBC_BLOB_TYPE_SHDR, pvShader, cbShader, w);
    AssertRCReturn(rc, rc);

    AssertCompile(RT_UOFFSETOF(DXBCHeader, u32Version) == 0x14);
    dxbcHash(&pHdr->u32Version, pHdr->cbTotal - RT_UOFFSETOF(DXBCHeader, u32Version), pHdr->au8Hash);

    return VINF_SUCCESS;
}


int DXShaderCreateDXBC(DXShaderInfo const *pInfo, void const *pvShaderCode, uint32_t cbShaderCode, void **ppvDXBC, uint32_t *pcbDXBC)
{
    /* Build DXBC container. */
    int rc;
    DXBCByteWriter dxbcByteWriter;
    DXBCByteWriter *w = &dxbcByteWriter;
    if (dxbcByteWriterInit(w, 4096 + cbShaderCode))
    {
        rc = dxbcCreateFromInfo(pInfo, pvShaderCode, cbShaderCode, w);
        if (RT_SUCCESS(rc))
            dxbcByteWriterFetchData(w, ppvDXBC, pcbDXBC);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


#ifdef DXBC_STANDALONE_TEST
static int dxbcCreateFromBytecode(void const *pvShaderCode, uint32_t cbShaderCode, void **ppvDXBC, uint32_t *pcbDXBC)
{
    /* Parse the shader bytecode and create DXBC container with resource, signature and shader bytecode blobs. */
    DXShaderInfo info;
    RT_ZERO(info);
    int rc = DXShaderParse(pvShaderCode, cbShaderCode, &info);
    if (RT_SUCCESS(rc))
        rc = DXShaderCreateDXBC(&info, pvShaderCode, cbShaderCode, ppvDXBC, pcbDXBC);
    return rc;
}

static int parseShaderVM(void const *pvShaderCode, uint32_t cbShaderCode)
{
    void *pv = NULL;
    uint32_t cb = 0;
    int rc = dxbcCreateFromBytecode(pvShaderCode, cbShaderCode, &pv, &cb);
    if (RT_SUCCESS(rc))
    {
        /* Hexdump DXBC */
        printf("{\n");
        uint8_t *pu8 = (uint8_t *)pv;
        for (uint32_t i = 0; i < cb; ++i)
        {
            if ((i % 16) == 0)
            {
                if (i > 0)
                    printf(",\n");

                printf("    0x%02x", pu8[i]);
            }
            else
            {
                printf(", 0x%02x", pu8[i]);
            }
        }
        printf("\n");
        printf("};\n");

        RTMemFree(pv);
    }

    return rc;
}

static DXBCBlobHeader *dxbcFindBlob(DXBCHeader *pDXBCHeader, uint32_t u32BlobType)
{
    uint8_t const *pu8DXBCBegin = (uint8_t *)pDXBCHeader;
    for (uint32_t i = 0; i < pDXBCHeader->cBlob; ++i)
    {
        DXBCBlobHeader *pCurrentBlob = (DXBCBlobHeader *)&pu8DXBCBegin[pDXBCHeader->aBlobOffset[i]];
        if (pCurrentBlob->u32BlobType == u32BlobType)
            return pCurrentBlob;
    }
    return NULL;
}

static int dxbcExtractShaderCode(DXBCHeader *pDXBCHeader, void **ppvCode, uint32_t *pcbCode)
{
    DXBCBlobHeader *pBlob = dxbcFindBlob(pDXBCHeader, DXBC_BLOB_TYPE_SHDR);
    AssertReturn(pBlob, VERR_NOT_IMPLEMENTED);

    DXBCBlobSHDR *pSHDR = (DXBCBlobSHDR *)&pBlob[1];
    *pcbCode = pSHDR->cToken * 4;
    *ppvCode = RTMemAlloc(*pcbCode);
    AssertReturn(*ppvCode, VERR_NO_MEMORY);

    memcpy(*ppvCode, pSHDR, *pcbCode);
    return VINF_SUCCESS;
}

static int parseShaderDXBC(void const *pvDXBC)
{
    DXBCHeader *pDXBCHeader = (DXBCHeader *)pvDXBC;
    void *pvShaderCode = NULL;
    uint32_t cbShaderCode = 0;
    int rc = dxbcExtractShaderCode(pDXBCHeader, &pvShaderCode, &cbShaderCode);
    if (RT_SUCCESS(rc))
    {
        rc = parseShaderVM(pvShaderCode, cbShaderCode);
        RTMemFree(pvShaderCode);
    }
    return rc;
}
#endif /* DXBC_STANDALONE_TEST */
