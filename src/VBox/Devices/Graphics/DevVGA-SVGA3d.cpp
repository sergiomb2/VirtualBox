/* $Id$ */
/** @file
 * DevSVGA3d - VMWare SVGA device, 3D parts - Common core code.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
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
#include <VBox/vmm/pdmdev.h>
#include <iprt/errcore.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/mem.h>

#include <VBox/vmm/pgm.h> /* required by DevVGA.h */
#include <VBoxVideo.h> /* required by DevVGA.h */

/* should go BEFORE any other DevVGA include to make all DevVGA.h config defines be visible */
#include "DevVGA.h"

#include "DevVGA-SVGA.h"
#include "DevVGA-SVGA3d.h"
#define VMSVGA3D_INCL_STRUCTURE_DESCRIPTORS
#include "DevVGA-SVGA3d-internal.h"
#include "DevVGA-SVGA-internal.h"



/**
 * Implements the SVGA_3D_CMD_SURFACE_DEFINE_V2 and SVGA_3D_CMD_SURFACE_DEFINE
 * commands (fifo).
 *
 * @returns VBox status code (currently ignored).
 * @param   pThisCC             The VGA/VMSVGA state for ring-3.
 * @param   sid                 The ID of the surface to (re-)define.
 * @param   surfaceFlags        .
 * @param   format              .
 * @param   multisampleCount    .
 * @param   autogenFilter       .
 * @param   numMipLevels        .
 * @param   pMipLevel0Size      .
 */
int vmsvga3dSurfaceDefine(PVGASTATECC pThisCC, uint32_t sid, SVGA3dSurface1Flags surfaceFlags, SVGA3dSurfaceFormat format,
                          uint32_t multisampleCount, SVGA3dTextureFilter autogenFilter,
                          uint32_t numMipLevels, SVGA3dSize const *pMipLevel0Size)
{
    PVMSVGA3DSURFACE pSurface;
    PVMSVGA3DSTATE   pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    LogFunc(("sid=%u surfaceFlags=%#x format=%s (%#x) multiSampleCount=%d autogenFilter=%d, numMipLevels=%d size=(%dx%dx%d)\n",
             sid, surfaceFlags, vmsvgaLookupEnum((int)format, &g_SVGA3dSurfaceFormat2String), format, multisampleCount, autogenFilter,
             numMipLevels, pMipLevel0Size->width, pMipLevel0Size->height, pMipLevel0Size->depth));

    ASSERT_GUEST_RETURN(sid < SVGA3D_MAX_SURFACE_IDS, VERR_INVALID_PARAMETER);
    ASSERT_GUEST_RETURN(numMipLevels >= 1 && numMipLevels < SVGA3D_MAX_MIP_LEVELS, VERR_INVALID_PARAMETER);

    if (sid >= pState->cSurfaces)
    {
        /* Grow the array. */
        uint32_t cNew = RT_ALIGN(sid + 15, 16);
        void *pvNew = RTMemRealloc(pState->papSurfaces, sizeof(pState->papSurfaces[0]) * cNew);
        AssertReturn(pvNew, VERR_NO_MEMORY);
        pState->papSurfaces = (PVMSVGA3DSURFACE *)pvNew;
        while (pState->cSurfaces < cNew)
        {
            pSurface = (PVMSVGA3DSURFACE)RTMemAllocZ(sizeof(*pSurface));
            AssertReturn(pSurface, VERR_NO_MEMORY);
            pSurface->id = SVGA3D_INVALID_ID;
            pState->papSurfaces[pState->cSurfaces++] = pSurface;
        }
    }
    pSurface = pState->papSurfaces[sid];

    /* If one already exists with this id, then destroy it now. */
    if (pSurface->id != SVGA3D_INVALID_ID)
        vmsvga3dSurfaceDestroy(pThisCC, sid);

    RT_ZERO(*pSurface);
    pSurface->id = SVGA3D_INVALID_ID; /* Keep this value until the surface init completes */
#ifdef VMSVGA3D_OPENGL
    pSurface->idWeakContextAssociation = SVGA3D_INVALID_ID;
    pSurface->oglId.buffer          = OPENGL_INVALID_ID;
#elif defined(VMSVGA3D_D3D11)
    pSurface->idAssociatedContext   = SVGA3D_INVALID_ID;
    // pSurface->pBackendSurface       = NULL;
#else /* VMSVGA3D_DIRECT3D */
    pSurface->idAssociatedContext   = SVGA3D_INVALID_ID;
    pSurface->hSharedObject         = NULL;
    pSurface->pSharedObjectTree     = NULL;
#endif

    /** @todo This 'switch' and the surfaceFlags tweaks should not be necessary.
     * The actual surface type will be figured out when the surface is actually used later.
     * The backends code must be reviewed for unnecessary dependencies on the surfaceFlags value.
     */
    /* The surface type is sort of undefined now, even though the hints and format can help to clear that up.
     * In some case we'll have to wait until the surface is used to create the D3D object.
     */
    switch (format)
    {
    case SVGA3D_Z_D32:
    case SVGA3D_Z_D16:
    case SVGA3D_Z_D24S8:
    case SVGA3D_Z_D15S1:
    case SVGA3D_Z_D24X8:
    case SVGA3D_Z_DF16:
    case SVGA3D_Z_DF24:
    case SVGA3D_Z_D24S8_INT:
        Assert(surfaceFlags & SVGA3D_SURFACE_HINT_DEPTHSTENCIL);
        surfaceFlags |= SVGA3D_SURFACE_HINT_DEPTHSTENCIL;
        break;

    /* Texture compression formats */
    case SVGA3D_DXT1:
    case SVGA3D_DXT2:
    case SVGA3D_DXT3:
    case SVGA3D_DXT4:
    case SVGA3D_DXT5:
    /* Bump-map formats */
    case SVGA3D_BUMPU8V8:
    case SVGA3D_BUMPL6V5U5:
    case SVGA3D_BUMPX8L8V8U8:
    case SVGA3D_V8U8:
    case SVGA3D_Q8W8V8U8:
    case SVGA3D_CxV8U8:
    case SVGA3D_X8L8V8U8:
    case SVGA3D_A2W10V10U10:
    case SVGA3D_V16U16:
    /* Typical render target formats; we should allow render target buffers to be used as textures. */
    case SVGA3D_X8R8G8B8:
    case SVGA3D_A8R8G8B8:
    case SVGA3D_R5G6B5:
    case SVGA3D_X1R5G5B5:
    case SVGA3D_A1R5G5B5:
    case SVGA3D_A4R4G4B4:
        Assert(surfaceFlags & (SVGA3D_SURFACE_HINT_TEXTURE | SVGA3D_SURFACE_SCREENTARGET));
        surfaceFlags |= SVGA3D_SURFACE_HINT_TEXTURE;
        break;

    case SVGA3D_LUMINANCE8:
    case SVGA3D_LUMINANCE4_ALPHA4:
    case SVGA3D_LUMINANCE16:
    case SVGA3D_LUMINANCE8_ALPHA8:
    case SVGA3D_ARGB_S10E5:   /* 16-bit floating-point ARGB */
    case SVGA3D_ARGB_S23E8:   /* 32-bit floating-point ARGB */
    case SVGA3D_A2R10G10B10:
    case SVGA3D_ALPHA8:
    case SVGA3D_R_S10E5:
    case SVGA3D_R_S23E8:
    case SVGA3D_RG_S10E5:
    case SVGA3D_RG_S23E8:
    case SVGA3D_G16R16:
    case SVGA3D_A16B16G16R16:
    case SVGA3D_UYVY:
    case SVGA3D_YUY2:
    case SVGA3D_NV12:
    case SVGA3D_FORMAT_DEAD2: /* Old SVGA3D_AYUV */
    case SVGA3D_ATI1:
    case SVGA3D_ATI2:
        break;

    /*
     * Any surface can be used as a buffer object, but SVGA3D_BUFFER is
     * the most efficient format to use when creating new surfaces
     * expressly for index or vertex data.
     */
    case SVGA3D_BUFFER:
        break;

    default:
        break;
    }

    pSurface->surfaceFlags      = surfaceFlags;
    pSurface->format            = format;
    /* cFaces is 6 for a cubemaps and 1 otherwise. */
    pSurface->cFaces            = (uint32_t)((surfaceFlags & SVGA3D_SURFACE_CUBEMAP) ? 6 : 1);
    pSurface->cLevels           = numMipLevels;
    pSurface->multiSampleCount  = multisampleCount;
    pSurface->autogenFilter     = autogenFilter;
    Assert(autogenFilter != SVGA3D_TEX_FILTER_FLATCUBIC);
    Assert(autogenFilter != SVGA3D_TEX_FILTER_GAUSSIANCUBIC);
    pSurface->paMipmapLevels    = (PVMSVGA3DMIPMAPLEVEL)RTMemAllocZ(numMipLevels * pSurface->cFaces * sizeof(VMSVGA3DMIPMAPLEVEL));
    AssertReturn(pSurface->paMipmapLevels, VERR_NO_MEMORY);

    pSurface->cbBlock = vmsvga3dSurfaceFormatSize(format, &pSurface->cxBlock, &pSurface->cyBlock);
    AssertReturn(pSurface->cbBlock, VERR_INVALID_PARAMETER);

    /** @todo cbMemRemaining = value of SVGA_REG_MOB_MAX_SIZE */
    uint32_t cbMemRemaining = SVGA3D_MAX_SURFACE_MEM_SIZE; /* Do not allow more than this for a surface. */
    SVGA3dSize mipmapSize = *pMipLevel0Size;
    int rc = VINF_SUCCESS;

    for (uint32_t i = 0; i < numMipLevels; ++i)
    {
        for (uint32_t iFace = 0; iFace < pSurface->cFaces; ++iFace)
        {
            uint32_t const iMipmap = iFace * numMipLevels + i;
            LogFunc(("[%d] face %d mip level %d (%d,%d,%d) cbBlock=%#x block %dx%d\n",
                     iMipmap, iFace, i, mipmapSize.width, mipmapSize.height, mipmapSize.depth,
                     pSurface->cbBlock, pSurface->cxBlock, pSurface->cyBlock));

            uint32_t cBlocksX;
            uint32_t cBlocksY;
            if (RT_LIKELY(pSurface->cxBlock == 1 && pSurface->cyBlock == 1))
            {
                cBlocksX = mipmapSize.width;
                cBlocksY = mipmapSize.height;
            }
            else
            {
                cBlocksX = mipmapSize.width / pSurface->cxBlock;
                if (mipmapSize.width % pSurface->cxBlock)
                    ++cBlocksX;
                cBlocksY = mipmapSize.height / pSurface->cyBlock;
                if (mipmapSize.height % pSurface->cyBlock)
                    ++cBlocksY;
            }

            AssertBreakStmt(cBlocksX > 0 && cBlocksY > 0 && mipmapSize.depth > 0, rc = VERR_INVALID_PARAMETER);

            const uint32_t cMaxBlocksX = cbMemRemaining / pSurface->cbBlock;
            AssertBreakStmt(cBlocksX < cMaxBlocksX, rc = VERR_INVALID_PARAMETER);

            const uint32_t cbSurfacePitch = pSurface->cbBlock * cBlocksX;
            LogFunc(("cbSurfacePitch=0x%x\n", cbSurfacePitch));

            const uint32_t cMaxBlocksY = cbMemRemaining / cbSurfacePitch;
            AssertBreakStmt(cBlocksY < cMaxBlocksY, rc = VERR_INVALID_PARAMETER);

            const uint32_t cbSurfacePlane = cbSurfacePitch * cBlocksY;

            const uint32_t cMaxDepth = cbMemRemaining / cbSurfacePlane;
            AssertBreakStmt(mipmapSize.depth < cMaxDepth, rc = VERR_INVALID_PARAMETER);

            const uint32_t cbSurface = cbSurfacePlane * mipmapSize.depth;

            PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[iMipmap];
            pMipmapLevel->mipmapSize     = mipmapSize;
            pMipmapLevel->cBlocksX       = cBlocksX;
            pMipmapLevel->cBlocksY       = cBlocksY;
            pMipmapLevel->cBlocks        = cBlocksX * cBlocksY * mipmapSize.depth;
            pMipmapLevel->cbSurfacePitch = cbSurfacePitch;
            pMipmapLevel->cbSurfacePlane = cbSurfacePlane;
            pMipmapLevel->cbSurface      = cbSurface;
            pMipmapLevel->pSurfaceData   = NULL;

            cbMemRemaining -= cbSurface;
        }

        AssertRCBreak(rc);

        mipmapSize.width >>= 1;
        if (mipmapSize.width == 0) mipmapSize.width = 1;
        mipmapSize.height >>= 1;
        if (mipmapSize.height == 0) mipmapSize.height = 1;
        mipmapSize.depth >>= 1;
        if (mipmapSize.depth == 0) mipmapSize.depth = 1;
    }

    AssertLogRelRCReturnStmt(rc, RTMemFree(pSurface->paMipmapLevels), rc);

#ifdef VMSVGA3D_DIRECT3D
    /* Translate the format and usage flags to D3D. */
    pSurface->d3dfmtRequested   = vmsvga3dSurfaceFormat2D3D(format);
    pSurface->formatD3D         = D3D9GetActualFormat(pState, pSurface->d3dfmtRequested);
    pSurface->multiSampleTypeD3D= vmsvga3dMultipeSampleCount2D3D(multisampleCount);
    pSurface->fUsageD3D         = 0;
    if (surfaceFlags & SVGA3D_SURFACE_HINT_DYNAMIC)
        pSurface->fUsageD3D |= D3DUSAGE_DYNAMIC;
    if (surfaceFlags & SVGA3D_SURFACE_HINT_RENDERTARGET)
        pSurface->fUsageD3D |= D3DUSAGE_RENDERTARGET;
    if (surfaceFlags & SVGA3D_SURFACE_HINT_DEPTHSTENCIL)
        pSurface->fUsageD3D |= D3DUSAGE_DEPTHSTENCIL;
    if (surfaceFlags & SVGA3D_SURFACE_HINT_WRITEONLY)
        pSurface->fUsageD3D |= D3DUSAGE_WRITEONLY;
    if (surfaceFlags & SVGA3D_SURFACE_AUTOGENMIPMAPS)
        pSurface->fUsageD3D |= D3DUSAGE_AUTOGENMIPMAP;
    pSurface->enmD3DResType = VMSVGA3D_D3DRESTYPE_NONE;
    /* pSurface->u.pSurface = NULL; */
    /* pSurface->bounce.pTexture = NULL; */
    /* pSurface->emulated.pTexture = NULL; */
#elif defined(VMSVGA3D_D3D11)
    /* Nothing, because all backend specific data reside in pSurface->pBackendSurface. */
#else
    /* pSurface->fEmulated = false; */
    /* pSurface->idEmulated = OPENGL_INVALID_ID; */
    vmsvga3dSurfaceFormat2OGL(pSurface, format);
#endif

#ifdef LOG_ENABLED
    SVGA3dSurfaceAllFlags const f = surfaceFlags;
    LogFunc(("surface flags:%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s 0x%RX64\n",
            (f & SVGA3D_SURFACE_CUBEMAP)              ? " CUBEMAP"  : "",
            (f & SVGA3D_SURFACE_HINT_STATIC)          ? " HINT_STATIC"  : "",
            (f & SVGA3D_SURFACE_HINT_DYNAMIC)         ? " HINT_DYNAMIC"  : "",
            (f & SVGA3D_SURFACE_HINT_INDEXBUFFER)     ? " HINT_INDEXBUFFER"  : "",
            (f & SVGA3D_SURFACE_HINT_VERTEXBUFFER)    ? " HINT_VERTEXBUFFER"  : "",
            (f & SVGA3D_SURFACE_HINT_TEXTURE)         ? " HINT_TEXTURE"  : "",
            (f & SVGA3D_SURFACE_HINT_RENDERTARGET)    ? " HINT_RENDERTARGET"  : "",
            (f & SVGA3D_SURFACE_HINT_DEPTHSTENCIL)    ? " HINT_DEPTHSTENCIL"  : "",
            (f & SVGA3D_SURFACE_HINT_WRITEONLY)       ? " HINT_WRITEONLY"  : "",
            (f & SVGA3D_SURFACE_DEAD2)                ? " DEAD2"  : "",
            (f & SVGA3D_SURFACE_AUTOGENMIPMAPS)       ? " AUTOGENMIPMAPS"  : "",
            (f & SVGA3D_SURFACE_DEAD1)                ? " DEAD1"  : "",
            (f & SVGA3D_SURFACE_MOB_PITCH)            ? " MOB_PITCH"  : "",
            (f & SVGA3D_SURFACE_INACTIVE)             ? " INACTIVE"  : "",
            (f & SVGA3D_SURFACE_HINT_RT_LOCKABLE)     ? " HINT_RT_LOCKABLE"  : "",
            (f & SVGA3D_SURFACE_VOLUME)               ? " VOLUME"  : "",
            (f & SVGA3D_SURFACE_SCREENTARGET)         ? " SCREENTARGET"  : "",
            (f & SVGA3D_SURFACE_ALIGN16)              ? " ALIGN16"  : "",
            (f & SVGA3D_SURFACE_1D)                   ? " 1D"  : "",
            (f & SVGA3D_SURFACE_ARRAY)                ? " ARRAY"  : "",
            (f & SVGA3D_SURFACE_BIND_VERTEX_BUFFER)   ? " BIND_VERTEX_BUFFER"  : "",
            (f & SVGA3D_SURFACE_BIND_INDEX_BUFFER)    ? " BIND_INDEX_BUFFER"  : "",
            (f & SVGA3D_SURFACE_BIND_CONSTANT_BUFFER) ? " BIND_CONSTANT_BUFFER"  : "",
            (f & SVGA3D_SURFACE_BIND_SHADER_RESOURCE) ? " BIND_SHADER_RESOURCE"  : "",
            (f & SVGA3D_SURFACE_BIND_RENDER_TARGET)   ? " BIND_RENDER_TARGET"  : "",
            (f & SVGA3D_SURFACE_BIND_DEPTH_STENCIL)   ? " BIND_DEPTH_STENCIL"  : "",
            (f & SVGA3D_SURFACE_BIND_STREAM_OUTPUT)   ? " BIND_STREAM_OUTPUT"  : "",
            (f & SVGA3D_SURFACE_STAGING_UPLOAD)       ? " STAGING_UPLOAD"  : "",
            (f & SVGA3D_SURFACE_STAGING_DOWNLOAD)     ? " STAGING_DOWNLOAD"  : "",
            (f & SVGA3D_SURFACE_HINT_INDIRECT_UPDATE) ? " HINT_INDIRECT_UPDATE"  : "",
            (f & SVGA3D_SURFACE_TRANSFER_FROM_BUFFER) ? " TRANSFER_FROM_BUFFER"  : "",
            (f & SVGA3D_SURFACE_RESERVED1)            ? " RESERVED1"  : "",
            (f & SVGA3D_SURFACE_MULTISAMPLE)          ? " MULTISAMPLE"  : "",
            (f & SVGA3D_SURFACE_BIND_UAVIEW)          ? " BIND_UAVIEW"  : "",
            (f & SVGA3D_SURFACE_TRANSFER_TO_BUFFER)   ? " TRANSFER_TO_BUFFER"  : "",
            (f & SVGA3D_SURFACE_BIND_LOGICOPS)        ? " BIND_LOGICOPS"  : "",
            (f & SVGA3D_SURFACE_BIND_RAW_VIEWS)       ? " BIND_RAW_VIEWS"  : "",
            (f & SVGA3D_SURFACE_BUFFER_STRUCTURED)    ? " BUFFER_STRUCTURED"  : "",
            (f & SVGA3D_SURFACE_DRAWINDIRECT_ARGS)    ? " DRAWINDIRECT_ARGS"  : "",
            (f & SVGA3D_SURFACE_RESOURCE_CLAMP)       ? " RESOURCE_CLAMP"  : "",
            (f & SVGA3D_SURFACE_FLAG_MAX)             ? " FLAG_MAX"  : "",
            f & ~(SVGA3D_SURFACE_FLAG_MAX - 1ULL)
           ));
#endif

    Assert(!VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface));

    /* Allocate buffer to hold the surface data until we can move it into a D3D object */
    for (uint32_t i = 0; i < numMipLevels * pSurface->cFaces; ++i)
    {
        PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[i];
        pMipmapLevel->pSurfaceData = RTMemAllocZ(pMipmapLevel->cbSurface);
        AssertReturn(pMipmapLevel->pSurfaceData, VERR_NO_MEMORY);
    }

    pSurface->id = sid;
    return VINF_SUCCESS;
}


/**
 * Implements the SVGA_3D_CMD_SURFACE_DESTROY command (fifo).
 *
 * @returns VBox status code (currently ignored).
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   sid             The ID of the surface to destroy.
 */
int vmsvga3dSurfaceDestroy(PVGASTATECC pThisCC, uint32_t sid)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, sid, &pSurface);
    AssertRCReturn(rc, rc);

    LogFunc(("sid=%u\n", sid));

    /* Check all contexts if this surface is used as a render target or active texture. */
    for (uint32_t cid = 0; cid < pState->cContexts; cid++)
    {
        PVMSVGA3DCONTEXT pContext = pState->papContexts[cid];
        if (pContext->id == cid)
        {
            for (uint32_t i = 0; i < RT_ELEMENTS(pContext->aSidActiveTextures); ++i)
                if (pContext->aSidActiveTextures[i] == sid)
                    pContext->aSidActiveTextures[i] = SVGA3D_INVALID_ID;
            for (uint32_t i = 0; i < RT_ELEMENTS(pContext->state.aRenderTargets); ++i)
                if (pContext->state.aRenderTargets[i] == sid)
                    pContext->state.aRenderTargets[i] = SVGA3D_INVALID_ID;
        }
    }

    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    if (pSvgaR3State->pFuncs3D)
        pSvgaR3State->pFuncs3D->pfnSurfaceDestroy(pThisCC, pSurface);

    if (pSurface->paMipmapLevels)
    {
        for (uint32_t i = 0; i < pSurface->cLevels * pSurface->cFaces; ++i)
            RTMemFreeZ(pSurface->paMipmapLevels[i].pSurfaceData, pSurface->paMipmapLevels[i].cbSurface);
        RTMemFree(pSurface->paMipmapLevels);
    }

    memset(pSurface, 0, sizeof(*pSurface));
    pSurface->id = SVGA3D_INVALID_ID;

    return VINF_SUCCESS;
}


/**
 * Implements the SVGA_3D_CMD_SURFACE_STRETCHBLT command (fifo).
 *
 * @returns VBox status code (currently ignored).
 * @param   pThis               The shared VGA/VMSVGA state.
 * @param   pThisCC             The VGA/VMSVGA state for ring-3.
 * @param   pDstSfcImg
 * @param   pDstBox
 * @param   pSrcSfcImg
 * @param   pSrcBox
 * @param   enmMode
 */
int vmsvga3dSurfaceStretchBlt(PVGASTATE pThis, PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pDstSfcImg, SVGA3dBox const *pDstBox,
                              SVGA3dSurfaceImageId const *pSrcSfcImg, SVGA3dBox const *pSrcBox, SVGA3dStretchBltMode enmMode)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    int rc;

    uint32_t const sidSrc = pSrcSfcImg->sid;
    PVMSVGA3DSURFACE pSrcSurface;
    rc = vmsvga3dSurfaceFromSid(pState, sidSrc, &pSrcSurface);
    AssertRCReturn(rc, rc);

    uint32_t const sidDst = pDstSfcImg->sid;
    PVMSVGA3DSURFACE pDstSurface;
    rc = vmsvga3dSurfaceFromSid(pState, sidDst, &pDstSurface);
    AssertRCReturn(rc, rc);

    AssertReturn(pSrcSfcImg->face < pSrcSurface->cFaces, VERR_INVALID_PARAMETER);
    AssertReturn(pSrcSfcImg->mipmap < pSrcSurface->cLevels, VERR_INVALID_PARAMETER);
    AssertReturn(pDstSfcImg->face < pDstSurface->cFaces, VERR_INVALID_PARAMETER);
    AssertReturn(pDstSfcImg->mipmap < pDstSurface->cLevels, VERR_INVALID_PARAMETER);

    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncs3D, VERR_NOT_IMPLEMENTED);

    PVMSVGA3DCONTEXT pContext;
#ifdef VMSVGA3D_OPENGL
    LogFunc(("src sid=%u (%d,%d)(%d,%d) dest sid=%u (%d,%d)(%d,%d) mode=%x\n",
         sidSrc, pSrcBox->x, pSrcBox->y, pSrcBox->x + pSrcBox->w, pSrcBox->y + pSrcBox->h,
         sidDst, pDstBox->x, pDstBox->y, pDstBox->x + pDstBox->w, pDstBox->y + pDstBox->h, enmMode));
    pContext = &pState->SharedCtx;
    VMSVGA3D_SET_CURRENT_CONTEXT(pState, pContext);
#else
    LogFunc(("src sid=%u cid=%u (%d,%d)(%d,%d) dest sid=%u cid=%u (%d,%d)(%d,%d) mode=%x\n",
         sidSrc, pSrcSurface->idAssociatedContext, pSrcBox->x, pSrcBox->y, pSrcBox->x + pSrcBox->w, pSrcBox->y + pSrcBox->h,
         sidDst, pDstSurface->idAssociatedContext, pDstBox->x, pDstBox->y, pDstBox->x + pDstBox->w, pDstBox->y + pDstBox->h, enmMode));

    uint32_t cid = pDstSurface->idAssociatedContext;
    if (cid == SVGA3D_INVALID_ID)
        cid = pSrcSurface->idAssociatedContext;

    /* At least one of surfaces must be in hardware. */
    AssertReturn(cid != SVGA3D_INVALID_ID, VERR_INVALID_PARAMETER);

    rc = vmsvga3dContextFromCid(pState, cid, &pContext);
    AssertRCReturn(rc, rc);
#endif

    if (!VMSVGA3DSURFACE_HAS_HW_SURFACE(pSrcSurface))
    {
        /* Unknown surface type; turn it into a texture, which can be used for other purposes too. */
        LogFunc(("unknown src sid=%u type=%d format=%d -> create texture\n", sidSrc, pSrcSurface->surfaceFlags, pSrcSurface->format));
        rc = pSvgaR3State->pFuncs3D->pfnCreateTexture(pThisCC, pContext, pContext->id, pSrcSurface);
        AssertRCReturn(rc, rc);
    }

    if (!VMSVGA3DSURFACE_HAS_HW_SURFACE(pDstSurface))
    {
        /* Unknown surface type; turn it into a texture, which can be used for other purposes too. */
        LogFunc(("unknown dest sid=%u type=%d format=%d -> create texture\n", sidDst, pDstSurface->surfaceFlags, pDstSurface->format));
        rc = pSvgaR3State->pFuncs3D->pfnCreateTexture(pThisCC, pContext, pContext->id, pDstSurface);
        AssertRCReturn(rc, rc);
    }

    PVMSVGA3DMIPMAPLEVEL pSrcMipmapLevel;
    rc = vmsvga3dMipmapLevel(pSrcSurface, pSrcSfcImg->face, pSrcSfcImg->mipmap, &pSrcMipmapLevel);
    AssertRCReturn(rc, rc);

    PVMSVGA3DMIPMAPLEVEL pDstMipmapLevel;
    rc = vmsvga3dMipmapLevel(pDstSurface, pDstSfcImg->face, pDstSfcImg->mipmap, &pDstMipmapLevel);
    AssertRCReturn(rc, rc);

    SVGA3dBox clipSrcBox = *pSrcBox;
    SVGA3dBox clipDstBox = *pDstBox;
    vmsvgaR3ClipBox(&pSrcMipmapLevel->mipmapSize, &clipSrcBox);
    vmsvgaR3ClipBox(&pDstMipmapLevel->mipmapSize, &clipDstBox);

    return pSvgaR3State->pFuncs3D->pfnSurfaceStretchBlt(pThis, pState,
                                         pDstSurface, pDstSfcImg->face, pDstSfcImg->mipmap, &clipDstBox,
                                         pSrcSurface, pSrcSfcImg->face, pSrcSfcImg->mipmap, &clipSrcBox,
                                         enmMode, pContext);
}

/**
 * Implements the SVGA_3D_CMD_SURFACE_DMA command (fifo).
 *
 * @returns VBox status code (currently ignored).
 * @param   pThis               The shared VGA/VMSVGA instance data.
 * @param   pThisCC             The VGA/VMSVGA state for ring-3.
 * @param   guest               .
 * @param   host                .
 * @param   transfer            .
 * @param   cCopyBoxes          .
 * @param   paBoxes             .
 */
int vmsvga3dSurfaceDMA(PVGASTATE pThis, PVGASTATECC pThisCC, SVGAGuestImage guest, SVGA3dSurfaceImageId host,
                       SVGA3dTransferType transfer, uint32_t cCopyBoxes, SVGA3dCopyBox *paBoxes)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, host.sid, &pSurface);
    AssertRCReturn(rc, rc);

    LogFunc(("%sguestptr gmr=%x offset=%x pitch=%x host sid=%u face=%d mipmap=%d transfer=%s cCopyBoxes=%d\n",
             (pSurface->surfaceFlags & SVGA3D_SURFACE_HINT_TEXTURE) ? "TEXTURE " : "",
             guest.ptr.gmrId, guest.ptr.offset, guest.pitch,
             host.sid, host.face, host.mipmap, (transfer == SVGA3D_WRITE_HOST_VRAM) ? "READ" : "WRITE", cCopyBoxes));

    PVMSVGA3DMIPMAPLEVEL pMipLevel;
    rc = vmsvga3dMipmapLevel(pSurface, host.face, host.mipmap, &pMipLevel);
    AssertRCReturn(rc, rc);

    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncs3D, VERR_NOT_IMPLEMENTED);

    PVMSVGA3DCONTEXT pContext = NULL;
    if (!VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface))
    {
        /*
         * Not realized in host hardware/library yet, we have to work with
         * the copy of the data we've got in VMSVGA3DMIMAPLEVEL::pSurfaceData.
         */
        AssertReturn(pMipLevel->pSurfaceData, VERR_INTERNAL_ERROR);
    }
    else
    {
#ifdef VMSVGA3D_DIRECT3D
        /* Flush the drawing pipeline for this surface as it could be used in a shared context. */
        vmsvga3dSurfaceFlush(pSurface);
#elif defined(VMSVGA3D_D3D11)
        /** @todo */
#else /* VMSVGA3D_OPENGL */
        pContext = &pState->SharedCtx;
        VMSVGA3D_SET_CURRENT_CONTEXT(pState, pContext);
#endif
    }

    /* SVGA_3D_CMD_SURFACE_DMA:
     * "define the 'source' in each copyBox as the guest image and the
     * 'destination' as the host image, regardless of transfer direction."
     */
    for (uint32_t i = 0; i < cCopyBoxes; ++i)
    {
        Log(("Copy box (%s) %d (%d,%d,%d)(%d,%d,%d) dest (%d,%d)\n",
             VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface) ? "hw" : "mem",
             i, paBoxes[i].srcx, paBoxes[i].srcy, paBoxes[i].srcz, paBoxes[i].w, paBoxes[i].h, paBoxes[i].d, paBoxes[i].x, paBoxes[i].y));

        /* Apparently we're supposed to clip it (gmr test sample) */

        /* The copybox's "dest" is coords in the host surface. Verify them against the surface's mipmap size. */
        SVGA3dBox hostBox;
        hostBox.x = paBoxes[i].x;
        hostBox.y = paBoxes[i].y;
        hostBox.z = paBoxes[i].z;
        hostBox.w = paBoxes[i].w;
        hostBox.h = paBoxes[i].h;
        hostBox.d = paBoxes[i].d;
        vmsvgaR3ClipBox(&pMipLevel->mipmapSize, &hostBox);

        if (   !hostBox.w
            || !hostBox.h
            || !hostBox.d)
        {
            Log(("Skip empty box\n"));
            continue;
        }
        RT_UNTRUSTED_VALIDATED_FENCE();

        /* Adjust the guest, i.e. "src", point.
         * Do not try to verify them here because vmsvgaR3GmrTransfer takes care of this.
         */
        uint32_t const srcx = paBoxes[i].srcx + (hostBox.x - paBoxes[i].x);
        uint32_t const srcy = paBoxes[i].srcy + (hostBox.y - paBoxes[i].y);
        uint32_t const srcz = paBoxes[i].srcz + (hostBox.z - paBoxes[i].z);

        /* Calculate offsets of the image blocks for the transfer. */
        uint32_t u32HostBlockX;
        uint32_t u32HostBlockY;
        uint32_t u32GuestBlockX;
        uint32_t u32GuestBlockY;
        uint32_t cBlocksX;
        uint32_t cBlocksY;
        if (RT_LIKELY(pSurface->cxBlock == 1 && pSurface->cyBlock == 1))
        {
            u32HostBlockX = hostBox.x;
            u32HostBlockY = hostBox.y;

            u32GuestBlockX = srcx;
            u32GuestBlockY = srcy;

            cBlocksX = hostBox.w;
            cBlocksY = hostBox.h;
        }
        else
        {
            /* Pixels to blocks. */
            u32HostBlockX = hostBox.x / pSurface->cxBlock;
            u32HostBlockY = hostBox.y / pSurface->cyBlock;
            Assert(u32HostBlockX * pSurface->cxBlock == hostBox.x);
            Assert(u32HostBlockY * pSurface->cyBlock == hostBox.y);

            u32GuestBlockX = srcx / pSurface->cxBlock;
            u32GuestBlockY = srcy / pSurface->cyBlock;
            Assert(u32GuestBlockX * pSurface->cxBlock == srcx);
            Assert(u32GuestBlockY * pSurface->cyBlock == srcy);

            cBlocksX = (hostBox.w + pSurface->cxBlock - 1) / pSurface->cxBlock;
            cBlocksY = (hostBox.h + pSurface->cyBlock - 1) / pSurface->cyBlock;
        }

        uint32_t cbGuestPitch = guest.pitch;
        if (cbGuestPitch == 0)
        {
            /* Host must "assume image is tightly packed". Our surfaces are. */
            cbGuestPitch = pMipLevel->cbSurfacePitch;
        }
        else
        {
            /* vmsvgaR3GmrTransfer will verify the value, just check it is sane. */
            AssertReturn(cbGuestPitch <= SVGA3D_MAX_SURFACE_MEM_SIZE, VERR_INVALID_PARAMETER);
            RT_UNTRUSTED_VALIDATED_FENCE();
        }

        /* srcx, srcy and srcz values are used to calculate the guest offset.
         * The offset will be verified by vmsvgaR3GmrTransfer, so just check for overflows here.
         */
        AssertReturn(srcz < UINT32_MAX / pMipLevel->mipmapSize.height / cbGuestPitch, VERR_INVALID_PARAMETER);
        AssertReturn(u32GuestBlockY < UINT32_MAX / cbGuestPitch, VERR_INVALID_PARAMETER);
        AssertReturn(u32GuestBlockX < UINT32_MAX / pSurface->cbBlock, VERR_INVALID_PARAMETER);
        RT_UNTRUSTED_VALIDATED_FENCE();

        if (   !VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface)
            || VMSVGA3DSURFACE_NEEDS_DATA(pSurface))
        {
            uint64_t uGuestOffset = u32GuestBlockX * pSurface->cbBlock +
                                    u32GuestBlockY * cbGuestPitch +
                                    srcz * pMipLevel->mipmapSize.height * cbGuestPitch;
            AssertReturn(uGuestOffset < UINT32_MAX, VERR_INVALID_PARAMETER);

            /* vmsvga3dSurfaceDefine verifies the surface dimensions and clipBox is within them. */
            uint32_t uHostOffset = u32HostBlockX * pSurface->cbBlock +
                                   u32HostBlockY * pMipLevel->cbSurfacePitch +
                                   hostBox.z * pMipLevel->cbSurfacePlane;
            AssertReturn(uHostOffset < pMipLevel->cbSurface, VERR_INTERNAL_ERROR);

            for (uint32_t z = 0; z < hostBox.d; ++z)
            {
                rc = vmsvgaR3GmrTransfer(pThis,
                                         pThisCC,
                                         transfer,
                                         (uint8_t *)pMipLevel->pSurfaceData,
                                         pMipLevel->cbSurface,
                                         uHostOffset,
                                         (int32_t)pMipLevel->cbSurfacePitch,
                                         guest.ptr,
                                         (uint32_t)uGuestOffset,
                                         cbGuestPitch,
                                         cBlocksX * pSurface->cbBlock,
                                         cBlocksY);
                AssertRC(rc);

                Log4(("first line [z=%d] (updated at offset 0x%x):\n%.*Rhxd\n",
                      z, uHostOffset, pMipLevel->cbSurfacePitch, pMipLevel->pSurfaceData));

                uHostOffset += pMipLevel->cbSurfacePlane;
                uGuestOffset += pMipLevel->mipmapSize.height * cbGuestPitch;
                AssertReturn(uGuestOffset < UINT32_MAX, VERR_INVALID_PARAMETER);
            }
        }

        if (VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface))
        {
            SVGA3dCopyBox clipBox;
            clipBox.x = hostBox.x;
            clipBox.y = hostBox.y;
            clipBox.z = hostBox.z;
            clipBox.w = hostBox.w;
            clipBox.h = hostBox.h;
            clipBox.d = hostBox.d;
            clipBox.srcx = srcx;
            clipBox.srcy = srcy;
            clipBox.srcz = srcz;
            rc = pSvgaR3State->pFuncs3D->pfnSurfaceDMACopyBox(pThis, pThisCC, pState, pSurface, pMipLevel, host.face, host.mipmap,
                                               guest.ptr, cbGuestPitch, transfer,
                                               &clipBox, pContext, rc, i);
            AssertRC(rc);
        }
    }

    if (!VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface))
    {
        pMipLevel->fDirty = true;
        pSurface->fDirty = true;
    }

    return rc;
}

static int vmsvga3dQueryWriteResult(PVGASTATE pThis, PVGASTATECC pThisCC, SVGAGuestPtr const *pGuestResult,
                                    SVGA3dQueryState enmState, uint32_t u32Result)
{
    SVGA3dQueryResult queryResult;
    queryResult.totalSize = sizeof(queryResult);    /* Set by guest before query is ended. */
    queryResult.state = enmState;                   /* Set by host or guest. See SVGA3dQueryState. */
    queryResult.result32 = u32Result;

    int rc = vmsvgaR3GmrTransfer(pThis, pThisCC, SVGA3D_READ_HOST_VRAM,
                                 (uint8_t *)&queryResult, sizeof(queryResult), 0, sizeof(queryResult),
                                 *pGuestResult, 0, sizeof(queryResult), sizeof(queryResult), 1);
    AssertRC(rc);
    return rc;
}

/* Used with saved state. */
int vmsvga3dQueryCreate(PVGASTATECC pThisCC, uint32_t cid, SVGA3dQueryType type)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    LogFunc(("cid=%u type=%d\n", cid, type));

    PVMSVGA3DCONTEXT pContext;
    int rc = vmsvga3dContextFromCid(pState, cid, &pContext);
    AssertRCReturn(rc, rc);

    if (type == SVGA3D_QUERYTYPE_OCCLUSION)
    {
        VMSVGA3DQUERY *p = &pContext->occlusion;
        if (!VMSVGA3DQUERY_EXISTS(p))
        {
            rc = pSvgaR3State->pFuncsVGPU9->pfnOcclusionQueryCreate(pThisCC, pContext);
            AssertRCReturn(rc, rc);
        }

        return VINF_SUCCESS;
    }

    /* Nothing else for VGPU9. */
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}

int vmsvga3dQueryBegin(PVGASTATECC pThisCC, uint32_t cid, SVGA3dQueryType type)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    LogFunc(("cid=%u type=%d\n", cid, type));

    PVMSVGA3DCONTEXT pContext;
    int rc = vmsvga3dContextFromCid(pState, cid, &pContext);
    AssertRCReturn(rc, rc);

    if (type == SVGA3D_QUERYTYPE_OCCLUSION)
    {
        VMSVGA3DQUERY *p = &pContext->occlusion;
        if (!VMSVGA3DQUERY_EXISTS(p))
        {
            /* Lazy creation of the query object. */
            rc = pSvgaR3State->pFuncsVGPU9->pfnOcclusionQueryCreate(pThisCC, pContext);
            AssertRCReturn(rc, rc);
        }

        rc = pSvgaR3State->pFuncsVGPU9->pfnOcclusionQueryBegin(pThisCC, pContext);
        AssertRCReturn(rc, rc);

        p->enmQueryState = VMSVGA3DQUERYSTATE_BUILDING;
        p->u32QueryResult = 0;

        return VINF_SUCCESS;
    }

    /* Nothing else for VGPU9. */
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}

int vmsvga3dQueryEnd(PVGASTATECC pThisCC, uint32_t cid, SVGA3dQueryType type)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    LogFunc(("cid=%u type=%d\n", cid, type));

    PVMSVGA3DCONTEXT pContext;
    int rc = vmsvga3dContextFromCid(pState, cid, &pContext);
    AssertRCReturn(rc, rc);

    if (type == SVGA3D_QUERYTYPE_OCCLUSION)
    {
        VMSVGA3DQUERY *p = &pContext->occlusion;
        Assert(p->enmQueryState == VMSVGA3DQUERYSTATE_BUILDING);
        AssertMsgReturn(VMSVGA3DQUERY_EXISTS(p), ("Query is NULL\n"), VERR_INTERNAL_ERROR);

        rc = pSvgaR3State->pFuncsVGPU9->pfnOcclusionQueryEnd(pThisCC, pContext);
        AssertRCReturn(rc, rc);

        p->enmQueryState = VMSVGA3DQUERYSTATE_ISSUED;
        return VINF_SUCCESS;
    }

    /* Nothing else for VGPU9. */
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}

int vmsvga3dQueryWait(PVGASTATECC pThisCC, uint32_t cid, SVGA3dQueryType type, PVGASTATE pThis, SVGAGuestPtr const *pGuestResult)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    LogFunc(("cid=%u type=%d guestResult GMR%d:0x%x\n", cid, type, pGuestResult->gmrId, pGuestResult->offset));

    PVMSVGA3DCONTEXT pContext;
    int rc = vmsvga3dContextFromCid(pState, cid, &pContext);
    AssertRCReturn(rc, rc);

    if (type == SVGA3D_QUERYTYPE_OCCLUSION)
    {
        VMSVGA3DQUERY *p = &pContext->occlusion;
        if (VMSVGA3DQUERY_EXISTS(p))
        {
            if (p->enmQueryState == VMSVGA3DQUERYSTATE_ISSUED)
            {
                /* Only if not already in SIGNALED state,
                 * i.e. not a second read from the guest or after restoring saved state.
                 */
                uint32_t u32Pixels = 0;
                rc = pSvgaR3State->pFuncsVGPU9->pfnOcclusionQueryGetData(pThisCC, pContext, &u32Pixels);
                if (RT_SUCCESS(rc))
                {
                    p->enmQueryState = VMSVGA3DQUERYSTATE_SIGNALED;
                    p->u32QueryResult += u32Pixels; /* += because it might contain partial result from saved state. */
                }
            }

            if (RT_SUCCESS(rc))
            {
                /* pGuestResult can be NULL when saving the state. */
                if (pGuestResult)
                {
                    /* Return data to the guest. */
                    vmsvga3dQueryWriteResult(pThis, pThisCC, pGuestResult, SVGA3D_QUERYSTATE_SUCCEEDED, p->u32QueryResult);
                }
                return VINF_SUCCESS;
            }
        }
        else
        {
            AssertMsgFailed(("GetData Query is NULL\n"));
        }

        rc = VERR_INTERNAL_ERROR;
    }
    else
    {
        rc = VERR_NOT_IMPLEMENTED;
    }

    if (pGuestResult)
        vmsvga3dQueryWriteResult(pThis, pThisCC, pGuestResult, SVGA3D_QUERYSTATE_FAILED, 0);
    AssertFailedReturn(rc);
}

int vmsvga3dSurfaceBlitToScreen(PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t idDstScreen, SVGASignedRect destRect,
                                SVGA3dSurfaceImageId srcImage, SVGASignedRect srcRect, uint32_t cRects, SVGASignedRect *pRect)
{
    /* Requires SVGA_FIFO_CAP_SCREEN_OBJECT support */
    LogFunc(("dest=%d (%d,%d)(%d,%d) sid=%u (face=%d, mipmap=%d) (%d,%d)(%d,%d) cRects=%d\n",
             idDstScreen, destRect.left, destRect.top, destRect.right, destRect.bottom, srcImage.sid, srcImage.face, srcImage.mipmap,
             srcRect.left, srcRect.top, srcRect.right, srcRect.bottom, cRects));
    for (uint32_t i = 0; i < cRects; i++)
    {
        LogFunc(("clipping rect %d (%d,%d)(%d,%d)\n", i, pRect[i].left, pRect[i].top, pRect[i].right, pRect[i].bottom));
    }

    VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, idDstScreen);
    AssertReturn(pScreen, VERR_INTERNAL_ERROR);

    /* vmwgfx driver does not always initialize srcImage.mipmap and srcImage.face. They are assumed to be zero. */
    SVGA3dSurfaceImageId src;
    src.sid = srcImage.sid;
    src.mipmap = 0;
    src.face = 0;

    if (pScreen->pHwScreen)
    {
        /* Use the backend accelerated method, if available. */
        PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
        if (pSvgaR3State->pFuncs3D)
        {
            int rc = pSvgaR3State->pFuncs3D->pfnSurfaceBlitToScreen(pThisCC, pScreen, destRect, src, srcRect, cRects, pRect);
            if (rc == VINF_SUCCESS)
            {
                return VINF_SUCCESS;
            }
        }
    }

    /** @todo scaling */
    AssertReturn(destRect.right - destRect.left == srcRect.right - srcRect.left && destRect.bottom - destRect.top == srcRect.bottom - srcRect.top, VERR_INVALID_PARAMETER);

    SVGA3dCopyBox    box;
    SVGAGuestImage dest;

    box.srcz = 0;
    box.z    = 0;
    box.d    = 1;

    dest.ptr.gmrId  = SVGA_GMR_FRAMEBUFFER;
    dest.ptr.offset = pScreen->offVRAM;
    dest.pitch      = pScreen->cbPitch;

    if (cRects == 0)
    {
        /* easy case; no clipping */

        /* SVGA_3D_CMD_SURFACE_DMA:
         * 'define the "source" in each copyBox as the guest image and the
         * "destination" as the host image, regardless of transfer direction.'
         *
         * Since the BlitToScreen operation transfers from a host surface to the guest VRAM,
         * it must set the copyBox "source" to the guest destination coords and
         * the copyBox "destination" to the host surface source coords.
         */
        /* Host image. */
        box.x       = srcRect.left;
        box.y       = srcRect.top;
        box.w       = srcRect.right - srcRect.left;
        box.h       = srcRect.bottom - srcRect.top;
        /* Guest image. */
        box.srcx    = destRect.left;
        box.srcy    = destRect.top;

        int rc = vmsvga3dSurfaceDMA(pThis, pThisCC, dest, src, SVGA3D_READ_HOST_VRAM, 1, &box);
        AssertRCReturn(rc, rc);

        /* Update the guest image, which is at box.src. */
        vmsvgaR3UpdateScreen(pThisCC, pScreen, box.srcx, box.srcy, box.w, box.h);
    }
    else
    {
        /** @todo merge into one SurfaceDMA call */
        for (uint32_t i = 0; i < cRects; i++)
        {
            /* "The clip rectangle coordinates are measured
             * relative to the top-left corner of destRect."
             * Therefore they are relative to the top-left corner of srcRect as well.
             */

            /* Host image. See 'SVGA_3D_CMD_SURFACE_DMA:' comment in the 'if' branch. */
            box.x    = srcRect.left + pRect[i].left;
            box.y    = srcRect.top  + pRect[i].top;
            box.w    = pRect[i].right - pRect[i].left;
            box.h    = pRect[i].bottom - pRect[i].top;
            /* Guest image. The target screen memory is currently in the guest VRAM. */
            box.srcx = destRect.left + pRect[i].left;
            box.srcy = destRect.top  + pRect[i].top;

            int rc = vmsvga3dSurfaceDMA(pThis, pThisCC, dest, src, SVGA3D_READ_HOST_VRAM, 1, &box);
            AssertRCReturn(rc, rc);

            /* Update the guest image, which is at box.src. */
            vmsvgaR3UpdateScreen(pThisCC, pScreen, box.srcx, box.srcy, box.w, box.h);
        }
    }

    return VINF_SUCCESS;
}

int vmsvga3dCommandPresent(PVGASTATE pThis, PVGASTATECC pThisCC, uint32_t sid, uint32_t cRects, SVGA3dCopyRect *pRect)
{
    /* Deprecated according to svga3d_reg.h. */
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_NO_MEMORY);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, sid, &pSurface);
    AssertRCReturn(rc, rc);

    /** @todo Detect screen from coords? Or split rect to screens? */
    VMSVGASCREENOBJECT *pScreen = vmsvgaR3GetScreenObject(pThisCC, 0);
    AssertReturn(pScreen, VERR_INTERNAL_ERROR);

    /* If there are no recangles specified, just grab a screenful. */
    SVGA3dCopyRect DummyRect;
    if (cRects != 0)
    { /* likely */ }
    else
    {
        /** @todo Find the usecase for this or check what the original device does.
         *        The original code was doing some scaling based on the surface
         *        size... */
        AssertMsgFailed(("No rects to present. Who is doing that and what do they actually expect?\n"));
        DummyRect.x = DummyRect.srcx = 0;
        DummyRect.y = DummyRect.srcy = 0;
        DummyRect.w = pScreen->cWidth;
        DummyRect.h = pScreen->cHeight;
        cRects = 1;
        pRect  = &DummyRect;
    }

    uint32_t i;
    for (i = 0; i < cRects; ++i)
    {
        uint32_t idDstScreen = 0; /** @todo Use virtual coords: SVGA_ID_INVALID. */
        SVGASignedRect destRect;
        destRect.left   = pRect[i].x;
        destRect.top    = pRect[i].y;
        destRect.right  = pRect[i].x + pRect[i].w;
        destRect.bottom = pRect[i].y + pRect[i].h;

        SVGA3dSurfaceImageId src;
        src.sid = sid;
        src.face = 0;
        src.mipmap = 0;

        SVGASignedRect srcRect;
        srcRect.left   = pRect[i].srcx;
        srcRect.top    = pRect[i].srcy;
        srcRect.right  = pRect[i].srcx + pRect[i].w;
        srcRect.bottom = pRect[i].srcy + pRect[i].h;

        /* Entire rect. */
        rc = vmsvga3dSurfaceBlitToScreen(pThis, pThisCC, idDstScreen, destRect, src, srcRect, 0, NULL);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

int vmsvga3dDefineScreen(PVGASTATE pThis, PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncs3D, VERR_NOT_IMPLEMENTED);

    if (pScreen->pHwScreen)
    {
        pSvgaR3State->pFuncs3D->pfnDestroyScreen(pThisCC, pScreen);
    }

    int rc = pSvgaR3State->pFuncs3D->pfnDefineScreen(pThis, pThisCC, pScreen);
    if (RT_SUCCESS(rc))
    {
        LogRelMax(1, ("VMSVGA: using accelerated graphics output\n"));
    }
    return rc;
}

int vmsvga3dDestroyScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncs3D, VERR_NOT_IMPLEMENTED);

    return pSvgaR3State->pFuncs3D->pfnDestroyScreen(pThisCC, pScreen);
}

int vmsvga3dSurfaceInvalidate(PVGASTATECC pThisCC, uint32_t sid, uint32_t face, uint32_t mipmap)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, sid, &pSurface);
    AssertRCReturn(rc, rc);

    if (face == SVGA_ID_INVALID && mipmap == SVGA_ID_INVALID)
    {
        for (uint32_t i = 0; i < pSurface->cLevels * pSurface->cFaces; ++i)
        {
            PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[i];
            pMipmapLevel->fDirty = true;
        }
    }
    else
    {
        PVMSVGA3DMIPMAPLEVEL pMipmapLevel;
        rc = vmsvga3dMipmapLevel(pSurface, face, mipmap, &pMipmapLevel);
        AssertRCReturn(rc, rc);

        pMipmapLevel->fDirty = true;
    }
    pSurface->fDirty = true;

    return rc;
}


/*
 *
 * 3D
 *
 */

int vmsvga3dQueryCaps(PVGASTATECC pThisCC, SVGA3dDevCapIndex idx3dCaps, uint32_t *pu32Val)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncs3D, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncs3D->pfnQueryCaps(pThisCC, idx3dCaps, pu32Val);
}

int vmsvga3dChangeMode(PVGASTATECC pThisCC)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncs3D, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncs3D->pfnChangeMode(pThisCC);
}

int vmsvga3dSurfaceCopy(PVGASTATECC pThisCC, SVGA3dSurfaceImageId dest, SVGA3dSurfaceImageId src, uint32_t cCopyBoxes, SVGA3dCopyBox *pBox)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncs3D, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncs3D->pfnSurfaceCopy(pThisCC, dest, src, cCopyBoxes, pBox);
}

void vmsvga3dUpdateHostScreenViewport(PVGASTATECC pThisCC, uint32_t idScreen, VMSVGAVIEWPORT const *pOldViewport)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturnVoid(pSvgaR3State->pFuncs3D);
    pSvgaR3State->pFuncs3D->pfnUpdateHostScreenViewport(pThisCC, idScreen, pOldViewport);
}


/*
 *
 * VGPU9
 *
 */

int vmsvga3dContextDefine(PVGASTATECC pThisCC, uint32_t cid)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnContextDefine(pThisCC, cid);
}

int vmsvga3dContextDestroy(PVGASTATECC pThisCC, uint32_t cid)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnContextDestroy(pThisCC, cid);
}

int vmsvga3dSetTransform(PVGASTATECC pThisCC, uint32_t cid, SVGA3dTransformType type, float matrix[16])
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetTransform(pThisCC, cid, type, matrix);
}

int vmsvga3dSetZRange(PVGASTATECC pThisCC, uint32_t cid, SVGA3dZRange zRange)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetZRange(pThisCC, cid, zRange);
}

int vmsvga3dSetRenderState(PVGASTATECC pThisCC, uint32_t cid, uint32_t cRenderStates, SVGA3dRenderState *pRenderState)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetRenderState(pThisCC, cid, cRenderStates, pRenderState);
}

int vmsvga3dSetRenderTarget(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRenderTargetType type, SVGA3dSurfaceImageId target)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetRenderTarget(pThisCC, cid, type, target);
}

int vmsvga3dSetTextureState(PVGASTATECC pThisCC, uint32_t cid, uint32_t cTextureStates, SVGA3dTextureState *pTextureState)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetTextureState(pThisCC, cid, cTextureStates, pTextureState);
}

int vmsvga3dSetMaterial(PVGASTATECC pThisCC, uint32_t cid, SVGA3dFace face, SVGA3dMaterial *pMaterial)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetMaterial(pThisCC, cid, face, pMaterial);
}

int vmsvga3dSetLightData(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, SVGA3dLightData *pData)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetLightData(pThisCC, cid, index, pData);
}

int vmsvga3dSetLightEnabled(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, uint32_t enabled)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetLightEnabled(pThisCC, cid, index, enabled);
}

int vmsvga3dSetViewPort(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRect *pRect)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetViewPort(pThisCC, cid, pRect);
}

int vmsvga3dSetClipPlane(PVGASTATECC pThisCC, uint32_t cid,  uint32_t index, float plane[4])
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetClipPlane(pThisCC, cid, index, plane);
}

int vmsvga3dCommandClear(PVGASTATECC pThisCC, uint32_t cid, SVGA3dClearFlag clearFlag, uint32_t color, float depth, uint32_t stencil, uint32_t cRects, SVGA3dRect *pRect)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnCommandClear(pThisCC, cid, clearFlag, color, depth, stencil, cRects, pRect);
}

int vmsvga3dDrawPrimitives(PVGASTATECC pThisCC, uint32_t cid, uint32_t numVertexDecls, SVGA3dVertexDecl *pVertexDecl, uint32_t numRanges, SVGA3dPrimitiveRange *pNumRange, uint32_t cVertexDivisor, SVGA3dVertexDivisor *pVertexDivisor)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnDrawPrimitives(pThisCC, cid, numVertexDecls, pVertexDecl, numRanges, pNumRange, cVertexDivisor, pVertexDivisor);
}

int vmsvga3dSetScissorRect(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRect *pRect)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnSetScissorRect(pThisCC, cid, pRect);
}

int vmsvga3dGenerateMipmaps(PVGASTATECC pThisCC, uint32_t sid, SVGA3dTextureFilter filter)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnGenerateMipmaps(pThisCC, sid, filter);
}

int vmsvga3dShaderDefine(PVGASTATECC pThisCC, uint32_t cid, uint32_t shid, SVGA3dShaderType type, uint32_t cbData, uint32_t *pShaderData)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnShaderDefine(pThisCC, cid, shid, type, cbData, pShaderData);
}

int vmsvga3dShaderDestroy(PVGASTATECC pThisCC, uint32_t cid, uint32_t shid, SVGA3dShaderType type)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnShaderDestroy(pThisCC, cid, shid, type);
}

int vmsvga3dShaderSet(PVGASTATECC pThisCC, struct VMSVGA3DCONTEXT *pContext, uint32_t cid, SVGA3dShaderType type, uint32_t shid)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnShaderSet(pThisCC, pContext, cid, type, shid);
}

int vmsvga3dShaderSetConst(PVGASTATECC pThisCC, uint32_t cid, uint32_t reg, SVGA3dShaderType type, SVGA3dShaderConstType ctype, uint32_t cRegisters, uint32_t *pValues)
{
    PVMSVGAR3STATE const pSvgaR3State = pThisCC->svga.pSvgaR3State;
    AssertReturn(pSvgaR3State->pFuncsVGPU9, VERR_NOT_IMPLEMENTED);
    return pSvgaR3State->pFuncsVGPU9->pfnShaderSetConst(pThisCC, cid, reg, type, ctype, cRegisters, pValues);
}

