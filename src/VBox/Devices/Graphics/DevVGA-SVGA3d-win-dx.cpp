/* $Id$ */
/** @file
 * DevVMWare - VMWare SVGA device
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VMSVGA
#include <VBox/AssertGuest.h>
#include <VBox/log.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pgm.h>

#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>

#include <VBoxVideo.h> /* required by DevVGA.h */
#include <VBoxVideo3D.h>

/* should go BEFORE any other DevVGA include to make all DevVGA.h config defines be visible */
#include "DevVGA.h"

#include "DevVGA-SVGA.h"
#include "DevVGA-SVGA3d.h"
#include "DevVGA-SVGA3d-internal.h"
#include "DevVGA-SVGA3d-dx-shader.h"

/* d3d11_1.h has a structure field named 'Status' but Status is defined as int on Linux host */
#if defined(Status)
#undef Status
#endif
#include <d3d11_1.h>


#ifdef RT_OS_WINDOWS
# define VBOX_D3D11_LIBRARY_NAME "d3d11"
#else
# define VBOX_D3D11_LIBRARY_NAME "VBoxDxVk"
#endif

#define DX_FORCE_SINGLE_DEVICE

/* This is not available on non Windows hosts. */
#ifndef D3D_RELEASE
# define D3D_RELEASE(a_Ptr) do { if ((a_Ptr)) (a_Ptr)->Release(); (a_Ptr) = NULL; } while (0)
#endif

/** Fake ID for the backend DX context. The context creates all shared textures. */
#define DX_CID_BACKEND UINT32_C(0xfffffffe)

#define DX_RELEASE_ARRAY(a_Count, a_papArray) do { \
    for (uint32_t i = 0; i < (a_Count); ++i) \
        D3D_RELEASE((a_papArray)[i]); \
} while (0)

typedef struct DXDEVICE
{
    ID3D11Device               *pDevice;               /* Device. */
    ID3D11DeviceContext        *pImmediateContext;     /* Corresponding context. */
    IDXGIFactory               *pDxgiFactory;          /* DXGI Factory. */
    D3D_FEATURE_LEVEL           FeatureLevel;

    /* Staging buffer for transfer to surface buffers. */
    ID3D11Buffer              *pStagingBuffer;         /* The staging buffer resource. */
    uint32_t                   cbStagingBuffer;        /* Current size of the staging buffer resource. */
} DXDEVICE;

/* Kind of a texture view. */
typedef enum VMSVGA3DBACKVIEWTYPE
{
    VMSVGA3D_VIEWTYPE_NONE           = 0,
    VMSVGA3D_VIEWTYPE_RENDERTARGET   = 1,
    VMSVGA3D_VIEWTYPE_DEPTHSTENCIL   = 2,
    VMSVGA3D_VIEWTYPE_SHADERRESOURCE = 3
} VMSVGA3DBACKVIEWTYPE;

/* Information about a texture view to track all created views:.
 * when a surface is invalidated, then all views must deleted;
 * when a view is deleted, then the view must be unlinked from the surface.
 */
typedef struct DXVIEWINFO
{
    uint32_t sid;                                      /* Surface which the view was created for. */
    uint32_t cid;                                      /* DX context which created the view. */
    uint32_t viewId;                                   /* View id assigned by the guest. */
    VMSVGA3DBACKVIEWTYPE enmViewType;
} DXVIEWINFO;

/* Context Object Table element for a texture view. */
typedef struct DXVIEW
{
    uint32_t cid;                                      /* DX context which created the view. */
    uint32_t sid;                                      /* Surface which the view was created for. */
    uint32_t viewId;                                   /* View id assigned by the guest. */
    VMSVGA3DBACKVIEWTYPE enmViewType;

    union
    {
        ID3D11View               *pView;               /* The view object. */
        ID3D11RenderTargetView   *pRenderTargetView;
        ID3D11DepthStencilView   *pDepthStencilView;
        ID3D11ShaderResourceView *pShaderResourceView;
    } u;

    RTLISTNODE nodeSurfaceView;                        /* Views are linked to the surface. */
} DXVIEW;

/* What kind of resource has been created for the VMSVGA3D surface. */
typedef enum VMSVGA3DBACKRESTYPE
{
    VMSVGA3D_RESTYPE_NONE           = 0,
    VMSVGA3D_RESTYPE_SCREEN_TARGET  = 1,
    VMSVGA3D_RESTYPE_TEXTURE_1D     = 2,
    VMSVGA3D_RESTYPE_TEXTURE_2D     = 3,
    VMSVGA3D_RESTYPE_TEXTURE_CUBE   = 4,
    VMSVGA3D_RESTYPE_TEXTURE_3D     = 5,
    VMSVGA3D_RESTYPE_BUFFER         = 6,
} VMSVGA3DBACKRESTYPE;

typedef struct VMSVGA3DBACKENDSURFACE
{
    VMSVGA3DBACKRESTYPE enmResType;
    DXGI_FORMAT enmDxgiFormat;
    union
    {
        ID3D11Resource     *pResource;
        ID3D11Texture1D    *pTexture1D;
        ID3D11Texture2D    *pTexture2D;
        ID3D11Texture3D    *pTexture3D;
        ID3D11Buffer       *pBuffer;
    } u;

    ID3D11Texture2D    *pDynamicTexture;  /* For screen updates from memory. */ /** @todo One for all screens. */
    ID3D11Texture2D    *pStagingTexture;  /* For Reading the screen content. */ /** @todo One for all screens. */
    ID3D11Texture3D    *pDynamicTexture3D;  /* For screen updates from memory. */ /** @todo One for all screens. */
    ID3D11Texture3D    *pStagingTexture3D;  /* For Reading the screen content. */ /** @todo One for all screens. */

    /* Screen targets are created as shared surfaces. */
    HANDLE              SharedHandle;     /* The shared handle of this structure. */

    /* DX context which last rendered to the texture.
     * This is only for render targets and screen targets, which can be shared between contexts.
     * The backend context (cid == DX_CID_BACKEND) can also be a drawing context.
     */
    uint32_t cidDrawing;

    /** AVL tree containing DXSHAREDTEXTURE structures. */
    AVLU32TREE SharedTextureTree;

    union
    {
       struct
       {
           /* Render target views, depth stencil views and shader resource views created for this texture. */
           RTLISTANCHOR listView;                 /* DXVIEW */
       } Texture;
    } u2;

} VMSVGA3DBACKENDSURFACE;

/* "The only resources that can be shared are 2D non-mipmapped textures." */
typedef struct DXSHAREDTEXTURE
{
    AVLU32NODECORE              Core;             /* Key is context id which opened this texture. */
    ID3D11Texture2D            *pTexture;         /* The opened shared texture. */
    uint32_t sid;                                 /* Surface id. */
} DXSHAREDTEXTURE;


typedef struct VMSVGAHWSCREEN
{
    ID3D11Texture2D            *pTexture;         /* Shared texture for the screen content. Only used as CopyResource target. */
    IDXGIResource              *pDxgiResource;    /* Interface of the texture. */
    IDXGIKeyedMutex            *pDXGIKeyedMutex;  /* Synchronization interface for the render device. */
    HANDLE                      SharedHandle;     /* The shared handle of this structure. */
    uint32_t                    sidScreenTarget;  /* The source surface for this screen. */
} VMSVGAHWSCREEN;


typedef struct DXELEMENTLAYOUT
{
    ID3D11InputLayout          *pElementLayout;
    uint32_t                    cElementDesc;
    D3D11_INPUT_ELEMENT_DESC    aElementDesc[32];
} DXELEMENTLAYOUT;

typedef struct DXSHADER
{
    SVGA3dShaderType            enmShaderType;
    union
    {
        ID3D11DeviceChild      *pShader;            /* All. */
        ID3D11VertexShader     *pVertexShader;      /* SVGA3D_SHADERTYPE_VS */
        ID3D11PixelShader      *pPixelShader;       /* SVGA3D_SHADERTYPE_PS */
        ID3D11GeometryShader   *pGeometryShader;    /* SVGA3D_SHADERTYPE_GS */
        ID3D11HullShader       *pHullShader;        /* SVGA3D_SHADERTYPE_HS */
        ID3D11DomainShader     *pDomainShader;      /* SVGA3D_SHADERTYPE_DS */
        ID3D11ComputeShader    *pComputeShader;     /* SVGA3D_SHADERTYPE_CS */
    };
    void                       *pvDXBC;
    uint32_t                    cbDXBC;

    uint32_t                    soid;               /* Stream output declarations for geometry shaders. */

    DXShaderInfo                shaderInfo;
} DXSHADER;

typedef struct DXSTREAMOUTPUT
{
    UINT                       cDeclarationEntry;
    D3D11_SO_DECLARATION_ENTRY aDeclarationEntry[SVGA3D_MAX_STREAMOUT_DECLS];
} DXSTREAMOUTPUT;

typedef struct VMSVGA3DBACKENDDXCONTEXT
{
    DXDEVICE                   dxDevice;               /* DX device interfaces for this context operations. */

    /* Arrays for Context-Object Tables. Number of entries depends on COTable size. */
    uint32_t                   cBlendState;            /* Number of entries in the papBlendState array. */
    uint32_t                   cDepthStencilState;     /* papDepthStencilState */
    uint32_t                   cSamplerState;          /* papSamplerState */
    uint32_t                   cRasterizerState;       /* papRasterizerState */
    uint32_t                   cElementLayout;         /* papElementLayout */
    uint32_t                   cRenderTargetView;      /* papRenderTargetView */
    uint32_t                   cDepthStencilView;      /* papDepthStencilView */
    uint32_t                   cShaderResourceView;    /* papShaderResourceView */
    uint32_t                   cQuery;                 /* papQuery */
    uint32_t                   cShader;                /* papShader */
    uint32_t                   cStreamOutput;          /* papStreamOutput */
    ID3D11BlendState         **papBlendState;
    ID3D11DepthStencilState  **papDepthStencilState;
    ID3D11SamplerState       **papSamplerState;
    ID3D11RasterizerState    **papRasterizerState;
    DXELEMENTLAYOUT           *paElementLayout;
    DXVIEW                    *paRenderTargetView;
    DXVIEW                    *paDepthStencilView;
    DXVIEW                    *paShaderResourceView;
    ID3D11Query              **papQuery;
    DXSHADER                  *paShader;
    DXSTREAMOUTPUT            *paStreamOutput;

    uint32_t                   cSOTarget;              /* How many SO targets are currently set (SetSOTargets) */
} VMSVGA3DBACKENDDXCONTEXT;

/* Shader disassembler function. Optional. */
typedef HRESULT FN_D3D_DISASSEMBLE(LPCVOID pSrcData, SIZE_T SrcDataSize, UINT Flags, LPCSTR szComments, ID3D10Blob **ppDisassembly);
typedef FN_D3D_DISASSEMBLE *PFN_D3D_DISASSEMBLE;

typedef struct VMSVGA3DBACKEND
{
    RTLDRMOD                   hD3D11;
    PFN_D3D11_CREATE_DEVICE    pfnD3D11CreateDevice;

    RTLDRMOD                   hD3DCompiler;
    PFN_D3D_DISASSEMBLE        pfnD3DDisassemble;

    DXDEVICE                   dxDevice;               /* Device for the VMSVGA3D context independent operation. */

    bool                       fSingleDevice;          /* Whether to use one DX device for all guest contexts. */

    /** @todo Here a set of functions which do different job in single and multiple device modes. */
} VMSVGA3DBACKEND;


/* Static function prototypes. */
static int dxDeviceFlush(DXDEVICE *pDevice);
static int dxDefineShaderResourceView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderResourceViewId shaderResourceViewId, SVGACOTableDXSRViewEntry const *pEntry);
static int dxDefineRenderTargetView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRenderTargetViewId renderTargetViewId, SVGACOTableDXRTViewEntry const *pEntry);
static int dxDefineDepthStencilView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilViewId depthStencilViewId, SVGACOTableDXDSViewEntry const *pEntry);
static int dxSetRenderTargets(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext);
static DECLCALLBACK(void) vmsvga3dBackSurfaceDestroy(PVGASTATECC pThisCC, PVMSVGA3DSURFACE pSurface);
static int dxDestroyShader(DXSHADER *pDXShader);


/* This is not available with the DXVK headers for some reason. */
#ifndef RT_OS_WINDOWS
typedef enum D3D11_TEXTURECUBE_FACE {
  D3D11_TEXTURECUBE_FACE_POSITIVE_X,
  D3D11_TEXTURECUBE_FACE_NEGATIVE_X,
  D3D11_TEXTURECUBE_FACE_POSITIVE_Y,
  D3D11_TEXTURECUBE_FACE_NEGATIVE_Y,
  D3D11_TEXTURECUBE_FACE_POSITIVE_Z,
  D3D11_TEXTURECUBE_FACE_NEGATIVE_Z
} D3D11_TEXTURECUBE_FACE;
#endif


DECLINLINE(D3D11_TEXTURECUBE_FACE) vmsvga3dCubemapFaceFromIndex(uint32_t iFace)
{
    D3D11_TEXTURECUBE_FACE Face;
    switch (iFace)
    {
        case 0: Face = D3D11_TEXTURECUBE_FACE_POSITIVE_X; break;
        case 1: Face = D3D11_TEXTURECUBE_FACE_NEGATIVE_X; break;
        case 2: Face = D3D11_TEXTURECUBE_FACE_POSITIVE_Y; break;
        case 3: Face = D3D11_TEXTURECUBE_FACE_NEGATIVE_Y; break;
        case 4: Face = D3D11_TEXTURECUBE_FACE_POSITIVE_Z; break;
        default:
        case 5: Face = D3D11_TEXTURECUBE_FACE_NEGATIVE_Z; break;
    }
    return Face;
}

/* This is to workaround issues with X8 formats, because they can't be used in some operations. */
#define DX_REPLACE_X8_WITH_A8
static DXGI_FORMAT vmsvgaDXSurfaceFormat2Dxgi(SVGA3dSurfaceFormat format)
{
    /* Ensure that correct headers are used.
     * SVGA3D_AYUV was equal to 45, then replaced with SVGA3D_FORMAT_DEAD2 = 45, and redefined as SVGA3D_AYUV = 152.
     */
    AssertCompile(SVGA3D_AYUV == 152);

#define DXGI_FORMAT_ DXGI_FORMAT_UNKNOWN
    /** @todo More formats. */
    switch (format)
    {
#ifdef DX_REPLACE_X8_WITH_A8
        case SVGA3D_X8R8G8B8:                   return DXGI_FORMAT_B8G8R8A8_UNORM;
#else
        case SVGA3D_X8R8G8B8:                   return DXGI_FORMAT_B8G8R8X8_UNORM;
#endif
        case SVGA3D_A8R8G8B8:                   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case SVGA3D_R5G6B5:                     return DXGI_FORMAT_B5G6R5_UNORM;
        case SVGA3D_X1R5G5B5:                   return DXGI_FORMAT_B5G5R5A1_UNORM;
        case SVGA3D_A1R5G5B5:                   return DXGI_FORMAT_B5G5R5A1_UNORM;
        case SVGA3D_A4R4G4B4:                   break; // 11.1 return DXGI_FORMAT_B4G4R4A4_UNORM;
        case SVGA3D_Z_D32:                      break;
        case SVGA3D_Z_D16:                      return DXGI_FORMAT_D16_UNORM;
        case SVGA3D_Z_D24S8:                    return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case SVGA3D_Z_D15S1:                    break;
        case SVGA3D_LUMINANCE8:                 return DXGI_FORMAT_;
        case SVGA3D_LUMINANCE4_ALPHA4:          return DXGI_FORMAT_;
        case SVGA3D_LUMINANCE16:                return DXGI_FORMAT_;
        case SVGA3D_LUMINANCE8_ALPHA8:          return DXGI_FORMAT_;
        case SVGA3D_DXT1:                       return DXGI_FORMAT_;
        case SVGA3D_DXT2:                       return DXGI_FORMAT_;
        case SVGA3D_DXT3:                       return DXGI_FORMAT_;
        case SVGA3D_DXT4:                       return DXGI_FORMAT_;
        case SVGA3D_DXT5:                       return DXGI_FORMAT_;
        case SVGA3D_BUMPU8V8:                   return DXGI_FORMAT_;
        case SVGA3D_BUMPL6V5U5:                 return DXGI_FORMAT_;
        case SVGA3D_BUMPX8L8V8U8:               return DXGI_FORMAT_;
        case SVGA3D_FORMAT_DEAD1:               break;
        case SVGA3D_ARGB_S10E5:                 return DXGI_FORMAT_;
        case SVGA3D_ARGB_S23E8:                 return DXGI_FORMAT_;
        case SVGA3D_A2R10G10B10:                return DXGI_FORMAT_;
        case SVGA3D_V8U8:                       return DXGI_FORMAT_;
        case SVGA3D_Q8W8V8U8:                   return DXGI_FORMAT_;
        case SVGA3D_CxV8U8:                     return DXGI_FORMAT_;
        case SVGA3D_X8L8V8U8:                   return DXGI_FORMAT_;
        case SVGA3D_A2W10V10U10:                return DXGI_FORMAT_;
        case SVGA3D_ALPHA8:                     return DXGI_FORMAT_;
        case SVGA3D_R_S10E5:                    return DXGI_FORMAT_;
        case SVGA3D_R_S23E8:                    return DXGI_FORMAT_;
        case SVGA3D_RG_S10E5:                   return DXGI_FORMAT_;
        case SVGA3D_RG_S23E8:                   return DXGI_FORMAT_;
        case SVGA3D_BUFFER:                     return DXGI_FORMAT_;
        case SVGA3D_Z_D24X8:                    return DXGI_FORMAT_;
        case SVGA3D_V16U16:                     return DXGI_FORMAT_;
        case SVGA3D_G16R16:                     return DXGI_FORMAT_;
        case SVGA3D_A16B16G16R16:               return DXGI_FORMAT_;
        case SVGA3D_UYVY:                       return DXGI_FORMAT_;
        case SVGA3D_YUY2:                       return DXGI_FORMAT_;
        case SVGA3D_NV12:                       return DXGI_FORMAT_;
        case SVGA3D_FORMAT_DEAD2:               break; /* Old SVGA3D_AYUV */
        case SVGA3D_R32G32B32A32_TYPELESS:      return DXGI_FORMAT_R32G32B32A32_TYPELESS;
        case SVGA3D_R32G32B32A32_UINT:          return DXGI_FORMAT_R32G32B32A32_UINT;
        case SVGA3D_R32G32B32A32_SINT:          return DXGI_FORMAT_R32G32B32A32_SINT;
        case SVGA3D_R32G32B32_TYPELESS:         return DXGI_FORMAT_R32G32B32_TYPELESS;
        case SVGA3D_R32G32B32_FLOAT:            return DXGI_FORMAT_R32G32B32_FLOAT;
        case SVGA3D_R32G32B32_UINT:             return DXGI_FORMAT_R32G32B32_UINT;
        case SVGA3D_R32G32B32_SINT:             return DXGI_FORMAT_R32G32B32_SINT;
        case SVGA3D_R16G16B16A16_TYPELESS:      return DXGI_FORMAT_R16G16B16A16_TYPELESS;
        case SVGA3D_R16G16B16A16_UINT:          return DXGI_FORMAT_R16G16B16A16_UINT;
        case SVGA3D_R16G16B16A16_SNORM:         return DXGI_FORMAT_R16G16B16A16_SNORM;
        case SVGA3D_R16G16B16A16_SINT:          return DXGI_FORMAT_R16G16B16A16_SINT;
        case SVGA3D_R32G32_TYPELESS:            return DXGI_FORMAT_R32G32_TYPELESS;
        case SVGA3D_R32G32_UINT:                return DXGI_FORMAT_R32G32_UINT;
        case SVGA3D_R32G32_SINT:                return DXGI_FORMAT_R32G32_SINT;
        case SVGA3D_R32G8X24_TYPELESS:          return DXGI_FORMAT_R32G8X24_TYPELESS;
        case SVGA3D_D32_FLOAT_S8X24_UINT:       return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case SVGA3D_R32_FLOAT_X8X24:            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        case SVGA3D_X32_G8X24_UINT:             return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        case SVGA3D_R10G10B10A2_TYPELESS:       return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case SVGA3D_R10G10B10A2_UINT:           return DXGI_FORMAT_R10G10B10A2_UINT;
        case SVGA3D_R11G11B10_FLOAT:            return DXGI_FORMAT_R11G11B10_FLOAT;
        case SVGA3D_R8G8B8A8_TYPELESS:          return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        case SVGA3D_R8G8B8A8_UNORM:             return DXGI_FORMAT_R8G8B8A8_UNORM;
        case SVGA3D_R8G8B8A8_UNORM_SRGB:        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case SVGA3D_R8G8B8A8_UINT:              return DXGI_FORMAT_R8G8B8A8_UINT;
        case SVGA3D_R8G8B8A8_SINT:              return DXGI_FORMAT_R8G8B8A8_SINT;
        case SVGA3D_R16G16_TYPELESS:            return DXGI_FORMAT_R16G16_TYPELESS;
        case SVGA3D_R16G16_UINT:                return DXGI_FORMAT_R16G16_UINT;
        case SVGA3D_R16G16_SINT:                return DXGI_FORMAT_R16G16_SINT;
        case SVGA3D_R32_TYPELESS:               return DXGI_FORMAT_R32_TYPELESS;
        case SVGA3D_D32_FLOAT:                  return DXGI_FORMAT_D32_FLOAT;
        case SVGA3D_R32_UINT:                   return DXGI_FORMAT_R32_UINT;
        case SVGA3D_R32_SINT:                   return DXGI_FORMAT_R32_SINT;
        case SVGA3D_R24G8_TYPELESS:             return DXGI_FORMAT_R24G8_TYPELESS;
        case SVGA3D_D24_UNORM_S8_UINT:          return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case SVGA3D_R24_UNORM_X8:               return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case SVGA3D_X24_G8_UINT:                return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        case SVGA3D_R8G8_TYPELESS:              return DXGI_FORMAT_R8G8_TYPELESS;
        case SVGA3D_R8G8_UNORM:                 return DXGI_FORMAT_R8G8_UNORM;
        case SVGA3D_R8G8_UINT:                  return DXGI_FORMAT_R8G8_UINT;
        case SVGA3D_R8G8_SINT:                  return DXGI_FORMAT_R8G8_SINT;
        case SVGA3D_R16_TYPELESS:               return DXGI_FORMAT_R16_TYPELESS;
        case SVGA3D_R16_UNORM:                  return DXGI_FORMAT_R16_UNORM;
        case SVGA3D_R16_UINT:                   return DXGI_FORMAT_R16_UINT;
        case SVGA3D_R16_SNORM:                  return DXGI_FORMAT_R16_SNORM;
        case SVGA3D_R16_SINT:                   return DXGI_FORMAT_R16_SINT;
        case SVGA3D_R8_TYPELESS:                return DXGI_FORMAT_R8_TYPELESS;
        case SVGA3D_R8_UNORM:                   return DXGI_FORMAT_R8_UNORM;
        case SVGA3D_R8_UINT:                    return DXGI_FORMAT_R8_UINT;
        case SVGA3D_R8_SNORM:                   return DXGI_FORMAT_R8_SNORM;
        case SVGA3D_R8_SINT:                    return DXGI_FORMAT_R8_SINT;
        case SVGA3D_P8:                         break;
        case SVGA3D_R9G9B9E5_SHAREDEXP:         return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case SVGA3D_R8G8_B8G8_UNORM:            return DXGI_FORMAT_R8G8_B8G8_UNORM;
        case SVGA3D_G8R8_G8B8_UNORM:            return DXGI_FORMAT_G8R8_G8B8_UNORM;
        case SVGA3D_BC1_TYPELESS:               return DXGI_FORMAT_BC1_TYPELESS;
        case SVGA3D_BC1_UNORM_SRGB:             return DXGI_FORMAT_BC1_UNORM_SRGB;
        case SVGA3D_BC2_TYPELESS:               return DXGI_FORMAT_BC2_TYPELESS;
        case SVGA3D_BC2_UNORM_SRGB:             return DXGI_FORMAT_BC2_UNORM_SRGB;
        case SVGA3D_BC3_TYPELESS:               return DXGI_FORMAT_BC3_TYPELESS;
        case SVGA3D_BC3_UNORM_SRGB:             return DXGI_FORMAT_BC3_UNORM_SRGB;
        case SVGA3D_BC4_TYPELESS:               return DXGI_FORMAT_BC4_TYPELESS;
        case SVGA3D_ATI1:                       break;
        case SVGA3D_BC4_SNORM:                  return DXGI_FORMAT_BC4_SNORM;
        case SVGA3D_BC5_TYPELESS:               return DXGI_FORMAT_BC5_TYPELESS;
        case SVGA3D_ATI2:                       break;
        case SVGA3D_BC5_SNORM:                  return DXGI_FORMAT_BC5_SNORM;
        case SVGA3D_R10G10B10_XR_BIAS_A2_UNORM: return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;
        case SVGA3D_B8G8R8A8_TYPELESS:          return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        case SVGA3D_B8G8R8A8_UNORM_SRGB:        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#ifdef DX_REPLACE_X8_WITH_A8
        case SVGA3D_B8G8R8X8_TYPELESS:          return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        case SVGA3D_B8G8R8X8_UNORM_SRGB:        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
        case SVGA3D_B8G8R8X8_TYPELESS:          return DXGI_FORMAT_B8G8R8X8_TYPELESS;
        case SVGA3D_B8G8R8X8_UNORM_SRGB:        return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
#endif
        case SVGA3D_Z_DF16:                     break;
        case SVGA3D_Z_DF24:                     break;
        case SVGA3D_Z_D24S8_INT:                return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case SVGA3D_YV12:                       break;
        case SVGA3D_R32G32B32A32_FLOAT:         return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case SVGA3D_R16G16B16A16_FLOAT:         return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case SVGA3D_R16G16B16A16_UNORM:         return DXGI_FORMAT_R16G16B16A16_UNORM;
        case SVGA3D_R32G32_FLOAT:               return DXGI_FORMAT_R32G32_FLOAT;
        case SVGA3D_R10G10B10A2_UNORM:          return DXGI_FORMAT_R10G10B10A2_UNORM;
        case SVGA3D_R8G8B8A8_SNORM:             return DXGI_FORMAT_R8G8B8A8_SNORM;
        case SVGA3D_R16G16_FLOAT:               return DXGI_FORMAT_R16G16_FLOAT;
        case SVGA3D_R16G16_UNORM:               return DXGI_FORMAT_R16G16_UNORM;
        case SVGA3D_R16G16_SNORM:               return DXGI_FORMAT_R16G16_SNORM;
        case SVGA3D_R32_FLOAT:                  return DXGI_FORMAT_R32_FLOAT;
        case SVGA3D_R8G8_SNORM:                 return DXGI_FORMAT_R8G8_SNORM;
        case SVGA3D_R16_FLOAT:                  return DXGI_FORMAT_R16_FLOAT;
        case SVGA3D_D16_UNORM:                  return DXGI_FORMAT_D16_UNORM;
        case SVGA3D_A8_UNORM:                   return DXGI_FORMAT_A8_UNORM;
        case SVGA3D_BC1_UNORM:                  return DXGI_FORMAT_BC1_UNORM;
        case SVGA3D_BC2_UNORM:                  return DXGI_FORMAT_BC2_UNORM;
        case SVGA3D_BC3_UNORM:                  return DXGI_FORMAT_BC3_UNORM;
        case SVGA3D_B5G6R5_UNORM:               return DXGI_FORMAT_B5G6R5_UNORM;
        case SVGA3D_B5G5R5A1_UNORM:             return DXGI_FORMAT_B5G5R5A1_UNORM;
        case SVGA3D_B8G8R8A8_UNORM:             return DXGI_FORMAT_B8G8R8A8_UNORM;
#ifdef DX_REPLACE_X8_WITH_A8
        case SVGA3D_B8G8R8X8_UNORM:             return DXGI_FORMAT_B8G8R8A8_UNORM;
#else
        case SVGA3D_B8G8R8X8_UNORM:             return DXGI_FORMAT_B8G8R8X8_UNORM;
#endif
        case SVGA3D_BC4_UNORM:                  return DXGI_FORMAT_BC4_UNORM;
        case SVGA3D_BC5_UNORM:                  return DXGI_FORMAT_BC5_UNORM;

        case SVGA3D_B4G4R4A4_UNORM:             return DXGI_FORMAT_;
        case SVGA3D_BC6H_TYPELESS:              return DXGI_FORMAT_;
        case SVGA3D_BC6H_UF16:                  return DXGI_FORMAT_;
        case SVGA3D_BC6H_SF16:                  return DXGI_FORMAT_;
        case SVGA3D_BC7_TYPELESS:               return DXGI_FORMAT_;
        case SVGA3D_BC7_UNORM:                  return DXGI_FORMAT_;
        case SVGA3D_BC7_UNORM_SRGB:             return DXGI_FORMAT_;
        case SVGA3D_AYUV:                       return DXGI_FORMAT_;

        case SVGA3D_FORMAT_INVALID:
        case SVGA3D_FORMAT_MAX:                 break;
    }
    // AssertFailed();
    return DXGI_FORMAT_UNKNOWN;
#undef DXGI_FORMAT_
}


static SVGA3dSurfaceFormat vmsvgaDXDevCapSurfaceFmt2Format(SVGA3dDevCapIndex enmDevCap)
{
    switch (enmDevCap)
    {
        case SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8:                 return SVGA3D_X8R8G8B8;
        case SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8:                 return SVGA3D_A8R8G8B8;
        case SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10:              return SVGA3D_A2R10G10B10;
        case SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5:                 return SVGA3D_X1R5G5B5;
        case SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5:                 return SVGA3D_A1R5G5B5;
        case SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4:                 return SVGA3D_A4R4G4B4;
        case SVGA3D_DEVCAP_SURFACEFMT_R5G6B5:                   return SVGA3D_R5G6B5;
        case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16:              return SVGA3D_LUMINANCE16;
        case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8:        return SVGA3D_LUMINANCE8_ALPHA8;
        case SVGA3D_DEVCAP_SURFACEFMT_ALPHA8:                   return SVGA3D_ALPHA8;
        case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8:               return SVGA3D_LUMINANCE8;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_D16:                    return SVGA3D_Z_D16;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8:                  return SVGA3D_Z_D24S8;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8:                  return SVGA3D_Z_D24X8;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT1:                     return SVGA3D_DXT1;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT2:                     return SVGA3D_DXT2;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT3:                     return SVGA3D_DXT3;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT4:                     return SVGA3D_DXT4;
        case SVGA3D_DEVCAP_SURFACEFMT_DXT5:                     return SVGA3D_DXT5;
        case SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8:             return SVGA3D_BUMPX8L8V8U8;
        case SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10:              return SVGA3D_A2W10V10U10;
        case SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8:                 return SVGA3D_BUMPU8V8;
        case SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8:                 return SVGA3D_Q8W8V8U8;
        case SVGA3D_DEVCAP_SURFACEFMT_CxV8U8:                   return SVGA3D_CxV8U8;
        case SVGA3D_DEVCAP_SURFACEFMT_R_S10E5:                  return SVGA3D_R_S10E5;
        case SVGA3D_DEVCAP_SURFACEFMT_R_S23E8:                  return SVGA3D_R_S23E8;
        case SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5:                 return SVGA3D_RG_S10E5;
        case SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8:                 return SVGA3D_RG_S23E8;
        case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5:               return SVGA3D_ARGB_S10E5;
        case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8:               return SVGA3D_ARGB_S23E8;
        case SVGA3D_DEVCAP_SURFACEFMT_V16U16:                   return SVGA3D_V16U16;
        case SVGA3D_DEVCAP_SURFACEFMT_G16R16:                   return SVGA3D_G16R16;
        case SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16:             return SVGA3D_A16B16G16R16;
        case SVGA3D_DEVCAP_SURFACEFMT_UYVY:                     return SVGA3D_UYVY;
        case SVGA3D_DEVCAP_SURFACEFMT_YUY2:                     return SVGA3D_YUY2;
        case SVGA3D_DEVCAP_SURFACEFMT_NV12:                     return SVGA3D_NV12;
        case SVGA3D_DEVCAP_DEAD10:                              return SVGA3D_FORMAT_DEAD2; /* SVGA3D_DEVCAP_SURFACEFMT_AYUV -> SVGA3D_AYUV */
        case SVGA3D_DEVCAP_SURFACEFMT_Z_DF16:                   return SVGA3D_Z_DF16;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_DF24:                   return SVGA3D_Z_DF24;
        case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT:              return SVGA3D_Z_D24S8_INT;
        case SVGA3D_DEVCAP_SURFACEFMT_ATI1:                     return SVGA3D_ATI1;
        case SVGA3D_DEVCAP_SURFACEFMT_ATI2:                     return SVGA3D_ATI2;
        case SVGA3D_DEVCAP_SURFACEFMT_YV12:                     return SVGA3D_YV12;
        default:
            AssertFailed();
            break;
    }
    return SVGA3D_FORMAT_INVALID;
}


static SVGA3dSurfaceFormat vmsvgaDXDevCapDxfmt2Format(SVGA3dDevCapIndex enmDevCap)
{
    switch (enmDevCap)
    {
        case SVGA3D_DEVCAP_DXFMT_X8R8G8B8:                      return SVGA3D_X8R8G8B8;
        case SVGA3D_DEVCAP_DXFMT_A8R8G8B8:                      return SVGA3D_A8R8G8B8;
        case SVGA3D_DEVCAP_DXFMT_R5G6B5:                        return SVGA3D_R5G6B5;
        case SVGA3D_DEVCAP_DXFMT_X1R5G5B5:                      return SVGA3D_X1R5G5B5;
        case SVGA3D_DEVCAP_DXFMT_A1R5G5B5:                      return SVGA3D_A1R5G5B5;
        case SVGA3D_DEVCAP_DXFMT_A4R4G4B4:                      return SVGA3D_A4R4G4B4;
        case SVGA3D_DEVCAP_DXFMT_Z_D32:                         return SVGA3D_Z_D32;
        case SVGA3D_DEVCAP_DXFMT_Z_D16:                         return SVGA3D_Z_D16;
        case SVGA3D_DEVCAP_DXFMT_Z_D24S8:                       return SVGA3D_Z_D24S8;
        case SVGA3D_DEVCAP_DXFMT_Z_D15S1:                       return SVGA3D_Z_D15S1;
        case SVGA3D_DEVCAP_DXFMT_LUMINANCE8:                    return SVGA3D_LUMINANCE8;
        case SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4:             return SVGA3D_LUMINANCE4_ALPHA4;
        case SVGA3D_DEVCAP_DXFMT_LUMINANCE16:                   return SVGA3D_LUMINANCE16;
        case SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8:             return SVGA3D_LUMINANCE8_ALPHA8;
        case SVGA3D_DEVCAP_DXFMT_DXT1:                          return SVGA3D_DXT1;
        case SVGA3D_DEVCAP_DXFMT_DXT2:                          return SVGA3D_DXT2;
        case SVGA3D_DEVCAP_DXFMT_DXT3:                          return SVGA3D_DXT3;
        case SVGA3D_DEVCAP_DXFMT_DXT4:                          return SVGA3D_DXT4;
        case SVGA3D_DEVCAP_DXFMT_DXT5:                          return SVGA3D_DXT5;
        case SVGA3D_DEVCAP_DXFMT_BUMPU8V8:                      return SVGA3D_BUMPU8V8;
        case SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5:                    return SVGA3D_BUMPL6V5U5;
        case SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8:                  return SVGA3D_BUMPX8L8V8U8;
        case SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1:                  return SVGA3D_FORMAT_DEAD1;
        case SVGA3D_DEVCAP_DXFMT_ARGB_S10E5:                    return SVGA3D_ARGB_S10E5;
        case SVGA3D_DEVCAP_DXFMT_ARGB_S23E8:                    return SVGA3D_ARGB_S23E8;
        case SVGA3D_DEVCAP_DXFMT_A2R10G10B10:                   return SVGA3D_A2R10G10B10;
        case SVGA3D_DEVCAP_DXFMT_V8U8:                          return SVGA3D_V8U8;
        case SVGA3D_DEVCAP_DXFMT_Q8W8V8U8:                      return SVGA3D_Q8W8V8U8;
        case SVGA3D_DEVCAP_DXFMT_CxV8U8:                        return SVGA3D_CxV8U8;
        case SVGA3D_DEVCAP_DXFMT_X8L8V8U8:                      return SVGA3D_X8L8V8U8;
        case SVGA3D_DEVCAP_DXFMT_A2W10V10U10:                   return SVGA3D_A2W10V10U10;
        case SVGA3D_DEVCAP_DXFMT_ALPHA8:                        return SVGA3D_ALPHA8;
        case SVGA3D_DEVCAP_DXFMT_R_S10E5:                       return SVGA3D_R_S10E5;
        case SVGA3D_DEVCAP_DXFMT_R_S23E8:                       return SVGA3D_R_S23E8;
        case SVGA3D_DEVCAP_DXFMT_RG_S10E5:                      return SVGA3D_RG_S10E5;
        case SVGA3D_DEVCAP_DXFMT_RG_S23E8:                      return SVGA3D_RG_S23E8;
        case SVGA3D_DEVCAP_DXFMT_BUFFER:                        return SVGA3D_BUFFER;
        case SVGA3D_DEVCAP_DXFMT_Z_D24X8:                       return SVGA3D_Z_D24X8;
        case SVGA3D_DEVCAP_DXFMT_V16U16:                        return SVGA3D_V16U16;
        case SVGA3D_DEVCAP_DXFMT_G16R16:                        return SVGA3D_G16R16;
        case SVGA3D_DEVCAP_DXFMT_A16B16G16R16:                  return SVGA3D_A16B16G16R16;
        case SVGA3D_DEVCAP_DXFMT_UYVY:                          return SVGA3D_UYVY;
        case SVGA3D_DEVCAP_DXFMT_YUY2:                          return SVGA3D_YUY2;
        case SVGA3D_DEVCAP_DXFMT_NV12:                          return SVGA3D_NV12;
        case SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD2:                  return SVGA3D_FORMAT_DEAD2; /* SVGA3D_DEVCAP_DXFMT_AYUV -> SVGA3D_AYUV */
        case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_TYPELESS:         return SVGA3D_R32G32B32A32_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_UINT:             return SVGA3D_R32G32B32A32_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_SINT:             return SVGA3D_R32G32B32A32_SINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32_TYPELESS:            return SVGA3D_R32G32B32_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32_FLOAT:               return SVGA3D_R32G32B32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32_UINT:                return SVGA3D_R32G32B32_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32_SINT:                return SVGA3D_R32G32B32_SINT;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_TYPELESS:         return SVGA3D_R16G16B16A16_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UINT:             return SVGA3D_R16G16B16A16_UINT;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SNORM:            return SVGA3D_R16G16B16A16_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SINT:             return SVGA3D_R16G16B16A16_SINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32_TYPELESS:               return SVGA3D_R32G32_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R32G32_UINT:                   return SVGA3D_R32G32_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32G32_SINT:                   return SVGA3D_R32G32_SINT;
        case SVGA3D_DEVCAP_DXFMT_R32G8X24_TYPELESS:             return SVGA3D_R32G8X24_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_D32_FLOAT_S8X24_UINT:          return SVGA3D_D32_FLOAT_S8X24_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24:               return SVGA3D_R32_FLOAT_X8X24;
        case SVGA3D_DEVCAP_DXFMT_X32_G8X24_UINT:                return SVGA3D_X32_G8X24_UINT;
        case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_TYPELESS:          return SVGA3D_R10G10B10A2_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UINT:              return SVGA3D_R10G10B10A2_UINT;
        case SVGA3D_DEVCAP_DXFMT_R11G11B10_FLOAT:               return SVGA3D_R11G11B10_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_TYPELESS:             return SVGA3D_R8G8B8A8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM:                return SVGA3D_R8G8B8A8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM_SRGB:           return SVGA3D_R8G8B8A8_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UINT:                 return SVGA3D_R8G8B8A8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SINT:                 return SVGA3D_R8G8B8A8_SINT;
        case SVGA3D_DEVCAP_DXFMT_R16G16_TYPELESS:               return SVGA3D_R16G16_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R16G16_UINT:                   return SVGA3D_R16G16_UINT;
        case SVGA3D_DEVCAP_DXFMT_R16G16_SINT:                   return SVGA3D_R16G16_SINT;
        case SVGA3D_DEVCAP_DXFMT_R32_TYPELESS:                  return SVGA3D_R32_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_D32_FLOAT:                     return SVGA3D_D32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R32_UINT:                      return SVGA3D_R32_UINT;
        case SVGA3D_DEVCAP_DXFMT_R32_SINT:                      return SVGA3D_R32_SINT;
        case SVGA3D_DEVCAP_DXFMT_R24G8_TYPELESS:                return SVGA3D_R24G8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_D24_UNORM_S8_UINT:             return SVGA3D_D24_UNORM_S8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8:                  return SVGA3D_R24_UNORM_X8;
        case SVGA3D_DEVCAP_DXFMT_X24_G8_UINT:                   return SVGA3D_X24_G8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R8G8_TYPELESS:                 return SVGA3D_R8G8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R8G8_UNORM:                    return SVGA3D_R8G8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R8G8_UINT:                     return SVGA3D_R8G8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R8G8_SINT:                     return SVGA3D_R8G8_SINT;
        case SVGA3D_DEVCAP_DXFMT_R16_TYPELESS:                  return SVGA3D_R16_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R16_UNORM:                     return SVGA3D_R16_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R16_UINT:                      return SVGA3D_R16_UINT;
        case SVGA3D_DEVCAP_DXFMT_R16_SNORM:                     return SVGA3D_R16_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R16_SINT:                      return SVGA3D_R16_SINT;
        case SVGA3D_DEVCAP_DXFMT_R8_TYPELESS:                   return SVGA3D_R8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_R8_UNORM:                      return SVGA3D_R8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R8_UINT:                       return SVGA3D_R8_UINT;
        case SVGA3D_DEVCAP_DXFMT_R8_SNORM:                      return SVGA3D_R8_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R8_SINT:                       return SVGA3D_R8_SINT;
        case SVGA3D_DEVCAP_DXFMT_P8:                            return SVGA3D_P8;
        case SVGA3D_DEVCAP_DXFMT_R9G9B9E5_SHAREDEXP:            return SVGA3D_R9G9B9E5_SHAREDEXP;
        case SVGA3D_DEVCAP_DXFMT_R8G8_B8G8_UNORM:               return SVGA3D_R8G8_B8G8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_G8R8_G8B8_UNORM:               return SVGA3D_G8R8_G8B8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC1_TYPELESS:                  return SVGA3D_BC1_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC1_UNORM_SRGB:                return SVGA3D_BC1_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_BC2_TYPELESS:                  return SVGA3D_BC2_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC2_UNORM_SRGB:                return SVGA3D_BC2_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_BC3_TYPELESS:                  return SVGA3D_BC3_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC3_UNORM_SRGB:                return SVGA3D_BC3_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_BC4_TYPELESS:                  return SVGA3D_BC4_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_ATI1:                          return SVGA3D_ATI1;
        case SVGA3D_DEVCAP_DXFMT_BC4_SNORM:                     return SVGA3D_BC4_SNORM;
        case SVGA3D_DEVCAP_DXFMT_BC5_TYPELESS:                  return SVGA3D_BC5_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_ATI2:                          return SVGA3D_ATI2;
        case SVGA3D_DEVCAP_DXFMT_BC5_SNORM:                     return SVGA3D_BC5_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R10G10B10_XR_BIAS_A2_UNORM:    return SVGA3D_R10G10B10_XR_BIAS_A2_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_TYPELESS:             return SVGA3D_B8G8R8A8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM_SRGB:           return SVGA3D_B8G8R8A8_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_TYPELESS:             return SVGA3D_B8G8R8X8_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM_SRGB:           return SVGA3D_B8G8R8X8_UNORM_SRGB;
        case SVGA3D_DEVCAP_DXFMT_Z_DF16:                        return SVGA3D_Z_DF16;
        case SVGA3D_DEVCAP_DXFMT_Z_DF24:                        return SVGA3D_Z_DF24;
        case SVGA3D_DEVCAP_DXFMT_Z_D24S8_INT:                   return SVGA3D_Z_D24S8_INT;
        case SVGA3D_DEVCAP_DXFMT_YV12:                          return SVGA3D_YV12;
        case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_FLOAT:            return SVGA3D_R32G32B32A32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_FLOAT:            return SVGA3D_R16G16B16A16_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UNORM:            return SVGA3D_R16G16B16A16_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R32G32_FLOAT:                  return SVGA3D_R32G32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UNORM:             return SVGA3D_R10G10B10A2_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SNORM:                return SVGA3D_R8G8B8A8_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R16G16_FLOAT:                  return SVGA3D_R16G16_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R16G16_UNORM:                  return SVGA3D_R16G16_UNORM;
        case SVGA3D_DEVCAP_DXFMT_R16G16_SNORM:                  return SVGA3D_R16G16_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R32_FLOAT:                     return SVGA3D_R32_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_R8G8_SNORM:                    return SVGA3D_R8G8_SNORM;
        case SVGA3D_DEVCAP_DXFMT_R16_FLOAT:                     return SVGA3D_R16_FLOAT;
        case SVGA3D_DEVCAP_DXFMT_D16_UNORM:                     return SVGA3D_D16_UNORM;
        case SVGA3D_DEVCAP_DXFMT_A8_UNORM:                      return SVGA3D_A8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC1_UNORM:                     return SVGA3D_BC1_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC2_UNORM:                     return SVGA3D_BC2_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC3_UNORM:                     return SVGA3D_BC3_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B5G6R5_UNORM:                  return SVGA3D_B5G6R5_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B5G5R5A1_UNORM:                return SVGA3D_B5G5R5A1_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM:                return SVGA3D_B8G8R8A8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM:                return SVGA3D_B8G8R8X8_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC4_UNORM:                     return SVGA3D_BC4_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC5_UNORM:                     return SVGA3D_BC5_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS:                 return SVGA3D_BC6H_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC6H_UF16:                     return SVGA3D_BC6H_UF16;
        case SVGA3D_DEVCAP_DXFMT_BC6H_SF16:                     return SVGA3D_BC6H_SF16;
        case SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS:                  return SVGA3D_BC7_TYPELESS;
        case SVGA3D_DEVCAP_DXFMT_BC7_UNORM:                     return SVGA3D_BC7_UNORM;
        case SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB:                return SVGA3D_BC7_UNORM_SRGB;
        default:
            AssertFailed();
            break;
    }
    return SVGA3D_FORMAT_INVALID;
}


static int vmsvgaDXCheckFormatSupportPreDX(PVMSVGA3DSTATE pState, SVGA3dSurfaceFormat enmFormat, uint32_t *pu32DevCap)
{
    int rc = VINF_SUCCESS;

    *pu32DevCap = 0;

    DXGI_FORMAT const dxgiFormat = vmsvgaDXSurfaceFormat2Dxgi(enmFormat);
    if (dxgiFormat != DXGI_FORMAT_UNKNOWN)
    {
        RT_NOREF(pState);
        /** @todo Implement */
    }
    else
        rc = VERR_NOT_SUPPORTED;
    return rc;
}

static int vmsvgaDXCheckFormatSupport(PVMSVGA3DSTATE pState, SVGA3dSurfaceFormat enmFormat, uint32_t *pu32DevCap)
{
    int rc = VINF_SUCCESS;

    *pu32DevCap = 0;

    DXGI_FORMAT const dxgiFormat = vmsvgaDXSurfaceFormat2Dxgi(enmFormat);
    if (dxgiFormat != DXGI_FORMAT_UNKNOWN)
    {
        ID3D11Device *pDevice = pState->pBackend->dxDevice.pDevice;
        UINT FormatSupport = 0;
        HRESULT hr = pDevice->CheckFormatSupport(dxgiFormat, &FormatSupport);
        if (SUCCEEDED(hr))
        {
            *pu32DevCap |= SVGA3D_DXFMT_SUPPORTED;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)
                *pu32DevCap |= SVGA3D_DXFMT_SHADER_SAMPLE;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET)
                *pu32DevCap |= SVGA3D_DXFMT_COLOR_RENDERTARGET;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)
                *pu32DevCap |= SVGA3D_DXFMT_DEPTH_RENDERTARGET;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_BLENDABLE)
                *pu32DevCap |= SVGA3D_DXFMT_BLENDABLE;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_MIP)
                *pu32DevCap |= SVGA3D_DXFMT_MIPS;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_TEXTURECUBE)
                *pu32DevCap |= SVGA3D_DXFMT_ARRAY;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_TEXTURE3D)
                *pu32DevCap |= SVGA3D_DXFMT_VOLUME;

            if (FormatSupport & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER)
                *pu32DevCap |= SVGA3D_DXFMT_DX_VERTEX_BUFFER;

            UINT NumQualityLevels;
            hr = pDevice->CheckMultisampleQualityLevels(dxgiFormat, 2, &NumQualityLevels);
            if (SUCCEEDED(hr) && NumQualityLevels != 0)
                *pu32DevCap |= SVGA3D_DXFMT_MULTISAMPLE;
        }
        else
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
    }
    else
        rc = VERR_NOT_SUPPORTED;
    return rc;
}


static int dxDeviceCreate(PVMSVGA3DBACKEND pBackend, DXDEVICE *pDevice)
{
    int rc = VINF_SUCCESS;

    if (pBackend->fSingleDevice && pBackend->dxDevice.pDevice)
    {
        pDevice->pDevice = pBackend->dxDevice.pDevice;
        pDevice->pDevice->AddRef();

        pDevice->pImmediateContext = pBackend->dxDevice.pImmediateContext;
        pDevice->pImmediateContext->AddRef();

        pDevice->pDxgiFactory = pBackend->dxDevice.pDxgiFactory;
        pDevice->pDxgiFactory->AddRef();

        pDevice->FeatureLevel = pBackend->dxDevice.FeatureLevel;

        pDevice->pStagingBuffer = 0;
        pDevice->cbStagingBuffer = 0;

        return rc;
    }

    IDXGIAdapter *pAdapter = NULL; /* Default adapter. */
    static D3D_FEATURE_LEVEL const s_aFeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    UINT Flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
    Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = pBackend->pfnD3D11CreateDevice(pAdapter,
                                                D3D_DRIVER_TYPE_HARDWARE,
                                                NULL,
                                                Flags,
                                                s_aFeatureLevels,
                                                RT_ELEMENTS(s_aFeatureLevels),
                                                D3D11_SDK_VERSION,
                                                &pDevice->pDevice,
                                                &pDevice->FeatureLevel,
                                                &pDevice->pImmediateContext);
    if (SUCCEEDED(hr))
    {
        LogRel(("VMSVGA: Feature level %#x\n", pDevice->FeatureLevel));

#ifdef DEBUG
        /* Break into debugger when DX runtime detects anything unusual. */
        HRESULT hr2;
        ID3D11Debug *pDebug = 0;
        hr2 = pDevice->pDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&pDebug);
        if (SUCCEEDED(hr2))
        {
            ID3D11InfoQueue *pInfoQueue = 0;
            hr2 = pDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&pInfoQueue);
            if (SUCCEEDED(hr2))
            {
                pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
//                pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
//                pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);

                /* No breakpoints for the following messages. */
                D3D11_MESSAGE_ID saIgnoredMessageIds[] =
                {
                    /* Message ID:                                    Caused by: */
                    D3D11_MESSAGE_ID_CREATEINPUTLAYOUT_TYPE_MISMATCH, /* Autogenerated input signatures. */
                    D3D11_MESSAGE_ID_LIVE_DEVICE,                     /* Live object report. Does not seem to prevent a breakpoint. */
                    (D3D11_MESSAGE_ID)3146081 /*DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET*/, /* U. */
                    D3D11_MESSAGE_ID_DEVICE_DRAW_SAMPLER_NOT_SET,     /* U. */
                    D3D11_MESSAGE_ID_DEVICE_DRAW_SAMPLER_MISMATCH,    /* U. */
                    D3D11_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,  /* P. */
                };

                D3D11_INFO_QUEUE_FILTER filter;
                RT_ZERO(filter);
                filter.DenyList.NumIDs = RT_ELEMENTS(saIgnoredMessageIds);
                filter.DenyList.pIDList = saIgnoredMessageIds;
                pInfoQueue->AddStorageFilterEntries(&filter);

                D3D_RELEASE(pInfoQueue);
            }
            D3D_RELEASE(pDebug);
        }
#endif

        IDXGIDevice *pDxgiDevice = 0;
        hr = pDevice->pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice);
        if (SUCCEEDED(hr))
        {
            IDXGIAdapter *pDxgiAdapter = 0;
            hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pDxgiAdapter);
            if (SUCCEEDED(hr))
            {
                hr = pDxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pDevice->pDxgiFactory);
                D3D_RELEASE(pDxgiAdapter);
            }

            D3D_RELEASE(pDxgiDevice);
        }
    }

    if (FAILED(hr))
        rc = VERR_NOT_SUPPORTED;

    return rc;
}


static void dxDeviceDestroy(PVMSVGA3DBACKEND pBackend, DXDEVICE *pDevice)
{
    RT_NOREF(pBackend);

    if (pDevice->pImmediateContext)
    {
        dxDeviceFlush(pDevice); /* Make sure that any pending draw calls are finished. */
        pDevice->pImmediateContext->ClearState();
    }

    D3D_RELEASE(pDevice->pStagingBuffer);

    D3D_RELEASE(pDevice->pDxgiFactory);
    D3D_RELEASE(pDevice->pImmediateContext);

#ifdef DEBUG
    HRESULT hr2;
    ID3D11Debug *pDebug = 0;
    hr2 = pDevice->pDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&pDebug);
    if (SUCCEEDED(hr2))
    {
        /// @todo Use this to see whether all resources have been properly released.
        //DEBUG_BREAKPOINT_TEST();
        //pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | (D3D11_RLDO_FLAGS)0x4 /*D3D11_RLDO_IGNORE_INTERNAL*/);
        D3D_RELEASE(pDebug);
    }
#endif

    D3D_RELEASE(pDevice->pDevice);
    RT_ZERO(*pDevice);
}


static void dxViewAddToList(PVGASTATECC pThisCC, DXVIEW *pDXView)
{
    LogFunc(("cid = %u, sid = %u, viewId = %u, type = %u\n",
             pDXView->cid, pDXView->sid, pDXView->viewId, pDXView->enmViewType));

    Assert(pDXView->u.pView); /* Only already created views should be added. Guard against mis-use by callers. */

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pDXView->sid, &pSurface);
    AssertRCReturnVoid(rc);

    RTListAppend(&pSurface->pBackendSurface->u2.Texture.listView, &pDXView->nodeSurfaceView);
}


static void dxViewRemoveFromList(DXVIEW *pDXView)
{
    LogFunc(("cid = %u, sid = %u, viewId = %u, type = %u\n",
             pDXView->cid, pDXView->sid, pDXView->viewId, pDXView->enmViewType));
    /* pView can be NULL, if COT entry is already empty. */
    if (pDXView->u.pView)
    {
        Assert(pDXView->nodeSurfaceView.pNext && pDXView->nodeSurfaceView.pPrev);
        RTListNodeRemove(&pDXView->nodeSurfaceView);
    }
}


static int dxViewDestroy(DXVIEW *pDXView)
{
    LogFunc(("cid = %u, sid = %u, viewId = %u, type = %u\n",
             pDXView->cid, pDXView->sid, pDXView->viewId, pDXView->enmViewType));
    if (pDXView->u.pView)
    {
        D3D_RELEASE(pDXView->u.pView);
        RTListNodeRemove(&pDXView->nodeSurfaceView);
        RT_ZERO(*pDXView);
    }

    return VINF_SUCCESS;
}


static int dxViewInit(DXVIEW *pDXView, PVMSVGA3DSURFACE pSurface, VMSVGA3DDXCONTEXT *pDXContext, uint32_t viewId, VMSVGA3DBACKVIEWTYPE enmViewType, ID3D11View *pView)
{
    pDXView->cid         = pDXContext->cid;
    pDXView->sid         = pSurface->id;
    pDXView->viewId      = viewId;
    pDXView->enmViewType = enmViewType;
    pDXView->u.pView     = pView;
    RTListAppend(&pSurface->pBackendSurface->u2.Texture.listView, &pDXView->nodeSurfaceView);

    LogFunc(("cid = %u, sid = %u, viewId = %u, type = %u\n",
              pDXView->cid, pDXView->sid, pDXView->viewId, pDXView->enmViewType));

DXVIEW *pIter, *pNext;
RTListForEachSafe(&pSurface->pBackendSurface->u2.Texture.listView, pIter, pNext, DXVIEW, nodeSurfaceView)
{
    AssertPtr(pNext);
    LogFunc(("pIter=%p, pNext=%p\n", pIter, pNext));
}

    return VINF_SUCCESS;
}


DECLINLINE(bool) dxIsSurfaceShareable(PVMSVGA3DSURFACE pSurface)
{
    /* It is not expected that volume textures will be shared between contexts. */
    if (pSurface->surfaceFlags & SVGA3D_SURFACE_VOLUME)
        return false;

    return pSurface->surfaceFlags & SVGA3D_SURFACE_SCREENTARGET
        || pSurface->surfaceFlags & SVGA3D_SURFACE_BIND_RENDER_TARGET;
}


static DXDEVICE *dxDeviceFromCid(uint32_t cid, PVMSVGA3DSTATE pState)
{
    if (cid != DX_CID_BACKEND)
    {
        if (pState->pBackend->fSingleDevice)
            return &pState->pBackend->dxDevice;

        VMSVGA3DDXCONTEXT *pDXContext;
        int rc = vmsvga3dDXContextFromCid(pState, cid, &pDXContext);
        if (RT_SUCCESS(rc))
            return &pDXContext->pBackendDXContext->dxDevice;
    }
    else
        return &pState->pBackend->dxDevice;

    AssertFailed();
    return NULL;
}


static DXDEVICE *dxDeviceFromContext(PVMSVGA3DSTATE p3dState, VMSVGA3DDXCONTEXT *pDXContext)
{
    if (pDXContext && !p3dState->pBackend->fSingleDevice)
        return &pDXContext->pBackendDXContext->dxDevice;

    return &p3dState->pBackend->dxDevice;
}


static int dxDeviceFlush(DXDEVICE *pDevice)
{
    /** @todo Should the flush follow the query submission? */
    pDevice->pImmediateContext->Flush();

    ID3D11Query *pQuery = 0;
    D3D11_QUERY_DESC qd;
    RT_ZERO(qd);
    qd.Query = D3D11_QUERY_EVENT;

    HRESULT hr = pDevice->pDevice->CreateQuery(&qd, &pQuery);
    Assert(hr == S_OK); RT_NOREF(hr);
    pDevice->pImmediateContext->End(pQuery);

    BOOL queryData;
    while (pDevice->pImmediateContext->GetData(pQuery, &queryData, sizeof(queryData), 0) != S_OK)
        RTThreadYield();

    D3D_RELEASE(pQuery);

    return VINF_SUCCESS;
}


static int dxContextWait(uint32_t cidDrawing, PVMSVGA3DSTATE pState)
{
    if (pState->pBackend->fSingleDevice)
      return VINF_SUCCESS;

    /* Flush cidDrawing context and issue a query. */
    DXDEVICE *pDXDevice = dxDeviceFromCid(cidDrawing, pState);
    if (pDXDevice)
        return dxDeviceFlush(pDXDevice);
    /* cidDrawing does not exist anymore. */
    return VINF_SUCCESS;
}


static int dxSurfaceWait(PVMSVGA3DSTATE pState, PVMSVGA3DSURFACE pSurface, uint32_t cidRequesting)
{
    if (pState->pBackend->fSingleDevice)
        return VINF_SUCCESS;

    VMSVGA3DBACKENDSURFACE *pBackendSurface = pSurface->pBackendSurface;
    if (!pBackendSurface)
        AssertFailedReturn(VERR_INVALID_STATE);

    int rc = VINF_SUCCESS;
    if (pBackendSurface->cidDrawing != SVGA_ID_INVALID)
    {
        if (pBackendSurface->cidDrawing != cidRequesting)
        {
            LogFunc(("sid = %u, assoc cid = %u, drawing cid = %u, req cid = %u\n",
                     pSurface->id, pSurface->idAssociatedContext, pBackendSurface->cidDrawing, cidRequesting));
            Assert(dxIsSurfaceShareable(pSurface));
            rc = dxContextWait(pBackendSurface->cidDrawing, pState);
            pBackendSurface->cidDrawing = SVGA_ID_INVALID;
        }
    }
    return rc;
}


static ID3D11Resource *dxResource(PVMSVGA3DSTATE pState, PVMSVGA3DSURFACE pSurface, VMSVGA3DDXCONTEXT *pDXContext)
{
    VMSVGA3DBACKENDSURFACE *pBackendSurface = pSurface->pBackendSurface;
    if (!pBackendSurface)
        AssertFailedReturn(NULL);

    ID3D11Resource *pResource;

    uint32_t const cidRequesting = pDXContext ? pDXContext->cid : DX_CID_BACKEND;
    if (cidRequesting == pSurface->idAssociatedContext || pState->pBackend->fSingleDevice)
        pResource = pBackendSurface->u.pResource;
    else
    {
        /*
         * Context, which as not created the surface, is requesting.
         */
        AssertReturn(pDXContext, NULL);

        Assert(dxIsSurfaceShareable(pSurface));
        Assert(pSurface->idAssociatedContext == DX_CID_BACKEND);

        DXSHAREDTEXTURE *pSharedTexture = (DXSHAREDTEXTURE *)RTAvlU32Get(&pBackendSurface->SharedTextureTree, pDXContext->cid);
        if (!pSharedTexture)
        {
            DXDEVICE *pDevice = dxDeviceFromContext(pState, pDXContext);
            AssertReturn(pDevice->pDevice, NULL);

            AssertReturn(pBackendSurface->SharedHandle, NULL);

            /* This context has not yet opened the texture. */
            pSharedTexture = (DXSHAREDTEXTURE *)RTMemAllocZ(sizeof(DXSHAREDTEXTURE));
            AssertReturn(pSharedTexture, NULL);

            pSharedTexture->Core.Key = pDXContext->cid;
            bool const fSuccess = RTAvlU32Insert(&pBackendSurface->SharedTextureTree, &pSharedTexture->Core);
            AssertReturn(fSuccess, NULL);

            HRESULT hr = pDevice->pDevice->OpenSharedResource(pBackendSurface->SharedHandle, __uuidof(ID3D11Texture2D), (void**)&pSharedTexture->pTexture);
            Assert(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
                pSharedTexture->sid = pSurface->id;
            else
            {
                RTAvlU32Remove(&pBackendSurface->SharedTextureTree, pDXContext->cid);
                RTMemFree(pSharedTexture);
                return NULL;
            }
        }

        pResource = pSharedTexture->pTexture;
    }

    /* Wait for drawing to finish. */
    dxSurfaceWait(pState, pSurface, cidRequesting);

    return pResource;
}


static uint32_t dxGetRenderTargetViewSid(PVMSVGA3DDXCONTEXT pDXContext, uint32_t renderTargetViewId)
{
    ASSERT_GUEST_RETURN(renderTargetViewId < pDXContext->cot.cRTView, SVGA_ID_INVALID);

    SVGACOTableDXRTViewEntry const *pRTViewEntry = &pDXContext->cot.paRTView[renderTargetViewId];
    return pRTViewEntry->sid;
}


static SVGACOTableDXSRViewEntry const *dxGetShaderResourceViewEntry(PVMSVGA3DDXCONTEXT pDXContext, uint32_t shaderResourceViewId)
{
    ASSERT_GUEST_RETURN(shaderResourceViewId < pDXContext->cot.cSRView, NULL);

    SVGACOTableDXSRViewEntry const *pSRViewEntry = &pDXContext->cot.paSRView[shaderResourceViewId];
    return pSRViewEntry;
}


static SVGACOTableDXDSViewEntry const *dxGetDepthStencilViewEntry(PVMSVGA3DDXCONTEXT pDXContext, uint32_t depthStencilViewId)
{
    ASSERT_GUEST_RETURN(depthStencilViewId < pDXContext->cot.cDSView, NULL);

    SVGACOTableDXDSViewEntry const *pDSViewEntry = &pDXContext->cot.paDSView[depthStencilViewId];
    return pDSViewEntry;
}


static SVGACOTableDXRTViewEntry const *dxGetRenderTargetViewEntry(PVMSVGA3DDXCONTEXT pDXContext, uint32_t renderTargetViewId)
{
    ASSERT_GUEST_RETURN(renderTargetViewId < pDXContext->cot.cRTView, NULL);

    SVGACOTableDXRTViewEntry const *pRTViewEntry = &pDXContext->cot.paRTView[renderTargetViewId];
    return pRTViewEntry;
}


static int dxTrackRenderTargets(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    for (unsigned long i = 0; i < RT_ELEMENTS(pDXContext->svgaDXContext.renderState.renderTargetViewIds); ++i)
    {
        uint32_t const renderTargetViewId = pDXContext->svgaDXContext.renderState.renderTargetViewIds[i];
        if (renderTargetViewId == SVGA_ID_INVALID)
            continue;

        uint32_t const sid = dxGetRenderTargetViewSid(pDXContext, renderTargetViewId);
        LogFunc(("[%u] sid = %u, drawing cid = %u\n", i, sid, pDXContext->cid));

        PVMSVGA3DSURFACE pSurface;
        int rc = vmsvga3dSurfaceFromSid(pState, sid, &pSurface);
        if (RT_SUCCESS(rc))
        {
            AssertContinue(pSurface->pBackendSurface);
            pSurface->pBackendSurface->cidDrawing = pDXContext->cid;
        }
    }
    return VINF_SUCCESS;
}


static int dxDefineStreamOutput(PVMSVGA3DDXCONTEXT pDXContext, SVGA3dStreamOutputId soid, SVGACOTableDXStreamOutputEntry const *pEntry)
{
    DXSTREAMOUTPUT *pDXStreamOutput = &pDXContext->pBackendDXContext->paStreamOutput[soid];

    /* Make D3D11_SO_DECLARATION_ENTRY array from SVGA3dStreamOutputDeclarationEntry. */
    pDXStreamOutput->cDeclarationEntry = pEntry->numOutputStreamEntries;
    for (uint32_t i = 0; i < pDXStreamOutput->cDeclarationEntry; ++i)
    {
        D3D11_SO_DECLARATION_ENTRY *pDst = &pDXStreamOutput->aDeclarationEntry[i];
        SVGA3dStreamOutputDeclarationEntry const *pSrc = &pEntry->decl[i];

        uint32_t const registerMask = pSrc->registerMask & 0xF;
        unsigned const iFirstBit = ASMBitFirstSetU32(registerMask);
        unsigned const iLastBit = ASMBitLastSetU32(registerMask);

        pDst->Stream         = pSrc->stream;
        pDst->SemanticName   = NULL; /* Will be taken from the shader output declaration. */
        pDst->SemanticIndex  = 0;
        pDst->StartComponent = iFirstBit > 0 ? iFirstBit - 1 : 0;
        pDst->ComponentCount = iFirstBit > 0 ? iLastBit - (iFirstBit - 1) : 0;
        pDst->OutputSlot     = pSrc->outputSlot;
    }

    return VINF_SUCCESS;
}

static void dxDestroyStreamOutput(DXSTREAMOUTPUT *pDXStreamOutput)
{
    RT_NOREF(pDXStreamOutput);
}

static D3D11_BLEND dxBlendFactorAlpha(uint8_t svgaBlend)
{
    /* "Blend options that end in _COLOR are not allowed." but the guest sometimes sends them. */
    switch (svgaBlend)
    {
        case SVGA3D_BLENDOP_SRCCOLOR:     return D3D11_BLEND_SRC_ALPHA;
        case SVGA3D_BLENDOP_INVSRCCOLOR:  return D3D11_BLEND_INV_SRC_ALPHA;
        case SVGA3D_BLENDOP_DESTCOLOR:    return D3D11_BLEND_DEST_ALPHA;
        case SVGA3D_BLENDOP_INVDESTCOLOR: return D3D11_BLEND_INV_DEST_ALPHA;
        case SVGA3D_BLENDOP_SRC1COLOR:    return D3D11_BLEND_SRC1_ALPHA;
        case SVGA3D_BLENDOP_INVSRC1COLOR: return D3D11_BLEND_INV_SRC1_ALPHA;
        default:
            break;
    }
    return (D3D11_BLEND)svgaBlend;
}


static D3D11_BLEND dxBlendFactorColor(uint8_t svgaBlend)
{
    return (D3D11_BLEND)svgaBlend;
}


static D3D11_BLEND_OP dxBlendOp(uint8_t svgaBlendEq)
{
    return (D3D11_BLEND_OP)svgaBlendEq;
}


/** @todo AssertCompile for types like D3D11_COMPARISON_FUNC and SVGA3dComparisonFunc */
static HRESULT dxBlendStateCreate(DXDEVICE *pDevice, SVGACOTableDXBlendStateEntry const *pEntry, ID3D11BlendState **pp)
{
    D3D11_BLEND_DESC BlendDesc;
    BlendDesc.AlphaToCoverageEnable = RT_BOOL(pEntry->alphaToCoverageEnable);
    BlendDesc.IndependentBlendEnable = RT_BOOL(pEntry->independentBlendEnable);
    for (int i = 0; i < SVGA3D_MAX_RENDER_TARGETS; ++i)
    {
        BlendDesc.RenderTarget[i].BlendEnable           = RT_BOOL(pEntry->perRT[i].blendEnable);
        BlendDesc.RenderTarget[i].SrcBlend              = dxBlendFactorColor(pEntry->perRT[i].srcBlend);
        BlendDesc.RenderTarget[i].DestBlend             = dxBlendFactorColor(pEntry->perRT[i].destBlend);
        BlendDesc.RenderTarget[i].BlendOp               = dxBlendOp         (pEntry->perRT[i].blendOp);
        BlendDesc.RenderTarget[i].SrcBlendAlpha         = dxBlendFactorAlpha(pEntry->perRT[i].srcBlendAlpha);
        BlendDesc.RenderTarget[i].DestBlendAlpha        = dxBlendFactorAlpha(pEntry->perRT[i].destBlendAlpha);
        BlendDesc.RenderTarget[i].BlendOpAlpha          = dxBlendOp         (pEntry->perRT[i].blendOpAlpha);
        BlendDesc.RenderTarget[i].RenderTargetWriteMask = pEntry->perRT[i].renderTargetWriteMask;
        /** @todo logicOpEnable and logicOp */
    }

    HRESULT hr = pDevice->pDevice->CreateBlendState(&BlendDesc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxDepthStencilStateCreate(DXDEVICE *pDevice, SVGACOTableDXDepthStencilEntry const *pEntry, ID3D11DepthStencilState **pp)
{
    D3D11_DEPTH_STENCIL_DESC desc;
    desc.DepthEnable                  = pEntry->depthEnable;
    desc.DepthWriteMask               = (D3D11_DEPTH_WRITE_MASK)pEntry->depthWriteMask;
    desc.DepthFunc                    = (D3D11_COMPARISON_FUNC)pEntry->depthFunc;
    desc.StencilEnable                = pEntry->stencilEnable;
    desc.StencilReadMask              = pEntry->stencilReadMask;
    desc.StencilWriteMask             = pEntry->stencilWriteMask;
    desc.FrontFace.StencilFailOp      = (D3D11_STENCIL_OP)pEntry->frontStencilFailOp;
    desc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)pEntry->frontStencilDepthFailOp;
    desc.FrontFace.StencilPassOp      = (D3D11_STENCIL_OP)pEntry->frontStencilPassOp;
    desc.FrontFace.StencilFunc        = (D3D11_COMPARISON_FUNC)pEntry->frontStencilFunc;
    desc.BackFace.StencilFailOp       = (D3D11_STENCIL_OP)pEntry->backStencilFailOp;
    desc.BackFace.StencilDepthFailOp  = (D3D11_STENCIL_OP)pEntry->backStencilDepthFailOp;
    desc.BackFace.StencilPassOp       = (D3D11_STENCIL_OP)pEntry->backStencilPassOp;
    desc.BackFace.StencilFunc         = (D3D11_COMPARISON_FUNC)pEntry->backStencilFunc;
    /** @todo frontEnable, backEnable */

    HRESULT hr = pDevice->pDevice->CreateDepthStencilState(&desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxSamplerStateCreate(DXDEVICE *pDevice, SVGACOTableDXSamplerEntry const *pEntry, ID3D11SamplerState **pp)
{
    D3D11_SAMPLER_DESC desc;
    /* Guest sometimes sends inconsistent (from D3D11 point of view) set of filter flags. */
    if (pEntry->filter & SVGA3D_FILTER_ANISOTROPIC)
        desc.Filter     = (pEntry->filter & SVGA3D_FILTER_COMPARE)
                        ? D3D11_FILTER_COMPARISON_ANISOTROPIC
                        : D3D11_FILTER_ANISOTROPIC;
    else
        desc.Filter     = (D3D11_FILTER)pEntry->filter;
    desc.AddressU       = (D3D11_TEXTURE_ADDRESS_MODE)pEntry->addressU;
    desc.AddressV       = (D3D11_TEXTURE_ADDRESS_MODE)pEntry->addressV;
    desc.AddressW       = (D3D11_TEXTURE_ADDRESS_MODE)pEntry->addressW;
    desc.MipLODBias     = pEntry->mipLODBias;
    desc.MaxAnisotropy  = RT_CLAMP(pEntry->maxAnisotropy, 1, 16); /* "Valid values are between 1 and 16" */
    desc.ComparisonFunc = (D3D11_COMPARISON_FUNC)pEntry->comparisonFunc;
    desc.BorderColor[0] = pEntry->borderColor.value[0];
    desc.BorderColor[1] = pEntry->borderColor.value[1];
    desc.BorderColor[2] = pEntry->borderColor.value[2];
    desc.BorderColor[3] = pEntry->borderColor.value[3];
    desc.MinLOD         = pEntry->minLOD;
    desc.MaxLOD         = pEntry->maxLOD;

    HRESULT hr = pDevice->pDevice->CreateSamplerState(&desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static D3D11_FILL_MODE dxFillMode(uint8_t svgaFillMode)
{
    if (svgaFillMode == SVGA3D_FILLMODE_POINT)
        return D3D11_FILL_WIREFRAME;
    return (D3D11_FILL_MODE)svgaFillMode;
}


static HRESULT dxRasterizerStateCreate(DXDEVICE *pDevice, SVGACOTableDXRasterizerStateEntry const *pEntry, ID3D11RasterizerState **pp)
{
    D3D11_RASTERIZER_DESC desc;
    desc.FillMode              = dxFillMode(pEntry->fillMode);
    desc.CullMode              = (D3D11_CULL_MODE)pEntry->cullMode;
    desc.FrontCounterClockwise = pEntry->frontCounterClockwise;
    /** @todo provokingVertexLast */
    desc.DepthBias             = pEntry->depthBias;
    desc.DepthBiasClamp        = pEntry->depthBiasClamp;
    desc.SlopeScaledDepthBias  = pEntry->slopeScaledDepthBias;
    desc.DepthClipEnable       = pEntry->depthClipEnable;
    desc.ScissorEnable         = pEntry->scissorEnable;
    desc.MultisampleEnable     = pEntry->multisampleEnable;
    desc.AntialiasedLineEnable = pEntry->antialiasedLineEnable;
    /** @todo lineWidth lineStippleEnable lineStippleFactor lineStipplePattern forcedSampleCount */

    HRESULT hr = pDevice->pDevice->CreateRasterizerState(&desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxRenderTargetViewCreate(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGACOTableDXRTViewEntry const *pEntry, VMSVGA3DSURFACE *pSurface, ID3D11RenderTargetView **pp)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);

    ID3D11Resource *pResource = dxResource(pThisCC->svga.p3dState, pSurface, pDXContext);

    D3D11_RENDER_TARGET_VIEW_DESC desc;
    RT_ZERO(desc);
    desc.Format = vmsvgaDXSurfaceFormat2Dxgi(pEntry->format);
    AssertReturn(desc.Format != DXGI_FORMAT_UNKNOWN, E_FAIL);
    switch (pEntry->resourceDimension)
    {
        case SVGA3D_RESOURCE_BUFFER:
            desc.ViewDimension = D3D11_RTV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = pEntry->desc.buffer.firstElement;
            desc.Buffer.NumElements = pEntry->desc.buffer.numElements;
            break;
        case SVGA3D_RESOURCE_TEXTURE1D:
            if (pEntry->desc.tex.arraySize <= 1)
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
                desc.Texture1D.MipSlice = pEntry->desc.tex.mipSlice;
            }
            else
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
                desc.Texture1DArray.MipSlice = pEntry->desc.tex.mipSlice;
                desc.Texture1DArray.FirstArraySlice = pEntry->desc.tex.firstArraySlice;
                desc.Texture1DArray.ArraySize = pEntry->desc.tex.arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE2D:
            if (pEntry->desc.tex.arraySize <= 1)
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = pEntry->desc.tex.mipSlice;
            }
            else
            {
                desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = pEntry->desc.tex.mipSlice;
                desc.Texture2DArray.FirstArraySlice = pEntry->desc.tex.firstArraySlice;
                desc.Texture2DArray.ArraySize = pEntry->desc.tex.arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE3D:
            desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
            desc.Texture3D.MipSlice = pEntry->desc.tex3D.mipSlice;
            desc.Texture3D.FirstWSlice = pEntry->desc.tex3D.firstW;
            desc.Texture3D.WSize = pEntry->desc.tex3D.wSize;
            break;
        case SVGA3D_RESOURCE_TEXTURECUBE:
            AssertFailed(); /** @todo test. Probably not applicable to a render target view. */
            desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            desc.Texture2DArray.MipSlice = pEntry->desc.tex.mipSlice;
            desc.Texture2DArray.FirstArraySlice = 0;
            desc.Texture2DArray.ArraySize = 6;
            break;
        case SVGA3D_RESOURCE_BUFFEREX:
            AssertFailed(); /** @todo test. Probably not applicable to a render target view. */
            desc.ViewDimension = D3D11_RTV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = pEntry->desc.buffer.firstElement;
            desc.Buffer.NumElements = pEntry->desc.buffer.numElements;
            break;
        default:
            ASSERT_GUEST_FAILED_RETURN(E_INVALIDARG);
    }

    HRESULT hr = pDevice->pDevice->CreateRenderTargetView(pResource, &desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxShaderResourceViewCreate(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGACOTableDXSRViewEntry const *pEntry, VMSVGA3DSURFACE *pSurface, ID3D11ShaderResourceView **pp)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);

    ID3D11Resource *pResource = dxResource(pThisCC->svga.p3dState, pSurface, pDXContext);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    RT_ZERO(desc);
    desc.Format = vmsvgaDXSurfaceFormat2Dxgi(pEntry->format);
    AssertReturn(desc.Format != DXGI_FORMAT_UNKNOWN, E_FAIL);

    switch (pEntry->resourceDimension)
    {
        case SVGA3D_RESOURCE_BUFFER:
            desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = pEntry->desc.buffer.firstElement;
            desc.Buffer.NumElements = pEntry->desc.buffer.numElements;
            break;
        case SVGA3D_RESOURCE_TEXTURE1D:
            if (pEntry->desc.tex.arraySize <= 1)
            {
                desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                desc.Texture1D.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
                desc.Texture1D.MipLevels = pEntry->desc.tex.mipLevels;
            }
            else
            {
                desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                desc.Texture1DArray.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
                desc.Texture1DArray.MipLevels = pEntry->desc.tex.mipLevels;
                desc.Texture1DArray.FirstArraySlice = pEntry->desc.tex.firstArraySlice;
                desc.Texture1DArray.ArraySize = pEntry->desc.tex.arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE2D:
            if (pEntry->desc.tex.arraySize <= 1)
            {
                desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
                desc.Texture2D.MipLevels = pEntry->desc.tex.mipLevels;
            }
            else
            {
                desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
                desc.Texture2DArray.MipLevels = pEntry->desc.tex.mipLevels;
                desc.Texture2DArray.FirstArraySlice = pEntry->desc.tex.firstArraySlice;
                desc.Texture2DArray.ArraySize = pEntry->desc.tex.arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE3D:
            desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            desc.Texture3D.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
            desc.Texture3D.MipLevels = pEntry->desc.tex.mipLevels;
            break;
        case SVGA3D_RESOURCE_TEXTURECUBE:
            desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            desc.TextureCube.MostDetailedMip = pEntry->desc.tex.mostDetailedMip;
            desc.TextureCube.MipLevels = pEntry->desc.tex.mipLevels;
            break;
        case SVGA3D_RESOURCE_BUFFEREX:
            AssertFailed(); /** @todo test. */
            desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
            desc.BufferEx.FirstElement = pEntry->desc.bufferex.firstElement;
            desc.BufferEx.NumElements = pEntry->desc.bufferex.numElements;
            desc.BufferEx.Flags = pEntry->desc.bufferex.flags;
            break;
        default:
            ASSERT_GUEST_FAILED_RETURN(E_INVALIDARG);
    }

    HRESULT hr = pDevice->pDevice->CreateShaderResourceView(pResource, &desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxDepthStencilViewCreate(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGACOTableDXDSViewEntry const *pEntry, VMSVGA3DSURFACE *pSurface, ID3D11DepthStencilView **pp)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);

    ID3D11Resource *pResource = dxResource(pThisCC->svga.p3dState, pSurface, pDXContext);

    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    RT_ZERO(desc);
    desc.Format = vmsvgaDXSurfaceFormat2Dxgi(pEntry->format);
    AssertReturn(desc.Format != DXGI_FORMAT_UNKNOWN, E_FAIL);
    desc.Flags = pEntry->flags;
    switch (pEntry->resourceDimension)
    {
        case SVGA3D_RESOURCE_TEXTURE1D:
            if (pEntry->arraySize <= 1)
            {
                desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
                desc.Texture1D.MipSlice = pEntry->mipSlice;
            }
            else
            {
                desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
                desc.Texture1DArray.MipSlice = pEntry->mipSlice;
                desc.Texture1DArray.FirstArraySlice = pEntry->firstArraySlice;
                desc.Texture1DArray.ArraySize = pEntry->arraySize;
            }
            break;
        case SVGA3D_RESOURCE_TEXTURE2D:
            if (pEntry->arraySize <= 1)
            {
                desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = pEntry->mipSlice;
            }
            else
            {
                desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = pEntry->mipSlice;
                desc.Texture2DArray.FirstArraySlice = pEntry->firstArraySlice;
                desc.Texture2DArray.ArraySize = pEntry->arraySize;
            }
            break;
        default:
            ASSERT_GUEST_FAILED_RETURN(E_INVALIDARG);
    }

    HRESULT hr = pDevice->pDevice->CreateDepthStencilView(pResource, &desc, pp);
    Assert(SUCCEEDED(hr));
    return hr;
}


static HRESULT dxShaderCreate(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, DXSHADER *pDXShader)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);

    HRESULT hr = S_OK;

    switch (pDXShader->enmShaderType)
    {
        case SVGA3D_SHADERTYPE_VS:
            hr = pDevice->pDevice->CreateVertexShader(pDXShader->pvDXBC, pDXShader->cbDXBC, NULL, &pDXShader->pVertexShader);
            Assert(SUCCEEDED(hr));
            break;
        case SVGA3D_SHADERTYPE_PS:
            hr = pDevice->pDevice->CreatePixelShader(pDXShader->pvDXBC, pDXShader->cbDXBC, NULL, &pDXShader->pPixelShader);
            Assert(SUCCEEDED(hr));
            break;
        case SVGA3D_SHADERTYPE_GS:
        {
            SVGA3dStreamOutputId const soid = pDXContext->svgaDXContext.streamOut.soid;
            if (soid == SVGA_ID_INVALID)
                hr = pDevice->pDevice->CreateGeometryShader(pDXShader->pvDXBC, pDXShader->cbDXBC, NULL, &pDXShader->pGeometryShader);
            else
            {
                ASSERT_GUEST_RETURN(soid < pDXContext->pBackendDXContext->cStreamOutput, E_INVALIDARG);

                SVGACOTableDXStreamOutputEntry const *pEntry = &pDXContext->cot.paStreamOutput[soid];
                DXSTREAMOUTPUT *pDXStreamOutput = &pDXContext->pBackendDXContext->paStreamOutput[soid];
                uint32_t const cSOTarget = pDXContext->pBackendDXContext->cSOTarget;

                for (uint32_t i = 0; i < pDXStreamOutput->cDeclarationEntry; ++i)
                {
                    D3D11_SO_DECLARATION_ENTRY *p = &pDXStreamOutput->aDeclarationEntry[i];
                    SVGA3dStreamOutputDeclarationEntry const *decl = &pEntry->decl[i];
                    p->SemanticName = DXShaderGetOutputSemanticName(&pDXShader->shaderInfo, decl->registerIndex);
                }

                hr = pDevice->pDevice->CreateGeometryShaderWithStreamOutput(pDXShader->pvDXBC, pDXShader->cbDXBC,
                    pDXStreamOutput->aDeclarationEntry, pDXStreamOutput->cDeclarationEntry,
                    pEntry->streamOutputStrideInBytes, cSOTarget, pEntry->rasterizedStream,
                    /*pClassLinkage=*/ NULL, &pDXShader->pGeometryShader);
                if (SUCCEEDED(hr))
                    pDXShader->soid = soid;
            }
            Assert(SUCCEEDED(hr));
            break;
        }
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN(E_INVALIDARG);
    }

    return hr;
}


static void dxShaderSet(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderType type, DXSHADER *pDXShader)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);

    switch (type)
    {
        case SVGA3D_SHADERTYPE_VS:
            pDevice->pImmediateContext->VSSetShader(pDXShader ? pDXShader->pVertexShader : NULL, NULL, 0);
            break;
        case SVGA3D_SHADERTYPE_PS:
            pDevice->pImmediateContext->PSSetShader(pDXShader ? pDXShader->pPixelShader : NULL, NULL, 0);
            break;
        case SVGA3D_SHADERTYPE_GS:
        {
            Assert(!pDXShader || (pDXShader->soid == pDXContext->svgaDXContext.streamOut.soid));
            pDevice->pImmediateContext->GSSetShader(pDXShader ? pDXShader->pGeometryShader : NULL, NULL, 0);
        } break;
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN_VOID();
    }
}


static void dxConstantBufferSet(DXDEVICE *pDevice, uint32_t slot, SVGA3dShaderType type, ID3D11Buffer *pConstantBuffer)
{
    switch (type)
    {
        case SVGA3D_SHADERTYPE_VS:
            pDevice->pImmediateContext->VSSetConstantBuffers(slot, 1, &pConstantBuffer);
            break;
        case SVGA3D_SHADERTYPE_PS:
            pDevice->pImmediateContext->PSSetConstantBuffers(slot, 1, &pConstantBuffer);
            break;
        case SVGA3D_SHADERTYPE_GS:
            pDevice->pImmediateContext->GSSetConstantBuffers(slot, 1, &pConstantBuffer);
            break;
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN_VOID();
    }
}


static void dxSamplerSet(DXDEVICE *pDevice, SVGA3dShaderType type, uint32_t startSampler, uint32_t cSampler, ID3D11SamplerState * const *papSampler)
{
    switch (type)
    {
        case SVGA3D_SHADERTYPE_VS:
            pDevice->pImmediateContext->VSSetSamplers(startSampler, cSampler, papSampler);
            break;
        case SVGA3D_SHADERTYPE_PS:
            pDevice->pImmediateContext->PSSetSamplers(startSampler, cSampler, papSampler);
            break;
        case SVGA3D_SHADERTYPE_GS:
            pDevice->pImmediateContext->GSSetSamplers(startSampler, cSampler, papSampler);
            break;
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN_VOID();
    }
}


static void dxShaderResourceViewSet(DXDEVICE *pDevice, SVGA3dShaderType type, uint32_t startView, uint32_t cShaderResourceView, ID3D11ShaderResourceView * const *papShaderResourceView)
{
    switch (type)
    {
        case SVGA3D_SHADERTYPE_VS:
            pDevice->pImmediateContext->VSSetShaderResources(startView, cShaderResourceView, papShaderResourceView);
            break;
        case SVGA3D_SHADERTYPE_PS:
            pDevice->pImmediateContext->PSSetShaderResources(startView, cShaderResourceView, papShaderResourceView);
            break;
        case SVGA3D_SHADERTYPE_GS:
            pDevice->pImmediateContext->GSSetShaderResources(startView, cShaderResourceView, papShaderResourceView);
            break;
        case SVGA3D_SHADERTYPE_HS:
        case SVGA3D_SHADERTYPE_DS:
        case SVGA3D_SHADERTYPE_CS:
        default:
            ASSERT_GUEST_FAILED_RETURN_VOID();
    }
}


static int dxBackendSurfaceAlloc(PVMSVGA3DBACKENDSURFACE *ppBackendSurface)
{
    PVMSVGA3DBACKENDSURFACE pBackendSurface = (PVMSVGA3DBACKENDSURFACE)RTMemAllocZ(sizeof(VMSVGA3DBACKENDSURFACE));
    AssertPtrReturn(pBackendSurface, VERR_NO_MEMORY);
    pBackendSurface->cidDrawing = SVGA_ID_INVALID;
    RTListInit(&pBackendSurface->u2.Texture.listView);
    *ppBackendSurface = pBackendSurface;
    return VINF_SUCCESS;
}


static int vmsvga3dBackSurfaceCreateScreenTarget(PVGASTATECC pThisCC, PVMSVGA3DSURFACE pSurface)
{
    PVMSVGA3DSTATE p3dState = pThisCC->svga.p3dState;
    AssertReturn(p3dState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    DXDEVICE *pDXDevice = &pBackend->dxDevice;
    AssertReturn(pDXDevice->pDevice, VERR_INVALID_STATE);

    /* Surface must have SCREEN_TARGET flag. */
    ASSERT_GUEST_RETURN(RT_BOOL(pSurface->surfaceFlags & SVGA3D_SURFACE_SCREENTARGET), VERR_INVALID_PARAMETER);

    if (VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface))
    {
        AssertFailed(); /* Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface;
    int rc = dxBackendSurfaceAlloc(&pBackendSurface);
    AssertRCReturn(rc, rc);

    D3D11_TEXTURE2D_DESC td;
    RT_ZERO(td);
    td.Width              = pSurface->paMipmapLevels[0].mipmapSize.width;
    td.Height             = pSurface->paMipmapLevels[0].mipmapSize.height;
    Assert(pSurface->cLevels == 1);
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = vmsvgaDXSurfaceFormat2Dxgi(pSurface->format);
    td.SampleDesc.Count   = 1;
    td.SampleDesc.Quality = 0;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags     = 0;
    td.MiscFlags          = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = pDXDevice->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->u.pTexture2D);
    Assert(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
    {
        /* Map-able texture. */
        td.Usage          = D3D11_USAGE_DYNAMIC;
        td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        td.MiscFlags      = 0;
        hr = pDXDevice->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->pDynamicTexture);
        Assert(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr))
    {
        /* Staging texture. */
        td.Usage          = D3D11_USAGE_STAGING;
        td.BindFlags      = 0; /* No flags allowed. */
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        hr = pDXDevice->pDevice->CreateTexture2D(&td, 0, &pBackendSurface->pStagingTexture);
        Assert(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr))
    {
        /* Get the shared handle. */
        IDXGIResource *pDxgiResource = NULL;
        hr = pBackendSurface->u.pTexture2D->QueryInterface(__uuidof(IDXGIResource), (void**)&pDxgiResource);
        Assert(SUCCEEDED(hr));
        if (SUCCEEDED(hr))
        {
            hr = pDxgiResource->GetSharedHandle(&pBackendSurface->SharedHandle);
            Assert(SUCCEEDED(hr));
            D3D_RELEASE(pDxgiResource);
        }
    }

    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_SCREEN_TARGET;
        pBackendSurface->enmDxgiFormat = td.Format;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = DX_CID_BACKEND;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->pStagingTexture);
    D3D_RELEASE(pBackendSurface->pDynamicTexture);
    D3D_RELEASE(pBackendSurface->u.pTexture2D);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static UINT dxBindFlags(SVGA3dSurfaceAllFlags surfaceFlags)
{
    /* Catch unimplemented flags. */
    Assert(!RT_BOOL(surfaceFlags & (SVGA3D_SURFACE_BIND_LOGICOPS | SVGA3D_SURFACE_BIND_RAW_VIEWS)));

    UINT BindFlags = 0;

    if (surfaceFlags & SVGA3D_SURFACE_BIND_VERTEX_BUFFER)   BindFlags |= D3D11_BIND_VERTEX_BUFFER;
    if (surfaceFlags & SVGA3D_SURFACE_BIND_INDEX_BUFFER)    BindFlags |= D3D11_BIND_INDEX_BUFFER;
    if (surfaceFlags & SVGA3D_SURFACE_BIND_CONSTANT_BUFFER) BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
    if (surfaceFlags & SVGA3D_SURFACE_BIND_SHADER_RESOURCE) BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    if (surfaceFlags & SVGA3D_SURFACE_BIND_RENDER_TARGET)   BindFlags |= D3D11_BIND_RENDER_TARGET;
    if (surfaceFlags & SVGA3D_SURFACE_BIND_DEPTH_STENCIL)   BindFlags |= D3D11_BIND_DEPTH_STENCIL;
    if (surfaceFlags & SVGA3D_SURFACE_BIND_STREAM_OUTPUT)   BindFlags |= D3D11_BIND_STREAM_OUTPUT;
    if (surfaceFlags & SVGA3D_SURFACE_BIND_UAVIEW)          BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

    return BindFlags;
}


static DXDEVICE *dxSurfaceDevice(PVMSVGA3DSTATE p3dState, PVMSVGA3DSURFACE pSurface, PVMSVGA3DDXCONTEXT pDXContext, UINT *pMiscFlags)
{
    if (p3dState->pBackend->fSingleDevice)
    {
        *pMiscFlags = 0;
        return &p3dState->pBackend->dxDevice;
    }

    if (dxIsSurfaceShareable(pSurface))
    {
        *pMiscFlags = D3D11_RESOURCE_MISC_SHARED;
        return &p3dState->pBackend->dxDevice;
    }

    *pMiscFlags = 0;
    return &pDXContext->pBackendDXContext->dxDevice;
}

static int vmsvga3dBackSurfaceCreateTexture(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    PVMSVGA3DSTATE p3dState = pThisCC->svga.p3dState;
    AssertReturn(p3dState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = p3dState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    UINT MiscFlags;
    DXDEVICE *pDXDevice = dxSurfaceDevice(p3dState, pSurface, pDXContext, &MiscFlags);
    AssertReturn(pDXDevice->pDevice, VERR_INVALID_STATE);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface;
    int rc = dxBackendSurfaceAlloc(&pBackendSurface);
    AssertRCReturn(rc, rc);

    uint32_t const cWidth = pSurface->paMipmapLevels[0].mipmapSize.width;
    uint32_t const cHeight = pSurface->paMipmapLevels[0].mipmapSize.height;
    uint32_t const cDepth = pSurface->paMipmapLevels[0].mipmapSize.depth;
    uint32_t const numMipLevels = pSurface->cLevels;

    DXGI_FORMAT dxgiFormat = vmsvgaDXSurfaceFormat2Dxgi(pSurface->format);
    AssertReturn(dxgiFormat != DXGI_FORMAT_UNKNOWN, E_FAIL);

    /*
     * Create D3D11 texture object.
     */
    HRESULT hr = S_OK;
    if (pSurface->surfaceFlags & SVGA3D_SURFACE_SCREENTARGET)
    {
        /*
         * Create the texture in backend device and open for the specified context.
         */
        D3D11_SUBRESOURCE_DATA *paInitialData = NULL;
        D3D11_SUBRESOURCE_DATA aInitialData[SVGA3D_MAX_MIP_LEVELS];
        if (pSurface->paMipmapLevels[0].pSurfaceData)
        {
            /** @todo Can happen for a non GBO surface or if GBO texture was updated prior to creation if the hardware resource. Test this. */
            for (uint32_t i = 0; i < numMipLevels; ++i)
            {
                PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[i];
                D3D11_SUBRESOURCE_DATA *p = &aInitialData[i];
                p->pSysMem          = pMipmapLevel->pSurfaceData;
                p->SysMemPitch      = pMipmapLevel->cbSurfacePitch;
                p->SysMemSlicePitch = pMipmapLevel->cbSurfacePlane;
            }
            paInitialData = &aInitialData[0];
        }

        D3D11_TEXTURE2D_DESC td;
        RT_ZERO(td);
        td.Width              = pSurface->paMipmapLevels[0].mipmapSize.width;
        td.Height             = pSurface->paMipmapLevels[0].mipmapSize.height;
        Assert(pSurface->cLevels == 1);
        td.MipLevels          = 1;
        td.ArraySize          = 1;
        td.Format             = dxgiFormat;
        td.SampleDesc.Count   = 1;
        td.SampleDesc.Quality = 0;
        td.Usage              = D3D11_USAGE_DEFAULT;
        td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags     = 0;
        td.MiscFlags          = MiscFlags;

        hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->u.pTexture2D);
        Assert(SUCCEEDED(hr));
        if (SUCCEEDED(hr))
        {
            /* Map-able texture. */
            td.Usage          = D3D11_USAGE_DYNAMIC;
            td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
            td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            td.MiscFlags      = 0;
            hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pDynamicTexture);
            Assert(SUCCEEDED(hr));
        }

        if (SUCCEEDED(hr))
        {
            /* Staging texture. */
            td.Usage          = D3D11_USAGE_STAGING;
            td.BindFlags      = 0; /* No flags allowed. */
            td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pStagingTexture);
            Assert(SUCCEEDED(hr));
        }

        if (SUCCEEDED(hr))
        {
            /* Get the shared handle. */
            IDXGIResource *pDxgiResource = NULL;
            hr = pBackendSurface->u.pTexture2D->QueryInterface(__uuidof(IDXGIResource), (void**)&pDxgiResource);
            Assert(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
            {
                hr = pDxgiResource->GetSharedHandle(&pBackendSurface->SharedHandle);
                Assert(SUCCEEDED(hr));
                D3D_RELEASE(pDxgiResource);
            }
        }

        if (SUCCEEDED(hr))
        {
            pBackendSurface->enmResType = VMSVGA3D_RESTYPE_SCREEN_TARGET;
        }
    }
    else if (pSurface->surfaceFlags & SVGA3D_SURFACE_CUBEMAP)
    {
        Assert(pSurface->cFaces == 6);
        Assert(cWidth == cHeight);
        Assert(cDepth == 1);
//DEBUG_BREAKPOINT_TEST();
        D3D11_SUBRESOURCE_DATA *paInitialData = NULL;
        D3D11_SUBRESOURCE_DATA aInitialData[6 * SVGA3D_MAX_MIP_LEVELS];
        if (pSurface->paMipmapLevels[0].pSurfaceData)
        {
            /** @todo Can happen for a non GBO surface or if GBO texture was updated prior to creation if the hardware resource. Test this. */
            /** @todo for (i = 0; i < pSurface->cFaces * numMipLevels; ++i) */
            for (uint32_t iFace = 0; iFace < 6; ++iFace)
            {
                for (uint32_t i = 0; i < numMipLevels; ++i)
                {
                    uint32_t const iSubresource = vmsvga3dCalcSubresource(i, iFace, numMipLevels);

                    PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[iSubresource];
                    D3D11_SUBRESOURCE_DATA *p = &aInitialData[iSubresource];
                    p->pSysMem          = pMipmapLevel->pSurfaceData;
                    p->SysMemPitch      = pMipmapLevel->cbSurfacePitch;
                    p->SysMemSlicePitch = pMipmapLevel->cbSurfacePlane;
                }
            }
            paInitialData = &aInitialData[0];
        }

        D3D11_TEXTURE2D_DESC td;
        RT_ZERO(td);
        td.Width              = cWidth;
        td.Height             = cHeight;
        td.MipLevels          = numMipLevels;
        td.ArraySize          = 6;
        td.Format             = dxgiFormat;
        td.SampleDesc.Count   = 1;
        td.SampleDesc.Quality = 0;
        td.Usage              = D3D11_USAGE_DEFAULT;
        td.BindFlags          = dxBindFlags(pSurface->surfaceFlags);
        td.CPUAccessFlags     = 0; /** @todo */
        td.MiscFlags          = MiscFlags | D3D11_RESOURCE_MISC_TEXTURECUBE; /** @todo */
        if (   numMipLevels > 1
            && (td.BindFlags & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)) == (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET))
            td.MiscFlags     |= D3D11_RESOURCE_MISC_GENERATE_MIPS; /* Required for GenMips. */

        hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->u.pTexture2D);
        Assert(SUCCEEDED(hr));
        if (SUCCEEDED(hr))
        {
            /* Map-able texture. */
            td.MipLevels      = 1; /* Must be for D3D11_USAGE_DYNAMIC. */
            td.ArraySize      = 1; /* Must be for D3D11_USAGE_DYNAMIC. */
            td.Usage          = D3D11_USAGE_DYNAMIC;
            td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
            td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            td.MiscFlags      = 0;
            hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pDynamicTexture);
            Assert(SUCCEEDED(hr));
        }

        if (SUCCEEDED(hr))
        {
            /* Staging texture. */
            td.Usage          = D3D11_USAGE_STAGING;
            td.BindFlags      = 0; /* No flags allowed. */
            td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            td.MiscFlags      = 0;
            hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pStagingTexture);
            Assert(SUCCEEDED(hr));
        }

        if (   SUCCEEDED(hr)
            && MiscFlags == D3D11_RESOURCE_MISC_SHARED)
        {
            /* Get the shared handle. */
            IDXGIResource *pDxgiResource = NULL;
            hr = pBackendSurface->u.pTexture2D->QueryInterface(__uuidof(IDXGIResource), (void**)&pDxgiResource);
            Assert(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
            {
                hr = pDxgiResource->GetSharedHandle(&pBackendSurface->SharedHandle);
                Assert(SUCCEEDED(hr));
                D3D_RELEASE(pDxgiResource);
            }
        }

        if (SUCCEEDED(hr))
        {
            pBackendSurface->enmResType = VMSVGA3D_RESTYPE_TEXTURE_CUBE;
        }
    }
    else if (pSurface->surfaceFlags & SVGA3D_SURFACE_1D)
    {
        AssertFailed(); /** @todo implement */
        hr = E_FAIL;
    }
    else
    {
        if (cDepth > 1)
        {
            /*
             * Volume texture.
             */
            Assert(pSurface->cFaces == 1);

            D3D11_SUBRESOURCE_DATA *paInitialData = NULL;
            D3D11_SUBRESOURCE_DATA aInitialData[SVGA3D_MAX_MIP_LEVELS];
            if (pSurface->paMipmapLevels[0].pSurfaceData)
            {
                /** @todo Can happen for a non GBO surface or if GBO texture was updated prior to creation if the hardware resource. Test this. */
                for (uint32_t i = 0; i < numMipLevels; ++i)
                {
                    PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[i];
                    D3D11_SUBRESOURCE_DATA *p = &aInitialData[i];
                    p->pSysMem          = pMipmapLevel->pSurfaceData;
                    p->SysMemPitch      = pMipmapLevel->cbSurfacePitch;
                    p->SysMemSlicePitch = pMipmapLevel->cbSurfacePlane;
                }
                paInitialData = &aInitialData[0];
            }

            D3D11_TEXTURE3D_DESC td;
            RT_ZERO(td);
            td.Width              = cWidth;
            td.Height             = cHeight;
            td.Depth              = cDepth;
            td.MipLevels          = numMipLevels;
            td.Format             = dxgiFormat;
            td.Usage              = D3D11_USAGE_DEFAULT;
            td.BindFlags          = dxBindFlags(pSurface->surfaceFlags);
            td.CPUAccessFlags     = 0; /** @todo */
            td.MiscFlags          = MiscFlags; /** @todo */
            if (   numMipLevels > 1
                && (td.BindFlags & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)) == (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET))
                td.MiscFlags     |= D3D11_RESOURCE_MISC_GENERATE_MIPS; /* Required for GenMips. */

            hr = pDXDevice->pDevice->CreateTexture3D(&td, paInitialData, &pBackendSurface->u.pTexture3D);
            Assert(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
            {
                /* Map-able texture. */
                td.MipLevels      = 1; /* Must be for D3D11_USAGE_DYNAMIC. */
                td.Usage          = D3D11_USAGE_DYNAMIC;
                td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
                td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                td.MiscFlags      = 0;
                hr = pDXDevice->pDevice->CreateTexture3D(&td, paInitialData, &pBackendSurface->pDynamicTexture3D);
                Assert(SUCCEEDED(hr));
            }

            if (SUCCEEDED(hr))
            {
                /* Staging texture. */
                td.Usage          = D3D11_USAGE_STAGING;
                td.BindFlags      = 0; /* No flags allowed. */
                td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
                td.MiscFlags      = 0;
                hr = pDXDevice->pDevice->CreateTexture3D(&td, paInitialData, &pBackendSurface->pStagingTexture3D);
                Assert(SUCCEEDED(hr));
            }

            if (   SUCCEEDED(hr)
                && MiscFlags == D3D11_RESOURCE_MISC_SHARED)
            {
                /* Get the shared handle. */
                IDXGIResource *pDxgiResource = NULL;
                hr = pBackendSurface->u.pTexture3D->QueryInterface(__uuidof(IDXGIResource), (void**)&pDxgiResource);
                Assert(SUCCEEDED(hr));
                if (SUCCEEDED(hr))
                {
                    hr = pDxgiResource->GetSharedHandle(&pBackendSurface->SharedHandle);
                    Assert(SUCCEEDED(hr));
                    D3D_RELEASE(pDxgiResource);
                }
            }

            if (SUCCEEDED(hr))
            {
                pBackendSurface->enmResType = VMSVGA3D_RESTYPE_TEXTURE_3D;
            }
        }
        else
        {
            /*
             * 2D texture.
             */
            Assert(pSurface->cFaces == 1);

            D3D11_SUBRESOURCE_DATA *paInitialData = NULL;
            D3D11_SUBRESOURCE_DATA aInitialData[SVGA3D_MAX_MIP_LEVELS];
            if (pSurface->paMipmapLevels[0].pSurfaceData)
            {
                /** @todo Can happen for a non GBO surface or if GBO texture was updated prior to creation if the hardware resource. Test this. */
                for (uint32_t i = 0; i < numMipLevels; ++i)
                {
                    PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[i];
                    D3D11_SUBRESOURCE_DATA *p = &aInitialData[i];
                    p->pSysMem          = pMipmapLevel->pSurfaceData;
                    p->SysMemPitch      = pMipmapLevel->cbSurfacePitch;
                    p->SysMemSlicePitch = pMipmapLevel->cbSurfacePlane;
                }
                paInitialData = &aInitialData[0];
            }

            D3D11_TEXTURE2D_DESC td;
            RT_ZERO(td);
            td.Width              = cWidth;
            td.Height             = cHeight;
            td.MipLevels          = numMipLevels;
            td.ArraySize          = 1; /** @todo */
            td.Format             = dxgiFormat;
            td.SampleDesc.Count   = 1;
            td.SampleDesc.Quality = 0;
            td.Usage              = D3D11_USAGE_DEFAULT;
            td.BindFlags          = dxBindFlags(pSurface->surfaceFlags);
            td.CPUAccessFlags     = 0; /** @todo */
            td.MiscFlags          = MiscFlags; /** @todo */
            if (   numMipLevels > 1
                && (td.BindFlags & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET)) == (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET))
                td.MiscFlags     |= D3D11_RESOURCE_MISC_GENERATE_MIPS; /* Required for GenMips. */

            hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->u.pTexture2D);
            Assert(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
            {
                /* Map-able texture. */
                td.MipLevels      = 1; /* Must be for D3D11_USAGE_DYNAMIC. */
                td.Usage          = D3D11_USAGE_DYNAMIC;
                td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
                td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                td.MiscFlags      = 0;
                hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pDynamicTexture);
                Assert(SUCCEEDED(hr));
            }

            if (SUCCEEDED(hr))
            {
                /* Staging texture. */
                td.Usage          = D3D11_USAGE_STAGING;
                td.BindFlags      = 0; /* No flags allowed. */
                td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
                td.MiscFlags      = 0;
                hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pStagingTexture);
                Assert(SUCCEEDED(hr));
            }

            if (   SUCCEEDED(hr)
                && MiscFlags == D3D11_RESOURCE_MISC_SHARED)
            {
                /* Get the shared handle. */
                IDXGIResource *pDxgiResource = NULL;
                hr = pBackendSurface->u.pTexture2D->QueryInterface(__uuidof(IDXGIResource), (void**)&pDxgiResource);
                Assert(SUCCEEDED(hr));
                if (SUCCEEDED(hr))
                {
                    hr = pDxgiResource->GetSharedHandle(&pBackendSurface->SharedHandle);
                    Assert(SUCCEEDED(hr));
                    D3D_RELEASE(pDxgiResource);
                }
            }

            if (SUCCEEDED(hr))
            {
                pBackendSurface->enmResType = VMSVGA3D_RESTYPE_TEXTURE_2D;
            }
        }
    }

    Assert(hr == S_OK);

    if (pSurface->autogenFilter != SVGA3D_TEX_FILTER_NONE)
    {
    }

    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        LogFunc(("sid = %u\n", pSurface->id));
        pBackendSurface->enmDxgiFormat = dxgiFormat;
        pSurface->pBackendSurface = pBackendSurface;
        if (p3dState->pBackend->fSingleDevice || RT_BOOL(MiscFlags & D3D11_RESOURCE_MISC_SHARED))
            pSurface->idAssociatedContext = DX_CID_BACKEND;
        else
            pSurface->idAssociatedContext = pDXContext->cid;
        return VINF_SUCCESS;
    }

    /** @todo different enmResType Failure. */
    D3D_RELEASE(pBackendSurface->pStagingTexture);
    D3D_RELEASE(pBackendSurface->pDynamicTexture);
    D3D_RELEASE(pBackendSurface->u.pTexture2D);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


/** @todo This is practically the same code as vmsvga3dBackSurfaceCreateTexture */
static int vmsvga3dBackSurfaceCreateDepthStencilTexture(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDXDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDXDevice->pDevice, VERR_INVALID_STATE);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface;
    int rc = dxBackendSurfaceAlloc(&pBackendSurface);
    AssertRCReturn(rc, rc);

    uint32_t const cWidth = pSurface->paMipmapLevels[0].mipmapSize.width;
    uint32_t const cHeight = pSurface->paMipmapLevels[0].mipmapSize.height;
    uint32_t const cDepth = pSurface->paMipmapLevels[0].mipmapSize.depth;
    uint32_t const numMipLevels = pSurface->cLevels;

    DXGI_FORMAT dxgiFormat = vmsvgaDXSurfaceFormat2Dxgi(pSurface->format);
    AssertReturn(dxgiFormat != DXGI_FORMAT_UNKNOWN, E_FAIL);

    /*
     * Create D3D11 texture object.
     */
    HRESULT hr = S_OK;
    if (pSurface->surfaceFlags & SVGA3D_SURFACE_CUBEMAP)
    {
        /*
         * CubeMap texture.
         */
        Assert(pSurface->cFaces == 6);
        Assert(cWidth == cHeight);
        Assert(cDepth == 1);
        Assert(numMipLevels == 1);
//DEBUG_BREAKPOINT_TEST();
        D3D11_SUBRESOURCE_DATA *paInitialData = NULL;
        D3D11_SUBRESOURCE_DATA aInitialData[6 * SVGA3D_MAX_MIP_LEVELS];
        if (pSurface->paMipmapLevels[0].pSurfaceData)
        {
            /** @todo Can happen for a non GBO surface or if GBO texture was updated prior to creation if the hardware resource. Test this. */
            /** @todo for (i = 0; i < pSurface->cFaces * numMipLevels; ++i) */
            for (uint32_t iFace = 0; iFace < 6; ++iFace)
            {
                for (uint32_t i = 0; i < numMipLevels; ++i)
                {
                    uint32_t const iSubresource = vmsvga3dCalcSubresource(i, iFace, numMipLevels);

                    PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[iSubresource];
                    D3D11_SUBRESOURCE_DATA *p = &aInitialData[iSubresource];
                    p->pSysMem          = pMipmapLevel->pSurfaceData;
                    p->SysMemPitch      = pMipmapLevel->cbSurfacePitch;
                    p->SysMemSlicePitch = pMipmapLevel->cbSurfacePlane;
                }
            }
            paInitialData = &aInitialData[0];
        }

        D3D11_TEXTURE2D_DESC td;
        RT_ZERO(td);
        td.Width              = cWidth;
        td.Height             = cHeight;
        td.MipLevels          = 1;
        td.ArraySize          = 6;
        td.Format             = dxgiFormat;
        td.SampleDesc.Count   = 1;
        td.SampleDesc.Quality = 0;
        td.Usage              = D3D11_USAGE_DEFAULT;
        td.BindFlags          = dxBindFlags(pSurface->surfaceFlags);
        td.CPUAccessFlags     = 0;
        td.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE;

        hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->u.pTexture2D);
        Assert(SUCCEEDED(hr));
        if (SUCCEEDED(hr))
        {
            /* Map-able texture. */
            td.MipLevels      = 1; /* Must be for D3D11_USAGE_DYNAMIC. */
            td.ArraySize      = 1; /* Must be for D3D11_USAGE_DYNAMIC. */
            td.Usage          = D3D11_USAGE_DYNAMIC;
            td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
            td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            td.MiscFlags      = 0;
            hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pDynamicTexture);
            Assert(SUCCEEDED(hr));
        }

        if (SUCCEEDED(hr))
        {
            /* Staging texture. */
            td.Usage          = D3D11_USAGE_STAGING;
            td.BindFlags      = 0; /* No flags allowed. */
            td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            td.MiscFlags      = 0;
            hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pStagingTexture);
            Assert(SUCCEEDED(hr));
        }

        if (SUCCEEDED(hr))
        {
            pBackendSurface->enmResType = VMSVGA3D_RESTYPE_TEXTURE_CUBE;
        }
    }
    else if (pSurface->surfaceFlags & SVGA3D_SURFACE_1D)
    {
        AssertFailed(); /** @todo implement */
        hr = E_FAIL;
    }
    else
    {
        if (cDepth > 1)
        {
            AssertFailed(); /** @todo implement */
            hr = E_FAIL;
        }
        else
        {
            /*
             * 2D texture.
             */
            Assert(pSurface->cFaces == 1);
            Assert(numMipLevels == 1);

            D3D11_SUBRESOURCE_DATA *paInitialData = NULL;
            D3D11_SUBRESOURCE_DATA aInitialData[SVGA3D_MAX_MIP_LEVELS];
            if (pSurface->paMipmapLevels[0].pSurfaceData)
            {
                /** @todo Can happen for a non GBO surface or if GBO texture was updated prior to creation if the hardware resource. Test this. */
                for (uint32_t i = 0; i < numMipLevels; ++i)
                {
                    PVMSVGA3DMIPMAPLEVEL pMipmapLevel = &pSurface->paMipmapLevels[i];
                    D3D11_SUBRESOURCE_DATA *p = &aInitialData[i];
                    p->pSysMem          = pMipmapLevel->pSurfaceData;
                    p->SysMemPitch      = pMipmapLevel->cbSurfacePitch;
                    p->SysMemSlicePitch = pMipmapLevel->cbSurfacePlane;
                }
                paInitialData = &aInitialData[0];
            }

            D3D11_TEXTURE2D_DESC td;
            RT_ZERO(td);
            td.Width              = cWidth;
            td.Height             = cHeight;
            td.MipLevels          = 1;
            td.ArraySize          = 1;
            td.Format             = dxgiFormat;
            td.SampleDesc.Count   = 1;
            td.SampleDesc.Quality = 0;
            td.Usage              = D3D11_USAGE_DEFAULT;
            td.BindFlags          = dxBindFlags(pSurface->surfaceFlags);
            td.CPUAccessFlags     = 0;
            td.MiscFlags          = 0;

            hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->u.pTexture2D);
            Assert(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
            {
                /* Map-able texture. */
                td.MipLevels      = 1; /* Must be for D3D11_USAGE_DYNAMIC. */
                td.Usage          = D3D11_USAGE_DYNAMIC;
                td.BindFlags      = D3D11_BIND_SHADER_RESOURCE; /* Have to specify a supported flag, otherwise E_INVALIDARG will be returned. */
                td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                td.MiscFlags      = 0;
                hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pDynamicTexture);
                Assert(SUCCEEDED(hr));
            }

            if (SUCCEEDED(hr))
            {
                /* Staging texture. */
                td.Usage          = D3D11_USAGE_STAGING;
                td.BindFlags      = 0; /* No flags allowed. */
                td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
                td.MiscFlags      = 0;
                hr = pDXDevice->pDevice->CreateTexture2D(&td, paInitialData, &pBackendSurface->pStagingTexture);
                Assert(SUCCEEDED(hr));
            }

            if (SUCCEEDED(hr))
            {
                pBackendSurface->enmResType = VMSVGA3D_RESTYPE_TEXTURE_2D;
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmDxgiFormat = dxgiFormat;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        return VINF_SUCCESS;
    }

    /** @todo different enmResType Failure. */
    D3D_RELEASE(pBackendSurface->pStagingTexture);
    D3D_RELEASE(pBackendSurface->pDynamicTexture);
    D3D_RELEASE(pBackendSurface->u.pTexture2D);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static int vmsvga3dBackSurfaceCreateBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Buffers should be created as such. */
    AssertReturn(RT_BOOL(pSurface->surfaceFlags & (  SVGA3D_SURFACE_HINT_INDEXBUFFER
                                                   | SVGA3D_SURFACE_HINT_VERTEXBUFFER
                                                   | SVGA3D_SURFACE_BIND_VERTEX_BUFFER
                                                   | SVGA3D_SURFACE_BIND_INDEX_BUFFER
                        )), VERR_INVALID_PARAMETER);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }

    PVMSVGA3DMIPMAPLEVEL pMipLevel;
    int rc = vmsvga3dMipmapLevel(pSurface, 0, 0, &pMipLevel);
    AssertRCReturn(rc, rc);

    PVMSVGA3DBACKENDSURFACE pBackendSurface;
    rc = dxBackendSurfaceAlloc(&pBackendSurface);
    AssertRCReturn(rc, rc);

    LogFunc(("sid = %u, size = %u\n", pSurface->id, pMipLevel->cbSurface));

    /* Upload the current data, if any. */
    D3D11_SUBRESOURCE_DATA *pInitialData = NULL;
    D3D11_SUBRESOURCE_DATA initialData;
    if (pMipLevel->pSurfaceData)
    {
        initialData.pSysMem          = pMipLevel->pSurfaceData;
        initialData.SysMemPitch      = pMipLevel->cbSurface;
        initialData.SysMemSlicePitch = pMipLevel->cbSurface;

        pInitialData = &initialData;
    }

    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth = pMipLevel->cbSurface;
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER
                 | D3D11_BIND_INDEX_BUFFER;

    HRESULT hr = pDevice->pDevice->CreateBuffer(&bd, pInitialData, &pBackendSurface->u.pBuffer);
    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_BUFFER;
        pBackendSurface->enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.pBuffer);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static int vmsvga3dBackSurfaceCreateSoBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Buffers should be created as such. */
    AssertReturn(RT_BOOL(pSurface->surfaceFlags & SVGA3D_SURFACE_BIND_STREAM_OUTPUT), VERR_INVALID_PARAMETER);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface;
    int rc = dxBackendSurfaceAlloc(&pBackendSurface);
    AssertRCReturn(rc, rc);

    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth           = pSurface->paMipmapLevels[0].cbSurface;
    bd.Usage               = D3D11_USAGE_DEFAULT;
    bd.BindFlags           = dxBindFlags(pSurface->surfaceFlags); // D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_STREAM_OUTPUT;
    bd.CPUAccessFlags      = 0; /// @todo ? D3D11_CPU_ACCESS_READ;
    bd.MiscFlags           = 0;
    bd.StructureByteStride = 0;

    HRESULT hr = pDevice->pDevice->CreateBuffer(&bd, 0, &pBackendSurface->u.pBuffer);
    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_BUFFER;
        pBackendSurface->enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.pBuffer);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}

#if 0 /*unused*/
/** @todo Not needed */
static int vmsvga3dBackSurfaceCreateConstantBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Buffers should be created as such. */
    AssertReturn(RT_BOOL(pSurface->surfaceFlags & ( SVGA3D_SURFACE_BIND_CONSTANT_BUFFER)), VERR_INVALID_PARAMETER);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }

    PVMSVGA3DMIPMAPLEVEL pMipLevel;
    int rc = vmsvga3dMipmapLevel(pSurface, 0, 0, &pMipLevel);
    AssertRCReturn(rc, rc);

    PVMSVGA3DBACKENDSURFACE pBackendSurface;
    rc = dxBackendSurfaceAlloc(&pBackendSurface);
    AssertRCReturn(rc, rc);

    /* Upload the current data, if any. */
    D3D11_SUBRESOURCE_DATA *pInitialData = NULL;
    D3D11_SUBRESOURCE_DATA initialData;
    if (pMipLevel->pSurfaceData)
    {
        initialData.pSysMem          = pMipLevel->pSurfaceData;
        initialData.SysMemPitch      = pMipLevel->cbSurface;
        initialData.SysMemSlicePitch = pMipLevel->cbSurface;

        pInitialData = &initialData;
    }

    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth           = pMipLevel->cbSurface;
    bd.Usage               = D3D11_USAGE_DYNAMIC;
    bd.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags           = 0;
    bd.StructureByteStride = 0;

    HRESULT hr = pDevice->pDevice->CreateBuffer(&bd, pInitialData, &pBackendSurface->u.pBuffer);
    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pBackendSurface->enmResType = VMSVGA3D_RESTYPE_BUFFER;
        pBackendSurface->enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        return VINF_SUCCESS;
    }

    /* Failure. */
    D3D_RELEASE(pBackendSurface->u.pBuffer);
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}


static HRESULT dxCreateConstantBuffer(DXDEVICE *pDevice, VMSVGA3DSURFACE const *pSurface, PVMSVGA3DBACKENDSURFACE pBackendSurface)
{
    D3D11_SUBRESOURCE_DATA *pInitialData = NULL; /** @todo */
    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth           = pSurface->paMipmapLevels[0].cbSurface;
    bd.Usage               = D3D11_USAGE_DYNAMIC; /** @todo HINT_STATIC */
    bd.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags           = 0;
    bd.StructureByteStride = 0;

    return pDevice->pDevice->CreateBuffer(&bd, pInitialData, &pBackendSurface->u.pBuffer);
}


static HRESULT dxCreateBuffer(DXDEVICE *pDevice, VMSVGA3DSURFACE const *pSurface, PVMSVGA3DBACKENDSURFACE pBackendSurface)
{
    D3D11_SUBRESOURCE_DATA *pInitialData = NULL; /** @todo */
    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth           = pSurface->paMipmapLevels[0].cbSurface;
    bd.Usage               = D3D11_USAGE_DYNAMIC; /** @todo HINT_STATIC */
    bd.BindFlags           = D3D11_BIND_VERTEX_BUFFER
                           | D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags           = 0;
    bd.StructureByteStride = 0;

    return pDevice->pDevice->CreateBuffer(&bd, pInitialData, &pBackendSurface->u.pBuffer);
}

/** @todo Not needed? */
static int vmsvga3dBackSurfaceCreate(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PVMSVGA3DSURFACE pSurface)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    if (pSurface->pBackendSurface != NULL)
    {
        AssertFailed(); /** @todo Should the function not be used like that? */
        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }

    PVMSVGA3DBACKENDSURFACE pBackendSurface;
    int rc = dxBackendSurfaceAlloc(&pBackendSurface);
    AssertRCReturn(rc, rc);

    HRESULT hr;

    /*
     * Figure out the type of the surface.
     */
    if (pSurface->surfaceFlags & SVGA3D_SURFACE_BIND_CONSTANT_BUFFER)
    {
        hr = dxCreateConstantBuffer(pDevice, pSurface, pBackendSurface);
        if (SUCCEEDED(hr))
        {
            pBackendSurface->enmResType = VMSVGA3D_RESTYPE_BUFFER;
            pBackendSurface->enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
        }
        else
            D3D_RELEASE(pBackendSurface->u.pBuffer);
    }
    else if (pSurface->surfaceFlags & (  SVGA3D_SURFACE_BIND_VERTEX_BUFFER
                                       | SVGA3D_SURFACE_BIND_INDEX_BUFFER
                                       | SVGA3D_SURFACE_HINT_VERTEXBUFFER
                                       | SVGA3D_SURFACE_HINT_INDEXBUFFER))
    {
        hr = dxCreateBuffer(pDevice, pSurface, pBackendSurface);
        if (SUCCEEDED(hr))
        {
            pBackendSurface->enmResType = VMSVGA3D_RESTYPE_BUFFER;
            pBackendSurface->enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
        }
        else
            D3D_RELEASE(pBackendSurface->u.pBuffer);
    }
    else
    {
        AssertFailed(); /** @todo implement */
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        /*
         * Success.
         */
        pSurface->pBackendSurface = pBackendSurface;
        pSurface->idAssociatedContext = pDXContext->cid;
        return VINF_SUCCESS;
    }

    /* Failure. */
    RTMemFree(pBackendSurface);
    return VERR_NO_MEMORY;
}
#endif


static int dxStagingBufferRealloc(DXDEVICE *pDXDevice, uint32_t cbRequiredSize)
{
    AssertReturn(cbRequiredSize < SVGA3D_MAX_SURFACE_MEM_SIZE, VERR_INVALID_PARAMETER);

    if (RT_LIKELY(cbRequiredSize <= pDXDevice->cbStagingBuffer))
        return VINF_SUCCESS;

    D3D_RELEASE(pDXDevice->pStagingBuffer);

    uint32_t const cbAlloc = RT_ALIGN_32(cbRequiredSize, _64K);

    D3D11_SUBRESOURCE_DATA *pInitialData = NULL;
    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth           = cbAlloc;
    bd.Usage               = D3D11_USAGE_STAGING;
    //bd.BindFlags         = 0; /* No bind flags are allowed for staging resources. */
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;

    int rc = VINF_SUCCESS;
    ID3D11Buffer *pBuffer;
    HRESULT hr = pDXDevice->pDevice->CreateBuffer(&bd, pInitialData, &pBuffer);
    if (SUCCEEDED(hr))
    {
        pDXDevice->pStagingBuffer = pBuffer;
        pDXDevice->cbStagingBuffer = cbAlloc;
    }
    else
    {
        pDXDevice->cbStagingBuffer = 0;
        rc = VERR_NO_MEMORY;
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackInit(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC)
{
    RT_NOREF(pDevIns, pThis);

    int rc;
#ifdef RT_OS_LINUX /** @todo Remove, this is currently needed for loading the X11 library in order to call XInitThreads(). */
    rc = glLdrInit(pDevIns);
    if (RT_FAILURE(rc))
    {
        LogRel(("VMSVGA3d: Error loading OpenGL library and resolving necessary functions: %Rrc\n", rc));
        return rc;
    }
#endif

    PVMSVGA3DSTATE pState = (PVMSVGA3DSTATE)RTMemAllocZ(sizeof(VMSVGA3DSTATE));
    AssertReturn(pState, VERR_NO_MEMORY);
    pThisCC->svga.p3dState = pState;

    PVMSVGA3DBACKEND pBackend = (PVMSVGA3DBACKEND)RTMemAllocZ(sizeof(VMSVGA3DBACKEND));
    AssertReturn(pBackend, VERR_NO_MEMORY);
    pState->pBackend = pBackend;

    rc = RTLdrLoadSystem(VBOX_D3D11_LIBRARY_NAME, /* fNoUnload = */ true, &pBackend->hD3D11);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(pBackend->hD3D11, "D3D11CreateDevice", (void **)&pBackend->pfnD3D11CreateDevice);
        AssertRC(rc);
    }

    if (RT_SUCCESS(rc))
    {
        /* Failure to load the shader disassembler is ignored. */
        int rc2 = RTLdrLoadSystem("D3DCompiler_47", /* fNoUnload = */ true, &pBackend->hD3DCompiler);
        AssertRC(rc2);
        if (RT_SUCCESS(rc2))
        {
            rc2 = RTLdrGetSymbol(pBackend->hD3DCompiler, "D3DDisassemble", (void **)&pBackend->pfnD3DDisassemble);
            AssertRC(rc2);
        }
        Log6Func(("Load D3DDisassemble: %Rrc\n", rc2));
    }

#if !defined(RT_OS_WINDOWS) || defined(DX_FORCE_SINGLE_DEVICE)
    pBackend->fSingleDevice = true;
#endif

    LogRelMax(1, ("VMSVGA: Single DX device mode: %s\n", pBackend->fSingleDevice ? "enabled" : "disabled"));

//DEBUG_BREAKPOINT_TEST();
    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackPowerOn(PPDMDEVINS pDevIns, PVGASTATE pThis, PVGASTATECC pThisCC)
{
    RT_NOREF(pDevIns, pThis);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    int rc = dxDeviceCreate(pBackend, &pBackend->dxDevice);
    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackReset(PVGASTATECC pThisCC)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    /** @todo This is generic code. Must be moved to in DevVGA-SVGA3d.cpp */
    /* Destroy all leftover surfaces. */
    for (uint32_t i = 0; i < pState->cSurfaces; i++)
    {
        if (pState->papSurfaces[i]->id != SVGA3D_INVALID_ID)
            vmsvga3dSurfaceDestroy(pThisCC, pState->papSurfaces[i]->id);
    }

    /* Destroy all leftover DX contexts. */
    for (uint32_t i = 0; i < pState->cDXContexts; i++)
    {
        if (pState->papDXContexts[i]->cid != SVGA3D_INVALID_ID)
            vmsvga3dDXDestroyContext(pThisCC, pState->papDXContexts[i]->cid);
    }

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackTerminate(PVGASTATECC pThisCC)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    if (pState->pBackend)
    {
        /* Clean up backends. For example release resources from surfaces. */
        vmsvga3dBackReset(pThisCC);

        dxDeviceDestroy(pState->pBackend, &pState->pBackend->dxDevice);

        RTMemFree(pState->pBackend);
        pState->pBackend = NULL;
    }

    return VINF_SUCCESS;
}


/** @todo Such structures must be in VBoxVideo3D.h */
typedef struct VBOX3DNOTIFYDEFINESCREEN
{
    VBOX3DNOTIFY Core;
    uint32_t cWidth;
    uint32_t cHeight;
    int32_t  xRoot;
    int32_t  yRoot;
    uint32_t fPrimary;
    uint32_t cDpi;
} VBOX3DNOTIFYDEFINESCREEN;


static int vmsvga3dDrvNotifyDefineScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    VBOX3DNOTIFYDEFINESCREEN n;
    n.Core.enmNotification = VBOX3D_NOTIFY_TYPE_HW_SCREEN_CREATED;
    n.Core.iDisplay        = pScreen->idScreen;
    n.Core.u32Reserved     = 0;
    n.Core.cbData          = sizeof(n) - RT_UOFFSETOF(VBOX3DNOTIFY, au8Data);
    RT_ZERO(n.Core.au8Data);
    n.cWidth               = pScreen->cWidth;
    n.cHeight              = pScreen->cHeight;
    n.xRoot                = pScreen->xOrigin;
    n.yRoot                = pScreen->yOrigin;
    n.fPrimary             = RT_BOOL(pScreen->fuScreen & SVGA_SCREEN_IS_PRIMARY);
    n.cDpi                 = pScreen->cDpi;

    return pThisCC->pDrv->pfn3DNotifyProcess(pThisCC->pDrv, &n.Core);
}


static int vmsvga3dDrvNotifyDestroyScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    VBOX3DNOTIFY n;
    n.enmNotification = VBOX3D_NOTIFY_TYPE_HW_SCREEN_DESTROYED;
    n.iDisplay        = pScreen->idScreen;
    n.u32Reserved     = 0;
    n.cbData          = sizeof(n) - RT_UOFFSETOF(VBOX3DNOTIFY, au8Data);
    RT_ZERO(n.au8Data);

    return pThisCC->pDrv->pfn3DNotifyProcess(pThisCC->pDrv, &n);
}


static int vmsvga3dDrvNotifyBindSurface(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, HANDLE hSharedSurface)
{
    VBOX3DNOTIFY n;
    n.enmNotification = VBOX3D_NOTIFY_TYPE_HW_SCREEN_BIND_SURFACE;
    n.iDisplay        = pScreen->idScreen;
    n.u32Reserved     = 0;
    n.cbData          = sizeof(n) - RT_UOFFSETOF(VBOX3DNOTIFY, au8Data);
    *(uint64_t *)&n.au8Data[0] = (uint64_t)hSharedSurface;

    return pThisCC->pDrv->pfn3DNotifyProcess(pThisCC->pDrv, &n);
}


typedef struct VBOX3DNOTIFYUPDATE
{
    VBOX3DNOTIFY Core;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} VBOX3DNOTIFYUPDATE;


static int vmsvga3dDrvNotifyUpdate(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen,
                                   uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    VBOX3DNOTIFYUPDATE n;
    n.Core.enmNotification = VBOX3D_NOTIFY_TYPE_HW_SCREEN_UPDATE_END;
    n.Core.iDisplay        = pScreen->idScreen;
    n.Core.u32Reserved     = 0;
    n.Core.cbData          = sizeof(n) - RT_UOFFSETOF(VBOX3DNOTIFY, au8Data);
    RT_ZERO(n.Core.au8Data);
    n.x = x;
    n.y = y;
    n.w = w;
    n.h = h;

    return pThisCC->pDrv->pfn3DNotifyProcess(pThisCC->pDrv, &n.Core);
}

static int vmsvga3dHwScreenCreate(PVMSVGA3DSTATE pState, uint32_t cWidth, uint32_t cHeight, VMSVGAHWSCREEN *p)
{
    PVMSVGA3DBACKEND pBackend = pState->pBackend;

    DXDEVICE *pDXDevice = &pBackend->dxDevice;
    AssertReturn(pDXDevice->pDevice, VERR_INVALID_STATE);

    D3D11_TEXTURE2D_DESC td;
    RT_ZERO(td);
    td.Width              = cWidth;
    td.Height             = cHeight;
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count   = 1;
    td.SampleDesc.Quality = 0;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags     = 0;
    td.MiscFlags          = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    HRESULT hr = pDXDevice->pDevice->CreateTexture2D(&td, 0, &p->pTexture);
    if (SUCCEEDED(hr))
    {
        /* Get the shared handle. */
        hr = p->pTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&p->pDxgiResource);
        if (SUCCEEDED(hr))
        {
            hr = p->pDxgiResource->GetSharedHandle(&p->SharedHandle);
            if (SUCCEEDED(hr))
                hr = p->pTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&p->pDXGIKeyedMutex);
        }
    }

    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    AssertFailed();
    return VERR_NOT_SUPPORTED;
}


static void vmsvga3dHwScreenDestroy(PVMSVGA3DSTATE pState, VMSVGAHWSCREEN *p)
{
    RT_NOREF(pState);
    D3D_RELEASE(p->pDXGIKeyedMutex);
    D3D_RELEASE(p->pDxgiResource);
    D3D_RELEASE(p->pTexture);
    p->SharedHandle = 0;
    p->sidScreenTarget = SVGA_ID_INVALID;
}


static DECLCALLBACK(int) vmsvga3dBackDefineScreen(PVGASTATE pThis, PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    RT_NOREF(pThis, pThisCC, pScreen);

    LogRel4(("VMSVGA: vmsvga3dBackDefineScreen: screen %u\n", pScreen->idScreen));

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    Assert(pScreen->pHwScreen == NULL);

    VMSVGAHWSCREEN *p = (VMSVGAHWSCREEN *)RTMemAllocZ(sizeof(VMSVGAHWSCREEN));
    AssertPtrReturn(p, VERR_NO_MEMORY);

    p->sidScreenTarget = SVGA_ID_INVALID;

    int rc = vmsvga3dDrvNotifyDefineScreen(pThisCC, pScreen);
    if (RT_SUCCESS(rc))
    {
        /* The frontend supports the screen. Create the actual resource. */
        rc = vmsvga3dHwScreenCreate(pState, pScreen->cWidth, pScreen->cHeight, p);
        if (RT_SUCCESS(rc))
            LogRel4(("VMSVGA: vmsvga3dBackDefineScreen: created\n"));
    }

    if (RT_SUCCESS(rc))
    {
        LogRel(("VMSVGA: Using HW accelerated screen %u\n", pScreen->idScreen));
        pScreen->pHwScreen = p;
    }
    else
    {
        LogRel4(("VMSVGA: vmsvga3dBackDefineScreen: %Rrc\n", rc));
        vmsvga3dHwScreenDestroy(pState, p);
        RTMemFree(p);
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackDestroyScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    vmsvga3dDrvNotifyDestroyScreen(pThisCC, pScreen);

    if (pScreen->pHwScreen)
    {
        vmsvga3dHwScreenDestroy(pState, pScreen->pHwScreen);
        RTMemFree(pScreen->pHwScreen);
        pScreen->pHwScreen = NULL;
    }

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSurfaceBlitToScreen(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen,
                                    SVGASignedRect destRect, SVGA3dSurfaceImageId srcImage,
                                    SVGASignedRect srcRect, uint32_t cRects, SVGASignedRect *paRects)
{
    RT_NOREF(pThisCC, pScreen, destRect, srcImage, srcRect, cRects, paRects);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    VMSVGAHWSCREEN *p = pScreen->pHwScreen;
    AssertReturn(p, VERR_NOT_SUPPORTED);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, srcImage.sid, &pSurface);
    AssertRCReturn(rc, rc);

    /** @todo Implement. */
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackSurfaceMap(PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pImage, SVGA3dBox const *pBox,
                                                VMSVGA3D_SURFACE_MAP enmMapType, VMSVGA3D_MAPPED_SURFACE *pMap)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, pImage->sid, &pSurface);
    AssertRCReturn(rc, rc);

    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    AssertPtrReturn(pBackendSurface, VERR_INVALID_STATE);

    PVMSVGA3DMIPMAPLEVEL pMipLevel;
    rc = vmsvga3dMipmapLevel(pSurface, pImage->face, pImage->mipmap, &pMipLevel);
    ASSERT_GUEST_RETURN(RT_SUCCESS(rc), rc);

    /* A surface is always mapped by the DX context which has created the surface. */
    DXDEVICE *pDevice = dxDeviceFromCid(pSurface->idAssociatedContext, pState);
    AssertReturn(pDevice && pDevice->pDevice, VERR_INVALID_STATE);

    SVGA3dBox clipBox;
    if (pBox)
    {
        clipBox = *pBox;
        vmsvgaR3ClipBox(&pMipLevel->mipmapSize, &clipBox);
        ASSERT_GUEST_RETURN(clipBox.w && clipBox.h && clipBox.d, VERR_INVALID_PARAMETER);
    }
    else
    {
        clipBox.x = 0;
        clipBox.y = 0;
        clipBox.z = 0;
        clipBox.w = pMipLevel->mipmapSize.width;
        clipBox.h = pMipLevel->mipmapSize.height;
        clipBox.d = pMipLevel->mipmapSize.depth;
    }

    D3D11_MAP d3d11MapType;
    switch (enmMapType)
    {
        case VMSVGA3D_SURFACE_MAP_READ:          d3d11MapType = D3D11_MAP_READ; break;
        case VMSVGA3D_SURFACE_MAP_WRITE:         d3d11MapType = D3D11_MAP_WRITE; break;
        case VMSVGA3D_SURFACE_MAP_READ_WRITE:    d3d11MapType = D3D11_MAP_READ_WRITE; break;
        case VMSVGA3D_SURFACE_MAP_WRITE_DISCARD: d3d11MapType = D3D11_MAP_WRITE_DISCARD; break;
        default:
            AssertFailed();
            return VERR_INVALID_PARAMETER;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    RT_ZERO(mappedResource);

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        Assert(pImage->face == 0 && pImage->mipmap == 0);

        /* Wait for the surface to finish drawing. */
        dxSurfaceWait(pState, pSurface, pSurface->idAssociatedContext);

        ID3D11Texture2D *pMappedTexture;
        if (enmMapType == VMSVGA3D_SURFACE_MAP_READ)
        {
            pMappedTexture = pBackendSurface->pStagingTexture;

            /* Copy the texture content to the staging texture. */
            pDevice->pImmediateContext->CopyResource(pBackendSurface->pStagingTexture, pBackendSurface->u.pTexture2D);
        }
        else if (enmMapType == VMSVGA3D_SURFACE_MAP_WRITE)
            pMappedTexture = pBackendSurface->pStagingTexture;
        else
            pMappedTexture = pBackendSurface->pDynamicTexture;

        UINT const Subresource = 0; /* Screen target surfaces have only one subresource. */
        HRESULT hr = pDevice->pImmediateContext->Map(pMappedTexture, Subresource,
                                                     d3d11MapType, /* MapFlags =  */ 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            pMap->enmMapType   = enmMapType;
            pMap->format       = pSurface->format;
            pMap->box          = clipBox;
            pMap->cbPixel      = pSurface->cbBlock;
            pMap->cbRowPitch   = mappedResource.RowPitch;
            pMap->cbDepthPitch = mappedResource.DepthPitch;
            pMap->pvData       = (uint8_t *)mappedResource.pData
                               + pMap->box.x * pMap->cbPixel
                               + pMap->box.y * pMap->cbRowPitch
                               + pMap->box.z * pMap->cbDepthPitch;
        }
        else
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
    }
    else if (   pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_2D
             || pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_CUBE
             || pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
    {
//Assert(pImage->face == 0 && pImage->mipmap == 0);
if (!(   (pBackendSurface->pStagingTexture && pBackendSurface->pDynamicTexture)
      || (pBackendSurface->pStagingTexture3D && pBackendSurface->pDynamicTexture3D)
     )
   )
{
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}

        dxSurfaceWait(pState, pSurface, pSurface->idAssociatedContext);

        ID3D11Resource *pMappedResource;
        if (enmMapType == VMSVGA3D_SURFACE_MAP_READ)
        {
            pMappedResource = (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
                            ? (ID3D11Resource *)pBackendSurface->pStagingTexture3D
                            : (ID3D11Resource *)pBackendSurface->pStagingTexture;

            /* Copy the texture content to the staging texture.
             * The requested miplevel of the texture is copied to the miplevel 0 of the staging texture,
             * because the staging (and dynamic) structures do not have miplevels.
             * Always copy entire miplevel so all Dst are zero and pSrcBox is NULL, as D3D11 requires.
             */
            ID3D11Resource *pDstResource = pMappedResource;
            UINT DstSubresource = 0;
            UINT DstX = 0;
            UINT DstY = 0;
            UINT DstZ = 0;
            ID3D11Resource *pSrcResource = pBackendSurface->u.pResource;
            UINT SrcSubresource = D3D11CalcSubresource(pImage->mipmap, pImage->face, pSurface->cLevels);
            D3D11_BOX *pSrcBox = NULL;
            //D3D11_BOX SrcBox;
            //SrcBox.left   = 0;
            //SrcBox.top    = 0;
            //SrcBox.front  = 0;
            //SrcBox.right  = pMipLevel->mipmapSize.width;
            //SrcBox.bottom = pMipLevel->mipmapSize.height;
            //SrcBox.back   = pMipLevel->mipmapSize.depth;
            pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                              pSrcResource, SrcSubresource, pSrcBox);
        }
        else if (enmMapType == VMSVGA3D_SURFACE_MAP_WRITE)
            pMappedResource = (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
                            ? (ID3D11Resource *)pBackendSurface->pStagingTexture3D
                            : (ID3D11Resource *)pBackendSurface->pStagingTexture;
        else
            pMappedResource = (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
                            ? (ID3D11Resource *)pBackendSurface->pDynamicTexture3D
                            : (ID3D11Resource *)pBackendSurface->pDynamicTexture;

        UINT const Subresource = 0;
        HRESULT hr = pDevice->pImmediateContext->Map(pMappedResource, Subresource,
                                                     d3d11MapType, /* MapFlags =  */ 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            pMap->enmMapType   = enmMapType;
            pMap->format       = pSurface->format;
            pMap->box          = clipBox;
            pMap->cbPixel      = pSurface->cbBlock;
            pMap->cbRowPitch   = mappedResource.RowPitch;
            pMap->cbDepthPitch = mappedResource.DepthPitch;
            pMap->pvData       = (uint8_t *)mappedResource.pData
                               + (pMap->box.x / pSurface->cxBlock) * pMap->cbPixel
                               + (pMap->box.y / pSurface->cyBlock) * pMap->cbRowPitch
                               + pMap->box.z * pMap->cbDepthPitch;
        }
        else
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
    }
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_BUFFER)
    {
        /* Map the staging buffer. */
        rc = dxStagingBufferRealloc(pDevice, pMipLevel->cbSurface);
        if (RT_SUCCESS(rc))
        {
            /* The staging buffer does not allow D3D11_MAP_WRITE_DISCARD, so replace it.  */
            if (d3d11MapType == D3D11_MAP_WRITE_DISCARD)
                d3d11MapType = D3D11_MAP_WRITE;

            if (enmMapType == VMSVGA3D_SURFACE_MAP_READ)
            {
                /* Copy from the buffer to the staging buffer. */
                ID3D11Resource *pDstResource = pDevice->pStagingBuffer;
                UINT DstSubresource = 0;
                UINT DstX = clipBox.x;
                UINT DstY = clipBox.y;
                UINT DstZ = clipBox.z;
                ID3D11Resource *pSrcResource = pBackendSurface->u.pResource;
                UINT SrcSubresource = 0;
                D3D11_BOX SrcBox;
                SrcBox.left   = clipBox.x;
                SrcBox.top    = clipBox.y;
                SrcBox.front  = clipBox.z;
                SrcBox.right  = clipBox.w;
                SrcBox.bottom = clipBox.h;
                SrcBox.back   = clipBox.d;
                pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                                  pSrcResource, SrcSubresource, &SrcBox);
            }

            UINT const Subresource = 0; /* Buffers have only one subresource. */
            HRESULT hr = pDevice->pImmediateContext->Map(pDevice->pStagingBuffer, Subresource,
                                                         d3d11MapType, /* MapFlags =  */ 0, &mappedResource);
            if (SUCCEEDED(hr))
            {
                pMap->enmMapType   = enmMapType;
                pMap->format       = pSurface->format;
                pMap->box          = clipBox;
                pMap->cbPixel      = pSurface->cbBlock;
                pMap->cbRowPitch   = mappedResource.RowPitch;
                pMap->cbDepthPitch = mappedResource.DepthPitch;
                pMap->pvData       = (uint8_t *)mappedResource.pData
                                   + pMap->box.x * pMap->cbPixel
                                   + pMap->box.y * pMap->cbRowPitch
                                   + pMap->box.z * pMap->cbDepthPitch;
            }
            else
                AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
        }
    }
    else
    {
        // UINT D3D11CalcSubresource(UINT MipSlice, UINT ArraySlice, UINT MipLevels);
        /** @todo Implement. */
        AssertFailed();
        rc = VERR_NOT_IMPLEMENTED;
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackSurfaceUnmap(PVGASTATECC pThisCC, SVGA3dSurfaceImageId const *pImage, VMSVGA3D_MAPPED_SURFACE *pMap, bool fWritten)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, pImage->sid, &pSurface);
    AssertRCReturn(rc, rc);

    /* The called should not use the function for system memory surfaces. */
    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    AssertReturn(pBackendSurface, VERR_INVALID_PARAMETER);

    PVMSVGA3DMIPMAPLEVEL pMipLevel;
    rc = vmsvga3dMipmapLevel(pSurface, pImage->face, pImage->mipmap, &pMipLevel);
    ASSERT_GUEST_RETURN(RT_SUCCESS(rc), rc);

    /* A surface is always mapped by the DX context which has created the surface. */
    DXDEVICE *pDevice = dxDeviceFromCid(pSurface->idAssociatedContext, pState);
    AssertReturn(pDevice && pDevice->pDevice, VERR_INVALID_STATE);

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        ID3D11Texture2D *pMappedTexture;
        if (pMap->enmMapType == VMSVGA3D_SURFACE_MAP_READ)
            pMappedTexture = pBackendSurface->pStagingTexture;
        else if (pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE)
            pMappedTexture = pBackendSurface->pStagingTexture;
        else
            pMappedTexture = pBackendSurface->pDynamicTexture;

        UINT const Subresource = 0; /* Screen target surfaces have only one subresource. */
        pDevice->pImmediateContext->Unmap(pMappedTexture, Subresource);

        if (   fWritten
            && (   pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE
                || pMap->enmMapType == VMSVGA3D_SURFACE_MAP_READ_WRITE
                || pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE_DISCARD))
        {
            ID3D11Resource *pDstResource = pBackendSurface->u.pTexture2D;
            UINT DstSubresource = Subresource;
            UINT DstX = pMap->box.x;
            UINT DstY = pMap->box.y;
            UINT DstZ = pMap->box.z;
            ID3D11Resource *pSrcResource = pMappedTexture;
            UINT SrcSubresource = Subresource;
            D3D11_BOX SrcBox;
            SrcBox.left   = pMap->box.x;
            SrcBox.top    = pMap->box.y;
            SrcBox.front  = pMap->box.z;
            SrcBox.right  = pMap->box.x + pMap->box.w;
            SrcBox.bottom = pMap->box.y + pMap->box.h;
            SrcBox.back   = pMap->box.z + pMap->box.d;

            pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                              pSrcResource, SrcSubresource, &SrcBox);

            pBackendSurface->cidDrawing = pSurface->idAssociatedContext;
        }
    }
    else if (   pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_2D
             || pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_CUBE
             || pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
    {
        ID3D11Resource *pMappedResource;
        if (pMap->enmMapType == VMSVGA3D_SURFACE_MAP_READ)
            pMappedResource = (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
                            ? (ID3D11Resource *)pBackendSurface->pStagingTexture3D
                            : (ID3D11Resource *)pBackendSurface->pStagingTexture;
        else if (pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE)
            pMappedResource = (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
                            ? (ID3D11Resource *)pBackendSurface->pStagingTexture3D
                            : (ID3D11Resource *)pBackendSurface->pStagingTexture;
        else
            pMappedResource = (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
                            ? (ID3D11Resource *)pBackendSurface->pDynamicTexture3D
                            : (ID3D11Resource *)pBackendSurface->pDynamicTexture;

        UINT const Subresource = 0; /* Staging or dynamic textures have one subresource. */
        pDevice->pImmediateContext->Unmap(pMappedResource, Subresource);

        if (   fWritten
            && (   pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE
                || pMap->enmMapType == VMSVGA3D_SURFACE_MAP_READ_WRITE
                || pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE_DISCARD))
        {
            /* If entire resource must be copied then use pSrcBox = NULL and dst point (0,0,0)
             * Because DX11 insists on this for some resource types, for example DEPTH_STENCIL resources.
             */
            uint32_t const cWidth0 = pSurface->paMipmapLevels[0].mipmapSize.width;
            uint32_t const cHeight0 = pSurface->paMipmapLevels[0].mipmapSize.height;
            uint32_t const cDepth0 = pSurface->paMipmapLevels[0].mipmapSize.depth;
            bool const fEntireResource = pMap->box.x == 0 && pMap->box.y == 0 && pMap->box.z == 0
                                      && pMap->box.w == cWidth0 && pMap->box.h == cHeight0 && pMap->box.d == cDepth0;

            ID3D11Resource *pDstResource = pBackendSurface->u.pResource;
            UINT DstSubresource = D3D11CalcSubresource(pImage->mipmap, pImage->face, pSurface->cLevels);
            UINT DstX = (pMap->box.x / pSurface->cxBlock) * pSurface->cxBlock;
            UINT DstY = (pMap->box.y / pSurface->cyBlock) * pSurface->cyBlock;
            UINT DstZ = pMap->box.z;
            ID3D11Resource *pSrcResource = pMappedResource;
            UINT SrcSubresource = Subresource;
            D3D11_BOX *pSrcBox;
            D3D11_BOX SrcBox;
            if (fEntireResource)
                pSrcBox = NULL;
            else
            {
                uint32_t const cxBlocks = (pMap->box.w + pSurface->cxBlock - 1) / pSurface->cxBlock;
                uint32_t const cyBlocks = (pMap->box.h + pSurface->cyBlock - 1) / pSurface->cyBlock;

                SrcBox.left   = DstX;
                SrcBox.top    = DstY;
                SrcBox.front  = DstZ;
                SrcBox.right  = DstX + cxBlocks * pSurface->cxBlock;
                SrcBox.bottom = DstY + cyBlocks * pSurface->cyBlock;
                SrcBox.back   = DstZ + pMap->box.d;
                pSrcBox = &SrcBox;
            }

            pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                              pSrcResource, SrcSubresource, pSrcBox);

            pBackendSurface->cidDrawing = pSurface->idAssociatedContext;
        }
    }
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_BUFFER)
    {
        /* Unmap the staging buffer. */
        UINT const Subresource = 0; /* Buffers have only one subresource. */
        pDevice->pImmediateContext->Unmap(pDevice->pStagingBuffer, Subresource);

        /* Copy from the staging buffer to the actual buffer */
        if (   fWritten
            && (   pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE
                || pMap->enmMapType == VMSVGA3D_SURFACE_MAP_READ_WRITE
                || pMap->enmMapType == VMSVGA3D_SURFACE_MAP_WRITE_DISCARD))
        {
            ID3D11Resource *pDstResource = pBackendSurface->u.pResource;
            UINT DstSubresource = 0;
            UINT DstX = (pMap->box.x / pSurface->cxBlock) * pSurface->cxBlock;
            UINT DstY = (pMap->box.y / pSurface->cyBlock) * pSurface->cyBlock;
            UINT DstZ = pMap->box.z;
            ID3D11Resource *pSrcResource = pDevice->pStagingBuffer;
            UINT SrcSubresource = 0;
            D3D11_BOX SrcBox;

            uint32_t const cxBlocks = (pMap->box.w + pSurface->cxBlock - 1) / pSurface->cxBlock;
            uint32_t const cyBlocks = (pMap->box.h + pSurface->cyBlock - 1) / pSurface->cyBlock;

            SrcBox.left   = DstX;
            SrcBox.top    = DstY;
            SrcBox.front  = DstZ;
            SrcBox.right  = DstX + cxBlocks * pSurface->cxBlock;
            SrcBox.bottom = DstY + cyBlocks * pSurface->cyBlock;
            SrcBox.back   = DstZ + pMap->box.d;

            pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                              pSrcResource, SrcSubresource, &SrcBox);
        }
    }
    else
    {
        AssertFailed();
        rc = VERR_NOT_IMPLEMENTED;
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dScreenTargetBind(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, uint32_t sid)
{
    int rc = VINF_SUCCESS;

    PVMSVGA3DSURFACE pSurface;
    if (sid != SVGA_ID_INVALID)
    {
        /* Create the surface if does not yet exist. */
        PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
        AssertReturn(pState, VERR_INVALID_STATE);

        rc = vmsvga3dSurfaceFromSid(pState, sid, &pSurface);
        AssertRCReturn(rc, rc);

        if (!VMSVGA3DSURFACE_HAS_HW_SURFACE(pSurface))
        {
            /* Create the actual texture. */
            rc = vmsvga3dBackSurfaceCreateScreenTarget(pThisCC, pSurface);
            AssertRCReturn(rc, rc);
        }
    }
    else
        pSurface = NULL;

    /* Notify the HW accelerated screen if it is used. */
    VMSVGAHWSCREEN *pHwScreen = pScreen->pHwScreen;
    if (!pHwScreen)
        return VINF_SUCCESS;

    /* Same surface -> do nothing. */
    if (pHwScreen->sidScreenTarget == sid)
        return VINF_SUCCESS;

    if (sid != SVGA_ID_INVALID)
    {
        AssertReturn(   pSurface->pBackendSurface
                     && pSurface->pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET, VERR_INVALID_PARAMETER);

        HANDLE const hSharedSurface = pHwScreen->SharedHandle;
        rc = vmsvga3dDrvNotifyBindSurface(pThisCC, pScreen, hSharedSurface);
    }

    if (RT_SUCCESS(rc))
    {
        pHwScreen->sidScreenTarget = sid;
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dScreenTargetUpdate(PVGASTATECC pThisCC, VMSVGASCREENOBJECT *pScreen, SVGA3dRect const *pRect)
{
    VMSVGAHWSCREEN *pHwScreen = pScreen->pHwScreen;
    AssertReturn(pHwScreen, VERR_NOT_SUPPORTED);

    if (pHwScreen->sidScreenTarget == SVGA_ID_INVALID)
        return VINF_SUCCESS; /* No surface bound. */

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;
    AssertReturn(pBackend, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pState, pHwScreen->sidScreenTarget, &pSurface);
    AssertRCReturn(rc, rc);

    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    AssertReturn(pBackendSurface && pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET, VERR_INVALID_PARAMETER);

    SVGA3dRect boundRect;
    boundRect.x = 0;
    boundRect.y = 0;
    boundRect.w = pSurface->paMipmapLevels[0].mipmapSize.width;
    boundRect.h = pSurface->paMipmapLevels[0].mipmapSize.height;
    SVGA3dRect clipRect = *pRect;
    vmsvgaR3Clip3dRect(&boundRect, &clipRect);
    ASSERT_GUEST_RETURN(clipRect.w && clipRect.h, VERR_INVALID_PARAMETER);

    /* Wait for the surface to finish drawing. */
    dxSurfaceWait(pState, pSurface, DX_CID_BACKEND);

    /* Copy the screen texture to the shared surface. */
    DWORD result = pHwScreen->pDXGIKeyedMutex->AcquireSync(0, 10000);
    if (result == S_OK)
    {
        pBackend->dxDevice.pImmediateContext->CopyResource(pHwScreen->pTexture, pBackendSurface->u.pTexture2D);

        dxDeviceFlush(&pBackend->dxDevice);

        result = pHwScreen->pDXGIKeyedMutex->ReleaseSync(1);
    }
    else
        AssertFailed();

    rc = vmsvga3dDrvNotifyUpdate(pThisCC, pScreen, pRect->x, pRect->y, pRect->w, pRect->h);
    return rc;
}


/*
 *
 * 3D interface.
 *
 */

static DECLCALLBACK(int) vmsvga3dBackQueryCaps(PVGASTATECC pThisCC, SVGA3dDevCapIndex idx3dCaps, uint32_t *pu32Val)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    int rc = VINF_SUCCESS;

    *pu32Val = 0;

    if (idx3dCaps > SVGA3D_DEVCAP_MAX)
    {
        LogRelMax(16, ("VMSVGA: unsupported SVGA3D_DEVCAP %d\n", idx3dCaps));
        return VERR_NOT_SUPPORTED;
    }

    D3D_FEATURE_LEVEL const FeatureLevel = pState->pBackend->dxDevice.FeatureLevel;

    /* Most values are taken from:
     * https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro
     *
     * Shader values are from
     * https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-models
     */

    switch (idx3dCaps)
    {
    case SVGA3D_DEVCAP_3D:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_MAX_LIGHTS:
        *pu32Val = SVGA3D_NUM_LIGHTS; /* VGPU9. Not applicable to DX11. */
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURES:
        *pu32Val = SVGA3D_NUM_TEXTURE_UNITS; /* VGPU9. Not applicable to DX11. */
        break;

    case SVGA3D_DEVCAP_MAX_CLIP_PLANES:
        *pu32Val = SVGA3D_NUM_CLIPPLANES;
        break;

    case SVGA3D_DEVCAP_VERTEX_SHADER_VERSION:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = SVGA3DVSVERSION_40;
        else
            *pu32Val = SVGA3DVSVERSION_30;
        break;

    case SVGA3D_DEVCAP_VERTEX_SHADER:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = SVGA3DPSVERSION_40;
        else
            *pu32Val = SVGA3DPSVERSION_30;
        break;

    case SVGA3D_DEVCAP_FRAGMENT_SHADER:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_MAX_RENDER_TARGETS:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 8;
        else
            *pu32Val = 4;
        break;

    case SVGA3D_DEVCAP_S23E8_TEXTURES:
    case SVGA3D_DEVCAP_S10E5_TEXTURES:
        /* Must be obsolete by now; surface format caps specify the same thing. */
        break;

    case SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND:
        /* Obsolete */
        break;

    /*
     *   2. The BUFFER_FORMAT capabilities are deprecated, and they always
     *      return TRUE. Even on physical hardware that does not support
     *      these formats natively, the SVGA3D device will provide an emulation
     *      which should be invisible to the guest OS.
     */
    case SVGA3D_DEVCAP_D16_BUFFER_FORMAT:
    case SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT:
    case SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_QUERY_TYPES:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_MAX_POINT_SIZE:
        AssertCompile(sizeof(uint32_t) == sizeof(float));
        *(float *)pu32Val = 256.0f;  /* VGPU9. Not applicable to DX11. */
        break;

    case SVGA3D_DEVCAP_MAX_SHADER_TEXTURES:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH:
    case SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0)
            *pu32Val = 16384;
        else if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 8192;
        else if (FeatureLevel >= D3D_FEATURE_LEVEL_9_3)
            *pu32Val = 4096;
        else
            *pu32Val = 2048;
        break;

    case SVGA3D_DEVCAP_MAX_VOLUME_EXTENT:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 2048;
        else
            *pu32Val = 256;
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0)
            *pu32Val = 16384;
        else if (FeatureLevel >= D3D_FEATURE_LEVEL_9_3)
            *pu32Val = 8192;
        else if (FeatureLevel >= D3D_FEATURE_LEVEL_9_2)
            *pu32Val = 2048;
        else
            *pu32Val = 128;
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_9_2)
            *pu32Val = D3D11_REQ_MAXANISOTROPY;
        else
            *pu32Val = 2; // D3D_FL9_1_DEFAULT_MAX_ANISOTROPY;
        break;

    case SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = UINT32_MAX;
        else if (FeatureLevel >= D3D_FEATURE_LEVEL_9_2)
            *pu32Val = 1048575; // D3D_FL9_2_IA_PRIMITIVE_MAX_COUNT;
        else
            *pu32Val = 65535; // D3D_FL9_1_IA_PRIMITIVE_MAX_COUNT;
        break;

    case SVGA3D_DEVCAP_MAX_VERTEX_INDEX:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = UINT32_MAX;
        else if (FeatureLevel >= D3D_FEATURE_LEVEL_9_2)
            *pu32Val = 1048575;
        else
            *pu32Val = 65534;
        break;

    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = UINT32_MAX;
        else
            *pu32Val = 512;
        break;

    case SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = UINT32_MAX;
        else
            *pu32Val = 512;
        break;

    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 4096;
        else
            *pu32Val = 32;
        break;

    case SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 4096;
        else
            *pu32Val = 32;
        break;

    case SVGA3D_DEVCAP_TEXTURE_OPS:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8:
    case SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8:
    case SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10:
    case SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5:
    case SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5:
    case SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4:
    case SVGA3D_DEVCAP_SURFACEFMT_R5G6B5:
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16:
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8:
    case SVGA3D_DEVCAP_SURFACEFMT_ALPHA8:
    case SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D16:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT1:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT2:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT3:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT4:
    case SVGA3D_DEVCAP_SURFACEFMT_DXT5:
    case SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8:
    case SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10:
    case SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8:
    case SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8:
    case SVGA3D_DEVCAP_SURFACEFMT_CxV8U8:
    case SVGA3D_DEVCAP_SURFACEFMT_R_S10E5:
    case SVGA3D_DEVCAP_SURFACEFMT_R_S23E8:
    case SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5:
    case SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8:
    case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5:
    case SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8:
    case SVGA3D_DEVCAP_SURFACEFMT_V16U16:
    case SVGA3D_DEVCAP_SURFACEFMT_G16R16:
    case SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16:
    case SVGA3D_DEVCAP_SURFACEFMT_UYVY:
    case SVGA3D_DEVCAP_SURFACEFMT_YUY2:
    case SVGA3D_DEVCAP_SURFACEFMT_NV12:
    case SVGA3D_DEVCAP_DEAD10: /* SVGA3D_DEVCAP_SURFACEFMT_AYUV */
    case SVGA3D_DEVCAP_SURFACEFMT_Z_DF16:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_DF24:
    case SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT:
    case SVGA3D_DEVCAP_SURFACEFMT_ATI1:
    case SVGA3D_DEVCAP_SURFACEFMT_ATI2:
    case SVGA3D_DEVCAP_SURFACEFMT_YV12:
    {
        SVGA3dSurfaceFormat const enmFormat = vmsvgaDXDevCapSurfaceFmt2Format(idx3dCaps);
        rc = vmsvgaDXCheckFormatSupportPreDX(pState, enmFormat, pu32Val);
        break;
    }

    case SVGA3D_DEVCAP_MISSING62:
        /* Unused */
        break;

    case SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS:
        if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
            *pu32Val = 8;
        else if (FeatureLevel >= D3D_FEATURE_LEVEL_9_3)
            *pu32Val = 4; // D3D_FL9_3_SIMULTANEOUS_RENDER_TARGET_COUNT
        else
            *pu32Val = 1; // D3D_FL9_1_SIMULTANEOUS_RENDER_TARGET_COUNT
        break;

    case SVGA3D_DEVCAP_DEAD4: /* SVGA3D_DEVCAP_MULTISAMPLE_NONMASKABLESAMPLES */
    case SVGA3D_DEVCAP_DEAD5: /* SVGA3D_DEVCAP_MULTISAMPLE_MASKABLESAMPLES */
        *pu32Val = (1 << (2-1)) | (1 << (4-1)) | (1 << (8-1)); /* 2x, 4x, 8x */
        break;

    case SVGA3D_DEVCAP_DEAD7: /* SVGA3D_DEVCAP_ALPHATOCOVERAGE */
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_DEAD6: /* SVGA3D_DEVCAP_SUPERSAMPLE */
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_AUTOGENMIPMAPS:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_MAX_CONTEXT_IDS:
        *pu32Val = SVGA3D_MAX_CONTEXT_IDS;
        break;

    case SVGA3D_DEVCAP_MAX_SURFACE_IDS:
        *pu32Val = SVGA3D_MAX_SURFACE_IDS;
        break;

    case SVGA3D_DEVCAP_DEAD1:
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_DEAD8: /* SVGA3D_DEVCAP_VIDEO_DECODE */
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_DEAD9: /* SVGA3D_DEVCAP_VIDEO_PROCESS */
        /* Obsolete */
        break;

    case SVGA3D_DEVCAP_LINE_AA:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_LINE_STIPPLE:
        *pu32Val = 0; /* DX11 does not seem to support this directly. */
        break;

    case SVGA3D_DEVCAP_MAX_LINE_WIDTH:
        AssertCompile(sizeof(uint32_t) == sizeof(float));
        *(float *)pu32Val = 1.0f;
        break;

    case SVGA3D_DEVCAP_MAX_AA_LINE_WIDTH:
        AssertCompile(sizeof(uint32_t) == sizeof(float));
        *(float *)pu32Val = 1.0f;
        break;

    case SVGA3D_DEVCAP_DEAD3: /* Old SVGA3D_DEVCAP_LOGICOPS */
        /* Deprecated. */
        AssertCompile(SVGA3D_DEVCAP_DEAD3 == 92); /* Newer SVGA headers redefine this. */
        break;

    case SVGA3D_DEVCAP_TS_COLOR_KEY:
        *pu32Val = 0; /* DX11 does not seem to support this directly. */
        break;

    case SVGA3D_DEVCAP_DEAD2:
        break;

    case SVGA3D_DEVCAP_DXCONTEXT:
        *pu32Val = 1;
        break;

    case SVGA3D_DEVCAP_DEAD11: /* SVGA3D_DEVCAP_MAX_TEXTURE_ARRAY_SIZE */
        *pu32Val = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        break;

    case SVGA3D_DEVCAP_DX_MAX_VERTEXBUFFERS:
        *pu32Val = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
        break;

    case SVGA3D_DEVCAP_DX_MAX_CONSTANT_BUFFERS:
        *pu32Val = D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT;
        break;

    case SVGA3D_DEVCAP_DX_PROVOKING_VERTEX:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_DXFMT_X8R8G8B8:
    case SVGA3D_DEVCAP_DXFMT_A8R8G8B8:
    case SVGA3D_DEVCAP_DXFMT_R5G6B5:
    case SVGA3D_DEVCAP_DXFMT_X1R5G5B5:
    case SVGA3D_DEVCAP_DXFMT_A1R5G5B5:
    case SVGA3D_DEVCAP_DXFMT_A4R4G4B4:
    case SVGA3D_DEVCAP_DXFMT_Z_D32:
    case SVGA3D_DEVCAP_DXFMT_Z_D16:
    case SVGA3D_DEVCAP_DXFMT_Z_D24S8:
    case SVGA3D_DEVCAP_DXFMT_Z_D15S1:
    case SVGA3D_DEVCAP_DXFMT_LUMINANCE8:
    case SVGA3D_DEVCAP_DXFMT_LUMINANCE4_ALPHA4:
    case SVGA3D_DEVCAP_DXFMT_LUMINANCE16:
    case SVGA3D_DEVCAP_DXFMT_LUMINANCE8_ALPHA8:
    case SVGA3D_DEVCAP_DXFMT_DXT1:
    case SVGA3D_DEVCAP_DXFMT_DXT2:
    case SVGA3D_DEVCAP_DXFMT_DXT3:
    case SVGA3D_DEVCAP_DXFMT_DXT4:
    case SVGA3D_DEVCAP_DXFMT_DXT5:
    case SVGA3D_DEVCAP_DXFMT_BUMPU8V8:
    case SVGA3D_DEVCAP_DXFMT_BUMPL6V5U5:
    case SVGA3D_DEVCAP_DXFMT_BUMPX8L8V8U8:
    case SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD1:
    case SVGA3D_DEVCAP_DXFMT_ARGB_S10E5:
    case SVGA3D_DEVCAP_DXFMT_ARGB_S23E8:
    case SVGA3D_DEVCAP_DXFMT_A2R10G10B10:
    case SVGA3D_DEVCAP_DXFMT_V8U8:
    case SVGA3D_DEVCAP_DXFMT_Q8W8V8U8:
    case SVGA3D_DEVCAP_DXFMT_CxV8U8:
    case SVGA3D_DEVCAP_DXFMT_X8L8V8U8:
    case SVGA3D_DEVCAP_DXFMT_A2W10V10U10:
    case SVGA3D_DEVCAP_DXFMT_ALPHA8:
    case SVGA3D_DEVCAP_DXFMT_R_S10E5:
    case SVGA3D_DEVCAP_DXFMT_R_S23E8:
    case SVGA3D_DEVCAP_DXFMT_RG_S10E5:
    case SVGA3D_DEVCAP_DXFMT_RG_S23E8:
    case SVGA3D_DEVCAP_DXFMT_BUFFER:
    case SVGA3D_DEVCAP_DXFMT_Z_D24X8:
    case SVGA3D_DEVCAP_DXFMT_V16U16:
    case SVGA3D_DEVCAP_DXFMT_G16R16:
    case SVGA3D_DEVCAP_DXFMT_A16B16G16R16:
    case SVGA3D_DEVCAP_DXFMT_UYVY:
    case SVGA3D_DEVCAP_DXFMT_YUY2:
    case SVGA3D_DEVCAP_DXFMT_NV12:
    case SVGA3D_DEVCAP_DXFMT_FORMAT_DEAD2: /* SVGA3D_DEVCAP_DXFMT_AYUV */
    case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_SINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32_SINT:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UINT:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_SINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R32G32_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32G32_SINT:
    case SVGA3D_DEVCAP_DXFMT_R32G8X24_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_D32_FLOAT_S8X24_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32_FLOAT_X8X24:
    case SVGA3D_DEVCAP_DXFMT_X32_G8X24_UINT:
    case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UINT:
    case SVGA3D_DEVCAP_DXFMT_R11G11B10_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SINT:
    case SVGA3D_DEVCAP_DXFMT_R16G16_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R16G16_UINT:
    case SVGA3D_DEVCAP_DXFMT_R16G16_SINT:
    case SVGA3D_DEVCAP_DXFMT_R32_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_D32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R32_UINT:
    case SVGA3D_DEVCAP_DXFMT_R32_SINT:
    case SVGA3D_DEVCAP_DXFMT_R24G8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_D24_UNORM_S8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R24_UNORM_X8:
    case SVGA3D_DEVCAP_DXFMT_X24_G8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R8G8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R8G8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R8G8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R8G8_SINT:
    case SVGA3D_DEVCAP_DXFMT_R16_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R16_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R16_UINT:
    case SVGA3D_DEVCAP_DXFMT_R16_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R16_SINT:
    case SVGA3D_DEVCAP_DXFMT_R8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_R8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R8_UINT:
    case SVGA3D_DEVCAP_DXFMT_R8_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R8_SINT:
    case SVGA3D_DEVCAP_DXFMT_P8:
    case SVGA3D_DEVCAP_DXFMT_R9G9B9E5_SHAREDEXP:
    case SVGA3D_DEVCAP_DXFMT_R8G8_B8G8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_G8R8_G8B8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC1_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC1_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_BC2_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC2_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_BC3_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC3_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_BC4_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_ATI1:
    case SVGA3D_DEVCAP_DXFMT_BC4_SNORM:
    case SVGA3D_DEVCAP_DXFMT_BC5_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_ATI2:
    case SVGA3D_DEVCAP_DXFMT_BC5_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R10G10B10_XR_BIAS_A2_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM_SRGB:
    case SVGA3D_DEVCAP_DXFMT_Z_DF16:
    case SVGA3D_DEVCAP_DXFMT_Z_DF24:
    case SVGA3D_DEVCAP_DXFMT_Z_D24S8_INT:
    case SVGA3D_DEVCAP_DXFMT_YV12:
    case SVGA3D_DEVCAP_DXFMT_R32G32B32A32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R16G16B16A16_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R32G32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R10G10B10A2_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R8G8B8A8_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R16G16_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R16G16_UNORM:
    case SVGA3D_DEVCAP_DXFMT_R16G16_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R32_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_R8G8_SNORM:
    case SVGA3D_DEVCAP_DXFMT_R16_FLOAT:
    case SVGA3D_DEVCAP_DXFMT_D16_UNORM:
    case SVGA3D_DEVCAP_DXFMT_A8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC1_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC2_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC3_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B5G6R5_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B5G5R5A1_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8A8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_B8G8R8X8_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC4_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC5_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC6H_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC6H_UF16:
    case SVGA3D_DEVCAP_DXFMT_BC6H_SF16:
    case SVGA3D_DEVCAP_DXFMT_BC7_TYPELESS:
    case SVGA3D_DEVCAP_DXFMT_BC7_UNORM:
    case SVGA3D_DEVCAP_DXFMT_BC7_UNORM_SRGB:
    {
        SVGA3dSurfaceFormat const enmFormat = vmsvgaDXDevCapDxfmt2Format(idx3dCaps);
        rc = vmsvgaDXCheckFormatSupport(pState, enmFormat, pu32Val);
        break;
    }

    case SVGA3D_DEVCAP_SM41:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MULTISAMPLE_2X:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MULTISAMPLE_4X:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MS_FULL_QUALITY:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_LOGICOPS:
        AssertCompile(SVGA3D_DEVCAP_LOGICOPS == 248);
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_LOGIC_BLENDOPS:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_RESERVED_1:
        break;

    case SVGA3D_DEVCAP_RESERVED_2:
        break;

    case SVGA3D_DEVCAP_SM5:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MULTISAMPLE_8X:
        *pu32Val = 0; /* boolean */
        break;

    case SVGA3D_DEVCAP_MAX:
    case SVGA3D_DEVCAP_INVALID:
        rc = VERR_NOT_SUPPORTED;
        break;
    }

    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackChangeMode(PVGASTATECC pThisCC)
{
    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSurfaceCopy(PVGASTATECC pThisCC, SVGA3dSurfaceImageId dest, SVGA3dSurfaceImageId src,
                               uint32_t cCopyBoxes, SVGA3dCopyBox *pBox)
{
    RT_NOREF(cCopyBoxes, pBox);

    LogFunc(("src sid %d -> dst sid %d\n", src.sid, dest.sid));

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    PVMSVGA3DBACKEND pBackend = pState->pBackend;

    PVMSVGA3DSURFACE pSrcSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, src.sid, &pSrcSurface);
    AssertRCReturn(rc, rc);

    PVMSVGA3DSURFACE pDstSurface;
    rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, dest.sid, &pDstSurface);
    AssertRCReturn(rc, rc);

    LogFunc(("src%s cid %d -> dst%s cid %d\n",
             pSrcSurface->pBackendSurface ? "" : " sysmem",
             pSrcSurface ? pSrcSurface->idAssociatedContext : SVGA_ID_INVALID,
             pDstSurface->pBackendSurface ? "" : " sysmem",
             pDstSurface ? pDstSurface->idAssociatedContext : SVGA_ID_INVALID));

    //DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    //AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    if (pSrcSurface->pBackendSurface)
    {
        if (pDstSurface->pBackendSurface == NULL)
        {
            /* Create the target if it can be used as a device context shared resource (render or screen target). */
            if (pBackend->fSingleDevice || dxIsSurfaceShareable(pDstSurface))
            {
                rc = vmsvga3dBackSurfaceCreateTexture(pThisCC, NULL, pDstSurface);
                AssertRCReturn(rc, rc);
            }
        }

        if (pDstSurface->pBackendSurface)
        {
            /* Surface -> Surface. */
            /* Expect both of them to be shared surfaces created by the backend context. */
            Assert(pSrcSurface->idAssociatedContext == DX_CID_BACKEND && pDstSurface->idAssociatedContext == DX_CID_BACKEND);

            /* Wait for the source surface to finish drawing. */
            dxSurfaceWait(pState, pSrcSurface, DX_CID_BACKEND);

            DXDEVICE *pDXDevice = &pBackend->dxDevice;

            /* Clip the box. */
            PVMSVGA3DMIPMAPLEVEL pSrcMipLevel;
            rc = vmsvga3dMipmapLevel(pSrcSurface, src.face, src.mipmap, &pSrcMipLevel);
            ASSERT_GUEST_RETURN(RT_SUCCESS(rc), rc);

            PVMSVGA3DMIPMAPLEVEL pDstMipLevel;
            rc = vmsvga3dMipmapLevel(pDstSurface, dest.face, dest.mipmap, &pDstMipLevel);
            ASSERT_GUEST_RETURN(RT_SUCCESS(rc), rc);

            SVGA3dCopyBox clipBox = *pBox;
            vmsvgaR3ClipCopyBox(&pSrcMipLevel->mipmapSize, &pDstMipLevel->mipmapSize, &clipBox);

            UINT DstSubresource = vmsvga3dCalcSubresource(dest.mipmap, dest.face, pDstSurface->cLevels);
            UINT DstX = clipBox.x;
            UINT DstY = clipBox.y;
            UINT DstZ = clipBox.z;

            UINT SrcSubresource = vmsvga3dCalcSubresource(src.mipmap, src.face, pSrcSurface->cLevels);
            D3D11_BOX SrcBox;
            SrcBox.left   = clipBox.srcx;
            SrcBox.top    = clipBox.srcy;
            SrcBox.front  = clipBox.srcz;
            SrcBox.right  = clipBox.srcx + clipBox.w;
            SrcBox.bottom = clipBox.srcy + clipBox.h;
            SrcBox.back   = clipBox.srcz + clipBox.d;

            Assert(cCopyBoxes == 1); /** @todo */

            ID3D11Resource *pDstResource;
            ID3D11Resource *pSrcResource;
            pDstResource = dxResource(pState, pDstSurface, NULL);
            pSrcResource = dxResource(pState, pSrcSurface, NULL);

            pDXDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                                pSrcResource, SrcSubresource, &SrcBox);

            pDstSurface->pBackendSurface->cidDrawing = DX_CID_BACKEND;
        }
        else
        {
            /* Surface -> Memory. */
            AssertFailed(); /** @todo implement */
        }
    }
    else
    {
        /* Memory -> Surface. */
        AssertFailed(); /** @todo implement */
    }

    return rc;
}


static DECLCALLBACK(void) vmsvga3dBackUpdateHostScreenViewport(PVGASTATECC pThisCC, uint32_t idScreen, VMSVGAVIEWPORT const *pOldViewport)
{
    RT_NOREF(pThisCC, idScreen, pOldViewport);
    /** @todo Scroll the screen content without requiring the guest to redraw. */
}


static DECLCALLBACK(int) vmsvga3dBackSurfaceUpdateHeapBuffers(PVGASTATECC pThisCC, PVMSVGA3DSURFACE pSurface)
{
    /** @todo */
    RT_NOREF(pThisCC, pSurface);
    return VERR_NOT_IMPLEMENTED;
}


#if 0 /*unused*/
/**
 * Create a new 3d context
 *
 * @returns VBox status code.
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   cid             Context id
 */
static DECLCALLBACK(int) vmsvga3dBackContextDefine(PVGASTATECC pThisCC, uint32_t cid)
{
    RT_NOREF(cid);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Destroy an existing 3d context
 *
 * @returns VBox status code.
 * @param   pThisCC         The VGA/VMSVGA state for ring-3.
 * @param   cid             Context id
 */
static DECLCALLBACK(int) vmsvga3dBackContextDestroy(PVGASTATECC pThisCC, uint32_t cid)
{
    RT_NOREF(cid);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetTransform(PVGASTATECC pThisCC, uint32_t cid, SVGA3dTransformType type, float matrix[16])
{
    RT_NOREF(cid, type, matrix);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetZRange(PVGASTATECC pThisCC, uint32_t cid, SVGA3dZRange zRange)
{
    RT_NOREF(cid, zRange);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetRenderState(PVGASTATECC pThisCC, uint32_t cid, uint32_t cRenderStates, SVGA3dRenderState *pRenderState)
{
    RT_NOREF(cid, cRenderStates, pRenderState);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetRenderTarget(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRenderTargetType type, SVGA3dSurfaceImageId target)
{
    RT_NOREF(cid, type, target);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetTextureState(PVGASTATECC pThisCC, uint32_t cid, uint32_t cTextureStates, SVGA3dTextureState *pTextureState)
{
    RT_NOREF(cid, cTextureStates, pTextureState);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetMaterial(PVGASTATECC pThisCC, uint32_t cid, SVGA3dFace face, SVGA3dMaterial *pMaterial)
{
    RT_NOREF(cid, face, pMaterial);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetLightData(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, SVGA3dLightData *pData)
{
    RT_NOREF(cid, index, pData);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetLightEnabled(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, uint32_t enabled)
{
    RT_NOREF(cid, index, enabled);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetViewPort(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRect *pRect)
{
    RT_NOREF(cid, pRect);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetClipPlane(PVGASTATECC pThisCC, uint32_t cid, uint32_t index, float plane[4])
{
    RT_NOREF(cid, index, plane);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackCommandClear(PVGASTATECC pThisCC, uint32_t cid, SVGA3dClearFlag clearFlag, uint32_t color, float depth,
                                    uint32_t stencil, uint32_t cRects, SVGA3dRect *pRect)
{
    /* From SVGA3D_BeginClear comments:
     *
     *      Clear is not affected by clipping, depth test, or other
     *      render state which affects the fragment pipeline.
     *
     * Therefore this code must ignore the current scissor rect.
     */

    RT_NOREF(cid, clearFlag, color, depth, stencil, cRects, pRect);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDrawPrimitives(PVGASTATECC pThisCC, uint32_t cid, uint32_t numVertexDecls, SVGA3dVertexDecl *pVertexDecl,
                                  uint32_t numRanges, SVGA3dPrimitiveRange *pRange,
                                  uint32_t cVertexDivisor, SVGA3dVertexDivisor *pVertexDivisor)
{
    RT_NOREF(cid, numVertexDecls, pVertexDecl, numRanges, pRange, cVertexDivisor, pVertexDivisor);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackSetScissorRect(PVGASTATECC pThisCC, uint32_t cid, SVGA3dRect *pRect)
{
    RT_NOREF(cid, pRect);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackGenerateMipmaps(PVGASTATECC pThisCC, uint32_t sid, SVGA3dTextureFilter filter)
{
    RT_NOREF(sid, filter);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackShaderDefine(PVGASTATECC pThisCC, uint32_t cid, uint32_t shid, SVGA3dShaderType type,
                                uint32_t cbData, uint32_t *pShaderData)
{
    RT_NOREF(cid, shid, type, cbData, pShaderData);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackShaderDestroy(PVGASTATECC pThisCC, uint32_t cid, uint32_t shid, SVGA3dShaderType type)
{
    RT_NOREF(cid, shid, type);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackShaderSet(PVGASTATECC pThisCC, PVMSVGA3DCONTEXT pContext, uint32_t cid, SVGA3dShaderType type, uint32_t shid)
{
    RT_NOREF(pContext, cid, type, shid);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackShaderSetConst(PVGASTATECC pThisCC, uint32_t cid, uint32_t reg, SVGA3dShaderType type,
                                  SVGA3dShaderConstType ctype, uint32_t cRegisters, uint32_t *pValues)
{
    RT_NOREF(cid, reg, type, ctype, cRegisters, pValues);

    PVMSVGA3DSTATE pState = pThisCC->svga.p3dState;
    AssertReturn(pState, VERR_INVALID_STATE);

    AssertFailed();
    return VINF_SUCCESS;
}
#endif


/**
 * Destroy backend specific surface bits (part of SVGA_3D_CMD_SURFACE_DESTROY).
 *
 * @param   pThisCC             The device context.
 * @param   pSurface            The surface being destroyed.
 */
static DECLCALLBACK(void) vmsvga3dBackSurfaceDestroy(PVGASTATECC pThisCC, PVMSVGA3DSURFACE pSurface)
{
    RT_NOREF(pThisCC);

    /* The caller should not use the function for system memory surfaces. */
    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    if (!pBackendSurface)
        return;
    pSurface->pBackendSurface = NULL;

    LogFunc(("sid=%u\n", pSurface->id));

    /* If any views have been created for this resource, then also release them. */
    DXVIEW *pIter, *pNext;
    RTListForEachSafe(&pBackendSurface->u2.Texture.listView, pIter, pNext, DXVIEW, nodeSurfaceView)
    {
        LogFunc(("pIter=%p, pNext=%p\n", pIter, pNext));
        dxViewDestroy(pIter);
    }

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        D3D_RELEASE(pBackendSurface->pStagingTexture);
        D3D_RELEASE(pBackendSurface->pDynamicTexture);
        D3D_RELEASE(pBackendSurface->u.pTexture2D);
    }
    else if (   pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_2D
             || pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_CUBE)
    {
        D3D_RELEASE(pBackendSurface->pStagingTexture);
        D3D_RELEASE(pBackendSurface->pDynamicTexture);
        D3D_RELEASE(pBackendSurface->u.pTexture2D);
    }
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
    {
        D3D_RELEASE(pBackendSurface->pStagingTexture3D);
        D3D_RELEASE(pBackendSurface->pDynamicTexture3D);
        D3D_RELEASE(pBackendSurface->u.pTexture3D);
    }
    else if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_BUFFER)
    {
        D3D_RELEASE(pBackendSurface->u.pBuffer);
    }
    else
    {
        AssertFailed();
    }

    RTMemFree(pBackendSurface);

    /* No context has created the surface, because the surface does not exist anymore. */
    pSurface->idAssociatedContext = SVGA_ID_INVALID;
}


static DECLCALLBACK(void) vmsvga3dBackSurfaceInvalidateImage(PVGASTATECC pThisCC, PVMSVGA3DSURFACE pSurface, uint32_t uFace, uint32_t uMipmap)
{
    RT_NOREF(pThisCC, uFace, uMipmap);

    /* The caller should not use the function for system memory surfaces. */
    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    if (!pBackendSurface)
        return;

    LogFunc(("sid=%u\n", pSurface->id));

    /* The guest uses this to invalidate a buffer. */
    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_BUFFER)
    {
        Assert(uFace == 0 && uMipmap == 0); /* The caller ensures this. */
        /** @todo This causes flickering when a buffer is invalidated and re-created right before a draw call. */
        //vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }
    else
    {
        /** @todo Delete views that have been created for this mipmap.
         * For now just delete all views, they will be recte=reated if necessary.
         */
        ASSERT_GUEST_FAILED();
        DXVIEW *pIter, *pNext;
        RTListForEachSafe(&pBackendSurface->u2.Texture.listView, pIter, pNext, DXVIEW, nodeSurfaceView)
        {
            dxViewDestroy(pIter);
        }
    }
}


/**
 * Backend worker for implementing SVGA_3D_CMD_SURFACE_STRETCHBLT.
 *
 * @returns VBox status code.
 * @param   pThis               The VGA device instance.
 * @param   pState              The VMSVGA3d state.
 * @param   pDstSurface         The destination host surface.
 * @param   uDstFace            The destination face (valid).
 * @param   uDstMipmap          The destination mipmap level (valid).
 * @param   pDstBox             The destination box.
 * @param   pSrcSurface         The source host surface.
 * @param   uSrcFace            The destination face (valid).
 * @param   uSrcMipmap          The source mimap level (valid).
 * @param   pSrcBox             The source box.
 * @param   enmMode             The strecht blt mode .
 * @param   pContext            The VMSVGA3d context (already current for OGL).
 */
static DECLCALLBACK(int) vmsvga3dBackSurfaceStretchBlt(PVGASTATE pThis, PVMSVGA3DSTATE pState,
                                  PVMSVGA3DSURFACE pDstSurface, uint32_t uDstFace, uint32_t uDstMipmap, SVGA3dBox const *pDstBox,
                                  PVMSVGA3DSURFACE pSrcSurface, uint32_t uSrcFace, uint32_t uSrcMipmap, SVGA3dBox const *pSrcBox,
                                  SVGA3dStretchBltMode enmMode, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pThis, pState, pDstSurface, uDstFace, uDstMipmap, pDstBox,
             pSrcSurface, uSrcFace, uSrcMipmap, pSrcBox, enmMode, pContext);

    AssertFailed();
    return VINF_SUCCESS;
}


/**
 * Backend worker for implementing SVGA_3D_CMD_SURFACE_DMA that copies one box.
 *
 * @returns Failure status code or @a rc.
 * @param   pThis               The shared VGA/VMSVGA instance data.
 * @param   pThisCC             The VGA/VMSVGA state for ring-3.
 * @param   pState              The VMSVGA3d state.
 * @param   pSurface            The host surface.
 * @param   pMipLevel           Mipmap level. The caller knows it already.
 * @param   uHostFace           The host face (valid).
 * @param   uHostMipmap         The host mipmap level (valid).
 * @param   GuestPtr            The guest pointer.
 * @param   cbGuestPitch        The guest pitch.
 * @param   transfer            The transfer direction.
 * @param   pBox                The box to copy (clipped, valid, except for guest's srcx, srcy, srcz).
 * @param   pContext            The context (for OpenGL).
 * @param   rc                  The current rc for all boxes.
 * @param   iBox                The current box number (for Direct 3D).
 */
static DECLCALLBACK(int) vmsvga3dBackSurfaceDMACopyBox(PVGASTATE pThis, PVGASTATECC pThisCC, PVMSVGA3DSTATE pState, PVMSVGA3DSURFACE pSurface,
                                  PVMSVGA3DMIPMAPLEVEL pMipLevel, uint32_t uHostFace, uint32_t uHostMipmap,
                                  SVGAGuestPtr GuestPtr, uint32_t cbGuestPitch, SVGA3dTransferType transfer,
                                  SVGA3dCopyBox const *pBox, PVMSVGA3DCONTEXT pContext, int rc, int iBox)
{
    RT_NOREF(pState, pMipLevel, pContext, iBox);

    /* The called should not use the function for system memory surfaces. */
    PVMSVGA3DBACKENDSURFACE pBackendSurface = pSurface->pBackendSurface;
    AssertReturn(pBackendSurface, VERR_INVALID_PARAMETER);

    if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_SCREEN_TARGET)
    {
        /** @todo This is generic code and should be in DevVGA-SVGA3d.cpp for backends which support Map/Unmap. */
        AssertReturn(uHostFace == 0 && uHostMipmap == 0, VERR_INVALID_PARAMETER);

        uint32_t const u32GuestBlockX = pBox->srcx / pSurface->cxBlock;
        uint32_t const u32GuestBlockY = pBox->srcy / pSurface->cyBlock;
        Assert(u32GuestBlockX * pSurface->cxBlock == pBox->srcx);
        Assert(u32GuestBlockY * pSurface->cyBlock == pBox->srcy);
        uint32_t const cBlocksX = (pBox->w + pSurface->cxBlock - 1) / pSurface->cxBlock;
        uint32_t const cBlocksY = (pBox->h + pSurface->cyBlock - 1) / pSurface->cyBlock;
        AssertMsgReturn(cBlocksX && cBlocksY, ("Empty box %dx%d\n", pBox->w, pBox->h), VERR_INTERNAL_ERROR);

        /* vmsvgaR3GmrTransfer verifies uGuestOffset.
         * srcx(u32GuestBlockX) and srcy(u32GuestBlockY) have been verified in vmsvga3dSurfaceDMA
         * to not cause 32 bit overflow when multiplied by cbBlock and cbGuestPitch.
         */
        uint64_t const uGuestOffset = u32GuestBlockX * pSurface->cbBlock + u32GuestBlockY * cbGuestPitch;
        AssertReturn(uGuestOffset < UINT32_MAX, VERR_INVALID_PARAMETER);

        SVGA3dSurfaceImageId image;
        image.sid = pSurface->id;
        image.face = uHostFace;
        image.mipmap = uHostMipmap;

        SVGA3dBox box;
        box.x = pBox->x;
        box.y = pBox->y;
        box.z = 0;
        box.w = pBox->w;
        box.h = pBox->h;
        box.d = 1;

        VMSVGA3D_SURFACE_MAP const enmMap = transfer == SVGA3D_WRITE_HOST_VRAM
                                          ? VMSVGA3D_SURFACE_MAP_WRITE
                                          : VMSVGA3D_SURFACE_MAP_READ;

        VMSVGA3D_MAPPED_SURFACE map;
        rc = vmsvga3dBackSurfaceMap(pThisCC, &image, &box, enmMap, &map);
        if (RT_SUCCESS(rc))
        {
            /* Prepare parameters for vmsvgaR3GmrTransfer, which needs the host buffer address, size
             * and offset of the first scanline.
             */
            uint32_t const cbLockedBuf = map.cbRowPitch * cBlocksY;
            uint8_t *pu8LockedBuf = (uint8_t *)map.pvData;
            uint32_t const offLockedBuf = 0;

            rc = vmsvgaR3GmrTransfer(pThis,
                                     pThisCC,
                                     transfer,
                                     pu8LockedBuf,
                                     cbLockedBuf,
                                     offLockedBuf,
                                     map.cbRowPitch,
                                     GuestPtr,
                                     (uint32_t)uGuestOffset,
                                     cbGuestPitch,
                                     cBlocksX * pSurface->cbBlock,
                                     cBlocksY);
            AssertRC(rc);

            // Log4(("first line:\n%.*Rhxd\n", cBlocksX * pSurface->cbBlock, LockedRect.pBits));

            //vmsvga3dMapWriteBmpFile(&map, "Dynamic");

            vmsvga3dBackSurfaceUnmap(pThisCC, &image, &map, /* fWritten = */ true);
        }
#if 0
            //DEBUG_BREAKPOINT_TEST();
            rc = vmsvga3dBackSurfaceMap(pThisCC, &image, NULL, VMSVGA3D_SURFACE_MAP_READ, &map);
            if (RT_SUCCESS(rc))
            {
                vmsvga3dMapWriteBmpFile(&map, "Staging");

                vmsvga3dBackSurfaceUnmap(pThisCC, &image, &map, /* fWritten =  */ false);
            }
#endif
    }
    else if (   pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_2D
             || pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
    {
        /** @todo This is generic code and should be in DevVGA-SVGA3d.cpp for backends which support Map/Unmap. */
        uint32_t const u32GuestBlockX = pBox->srcx / pSurface->cxBlock;
        uint32_t const u32GuestBlockY = pBox->srcy / pSurface->cyBlock;
        Assert(u32GuestBlockX * pSurface->cxBlock == pBox->srcx);
        Assert(u32GuestBlockY * pSurface->cyBlock == pBox->srcy);
        uint32_t const cBlocksX = (pBox->w + pSurface->cxBlock - 1) / pSurface->cxBlock;
        uint32_t const cBlocksY = (pBox->h + pSurface->cyBlock - 1) / pSurface->cyBlock;
        AssertMsgReturn(cBlocksX && cBlocksY && pBox->d, ("Empty box %dx%dx%d\n", pBox->w, pBox->h, pBox->d), VERR_INTERNAL_ERROR);

        /* vmsvgaR3GmrTransfer verifies uGuestOffset.
         * srcx(u32GuestBlockX) and srcy(u32GuestBlockY) have been verified in vmsvga3dSurfaceDMA
         * to not cause 32 bit overflow when multiplied by cbBlock and cbGuestPitch.
         */
        uint64_t uGuestOffset = u32GuestBlockX * pSurface->cbBlock + u32GuestBlockY * cbGuestPitch;
        AssertReturn(uGuestOffset < UINT32_MAX, VERR_INVALID_PARAMETER);

        /* 3D texture needs additional processing. */
        ASSERT_GUEST_RETURN(   pBox->z < D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION
                            && pBox->d <= D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION
                            && pBox->d <= D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION - pBox->z,
                            VERR_INVALID_PARAMETER);
        ASSERT_GUEST_RETURN(   pBox->srcz < D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION
                            && pBox->d <= D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION
                            && pBox->d <= D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION - pBox->srcz,
                            VERR_INVALID_PARAMETER);

        uGuestOffset += pBox->srcz * pMipLevel->cbSurfacePlane;

        SVGA3dSurfaceImageId image;
        image.sid = pSurface->id;
        image.face = uHostFace;
        image.mipmap = uHostMipmap;

        SVGA3dBox box;
        box.x = pBox->x;
        box.y = pBox->y;
        box.z = pBox->z;
        box.w = pBox->w;
        box.h = pBox->h;
        box.d = pBox->d;

        VMSVGA3D_SURFACE_MAP const enmMap = transfer == SVGA3D_WRITE_HOST_VRAM
                                          ? VMSVGA3D_SURFACE_MAP_WRITE
                                          : VMSVGA3D_SURFACE_MAP_READ;

        VMSVGA3D_MAPPED_SURFACE map;
        rc = vmsvga3dBackSurfaceMap(pThisCC, &image, &box, enmMap, &map);
        if (RT_SUCCESS(rc))
        {
#if 0
            if (box.w == 250 && box.h == 250 && box.d == 1 && enmMap == VMSVGA3D_SURFACE_MAP_READ)
            {
                DEBUG_BREAKPOINT_TEST();
                vmsvga3dMapWriteBmpFile(&map, "P");
            }
#endif
            /* Prepare parameters for vmsvgaR3GmrTransfer, which needs the host buffer address, size
             * and offset of the first scanline.
             */
            uint32_t cbLockedBuf = map.cbRowPitch * cBlocksY;
            if (pBackendSurface->enmResType == VMSVGA3D_RESTYPE_TEXTURE_3D)
                cbLockedBuf += map.cbDepthPitch * (pBox->d - 1); /// @todo why map does not compute this for 2D textures
            uint8_t *pu8LockedBuf = (uint8_t *)map.pvData;
            uint32_t offLockedBuf = 0;

            for (uint32_t iPlane = 0; iPlane < pBox->d; ++iPlane)
            {
                AssertBreak(uGuestOffset < UINT32_MAX);

                rc = vmsvgaR3GmrTransfer(pThis,
                                         pThisCC,
                                         transfer,
                                         pu8LockedBuf,
                                         cbLockedBuf,
                                         offLockedBuf,
                                         map.cbRowPitch,
                                         GuestPtr,
                                         (uint32_t)uGuestOffset,
                                         cbGuestPitch,
                                         cBlocksX * pSurface->cbBlock,
                                         cBlocksY);
                AssertRC(rc);

                uGuestOffset += pMipLevel->cbSurfacePlane;
                offLockedBuf += map.cbDepthPitch;
            }

            bool const fWritten = (transfer == SVGA3D_WRITE_HOST_VRAM);
            vmsvga3dBackSurfaceUnmap(pThisCC, &image, &map, fWritten);
        }
    }
    else
    {
        AssertMsgFailed(("Unsupported surface type %d\n", pBackendSurface->enmResType));
        rc = VERR_NOT_IMPLEMENTED;
    }

    return rc;
}


/**
 * Create D3D/OpenGL texture object for the specified surface.
 *
 * Surfaces are created when needed.
 *
 * @param   pThisCC             The device context.
 * @param   pContext            The context.
 * @param   idAssociatedContext Probably the same as pContext->id.
 * @param   pSurface            The surface to create the texture for.
 */
static DECLCALLBACK(int) vmsvga3dBackCreateTexture(PVGASTATECC pThisCC, PVMSVGA3DCONTEXT pContext, uint32_t idAssociatedContext,
                                     PVMSVGA3DSURFACE pSurface)

{
    RT_NOREF(pThisCC, pContext, idAssociatedContext, pSurface);

    AssertFailed();
    return VINF_SUCCESS;
}


#if 0 /*unused*/
static DECLCALLBACK(int) vmsvga3dBackOcclusionQueryCreate(PVGASTATECC pThisCC, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pThisCC, pContext);
    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackOcclusionQueryBegin(PVGASTATECC pThisCC, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pThisCC, pContext);
    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackOcclusionQueryEnd(PVGASTATECC pThisCC, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pThisCC, pContext);
    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackOcclusionQueryGetData(PVGASTATECC pThisCC, PVMSVGA3DCONTEXT pContext, uint32_t *pu32Pixels)
{
    RT_NOREF(pThisCC, pContext);
    *pu32Pixels = 0;
    AssertFailed();
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackOcclusionQueryDelete(PVGASTATECC pThisCC, PVMSVGA3DCONTEXT pContext)
{
    RT_NOREF(pThisCC, pContext);
    AssertFailed();
    return VINF_SUCCESS;
}
#endif


/*
 * DX callbacks.
 */

static DECLCALLBACK(int) vmsvga3dBackDXDefineContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    /* Allocate a backend specific context structure. */
    PVMSVGA3DBACKENDDXCONTEXT pBackendDXContext = (PVMSVGA3DBACKENDDXCONTEXT)RTMemAllocZ(sizeof(VMSVGA3DBACKENDDXCONTEXT));
    AssertPtrReturn(pBackendDXContext, VERR_NO_MEMORY);
    pDXContext->pBackendDXContext = pBackendDXContext;

    LogFunc(("cid %d\n", pDXContext->cid));

    int rc = dxDeviceCreate(pBackend, &pBackendDXContext->dxDevice);
    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    LogFunc(("cid %d\n", pDXContext->cid));

    if (pDXContext->pBackendDXContext)
    {
        /* Clean up context resources. */
        VMSVGA3DBACKENDDXCONTEXT *pBackendDXContext = pDXContext->pBackendDXContext;

        if (pBackendDXContext->paRenderTargetView)
        {
            for (uint32_t i = 0; i < pBackendDXContext->cRenderTargetView; ++i)
                D3D_RELEASE(pBackendDXContext->paRenderTargetView[i].u.pRenderTargetView);
        }
        if (pBackendDXContext->paDepthStencilView)
        {
            for (uint32_t i = 0; i < pBackendDXContext->cDepthStencilView; ++i)
                D3D_RELEASE(pBackendDXContext->paDepthStencilView[i].u.pDepthStencilView);
        }
        if (pBackendDXContext->paShaderResourceView)
        {
            for (uint32_t i = 0; i < pBackendDXContext->cShaderResourceView; ++i)
                D3D_RELEASE(pBackendDXContext->paShaderResourceView[i].u.pShaderResourceView);
        }
        if (pBackendDXContext->paElementLayout)
        {
            for (uint32_t i = 0; i < pBackendDXContext->cElementLayout; ++i)
                D3D_RELEASE(pBackendDXContext->paElementLayout[i].pElementLayout);
        }
        if (pBackendDXContext->papBlendState)
            DX_RELEASE_ARRAY(pBackendDXContext->cBlendState, pBackendDXContext->papBlendState);
        if (pBackendDXContext->papDepthStencilState)
            DX_RELEASE_ARRAY(pBackendDXContext->cDepthStencilState, pBackendDXContext->papDepthStencilState);
        if (pBackendDXContext->papRasterizerState)
            DX_RELEASE_ARRAY(pBackendDXContext->cRasterizerState, pBackendDXContext->papRasterizerState);
        if (pBackendDXContext->papSamplerState)
            DX_RELEASE_ARRAY(pBackendDXContext->cSamplerState, pBackendDXContext->papSamplerState);
        if (pBackendDXContext->papQuery)
            DX_RELEASE_ARRAY(pBackendDXContext->cQuery, pBackendDXContext->papQuery);
        if (pBackendDXContext->paShader)
        {
            for (uint32_t i = 0; i < pBackendDXContext->cShader; ++i)
                dxDestroyShader(&pBackendDXContext->paShader[i]);
        }
        if (pBackendDXContext->paStreamOutput)
        {
            for (uint32_t i = 0; i < pBackendDXContext->cStreamOutput; ++i)
                dxDestroyStreamOutput(&pBackendDXContext->paStreamOutput[i]);
        }

        RTMemFreeZ(pBackendDXContext->papBlendState, sizeof(pBackendDXContext->papBlendState[0]) * pBackendDXContext->cBlendState);
        RTMemFreeZ(pBackendDXContext->papDepthStencilState, sizeof(pBackendDXContext->papDepthStencilState[0]) * pBackendDXContext->cDepthStencilState);
        RTMemFreeZ(pBackendDXContext->papSamplerState, sizeof(pBackendDXContext->papSamplerState[0]) * pBackendDXContext->cSamplerState);
        RTMemFreeZ(pBackendDXContext->papRasterizerState, sizeof(pBackendDXContext->papRasterizerState[0]) * pBackendDXContext->cRasterizerState);
        RTMemFreeZ(pBackendDXContext->paElementLayout, sizeof(pBackendDXContext->paElementLayout[0]) * pBackendDXContext->cElementLayout);
        RTMemFreeZ(pBackendDXContext->paRenderTargetView, sizeof(pBackendDXContext->paRenderTargetView[0]) * pBackendDXContext->cRenderTargetView);
        RTMemFreeZ(pBackendDXContext->paDepthStencilView, sizeof(pBackendDXContext->paDepthStencilView[0]) * pBackendDXContext->cDepthStencilView);
        RTMemFreeZ(pBackendDXContext->paShaderResourceView, sizeof(pBackendDXContext->paShaderResourceView[0]) * pBackendDXContext->cShaderResourceView);
        RTMemFreeZ(pBackendDXContext->papQuery, sizeof(pBackendDXContext->papQuery[0]) * pBackendDXContext->cQuery);
        RTMemFreeZ(pBackendDXContext->paShader, sizeof(pBackendDXContext->paShader[0]) * pBackendDXContext->cShader);
        RTMemFreeZ(pBackendDXContext->paStreamOutput, sizeof(pBackendDXContext->paStreamOutput[0]) * pBackendDXContext->cStreamOutput);

        /* Destroy backend surfaces which belong to this context. */
        /** @todo The context should have a list of surfaces (and also shared resources). */
        for (uint32_t sid = 0; sid < pThisCC->svga.p3dState->cSurfaces; ++sid)
        {
            PVMSVGA3DSURFACE const pSurface = pThisCC->svga.p3dState->papSurfaces[sid];
            if (   pSurface
                && pSurface->id == sid)
            {
                if (pSurface->idAssociatedContext == pDXContext->cid)
                {
                    if (pSurface->pBackendSurface)
                        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
                }
                else if (pSurface->idAssociatedContext == DX_CID_BACKEND)
                {
                    /* May have shared resources in this context. */
                    if (pSurface->pBackendSurface)
                    {
                        DXSHAREDTEXTURE *pSharedTexture = (DXSHAREDTEXTURE *)RTAvlU32Get(&pSurface->pBackendSurface->SharedTextureTree, pDXContext->cid);
                        if (pSharedTexture)
                        {
                            Assert(pSharedTexture->sid == sid);
                            RTAvlU32Remove(&pSurface->pBackendSurface->SharedTextureTree, pDXContext->cid);
                            D3D_RELEASE(pSharedTexture->pTexture);
                            RTMemFreeZ(pSharedTexture, sizeof(*pSharedTexture));
                        }
                    }
                }
            }
        }

        dxDeviceDestroy(pBackend, &pBackendDXContext->dxDevice);

        RTMemFreeZ(pBackendDXContext, sizeof(*pBackendDXContext));
        pDXContext->pBackendDXContext = NULL;
    }
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend, pDXContext);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSwitchContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    if (!pBackend->fSingleDevice)
        return VINF_NOT_IMPLEMENTED; /* Not required. */

    /* The new context state will be applied by the generic DX code. */
    RT_NOREF(pDXContext);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXReadbackContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend, pDXContext);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXInvalidateContext(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetSingleConstantBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t slot, SVGA3dShaderType type, SVGA3dSurfaceId sid, uint32_t offsetInBytes, uint32_t sizeInBytes)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    if (sid == SVGA_ID_INVALID)
    {
        dxConstantBufferSet(pDevice, slot, type, NULL);
        return VINF_SUCCESS;
    }

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, sid, &pSurface);
    AssertRCReturn(rc, rc);

    PVMSVGA3DMIPMAPLEVEL pMipLevel;
    rc = vmsvga3dMipmapLevel(pSurface, 0, 0, &pMipLevel);
    AssertRCReturn(rc, rc);

    uint32_t const cbSurface = pMipLevel->cbSurface;
    ASSERT_GUEST_RETURN(   offsetInBytes < cbSurface
                        && sizeInBytes <= cbSurface - offsetInBytes, VERR_INVALID_PARAMETER);

    /* Constant buffers are created on demand. */
    Assert(pSurface->pBackendSurface == NULL);

    /* Upload the current data, if any. */
    D3D11_SUBRESOURCE_DATA *pInitialData = NULL;
    D3D11_SUBRESOURCE_DATA initialData;
    if (pMipLevel->pSurfaceData)
    {
        initialData.pSysMem          = (uint8_t *)pMipLevel->pSurfaceData + offsetInBytes;
        initialData.SysMemPitch      = sizeInBytes;
        initialData.SysMemSlicePitch = sizeInBytes;

        pInitialData = &initialData;
    }

    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth           = sizeInBytes;
    bd.Usage               = D3D11_USAGE_DEFAULT;
    bd.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags      = 0;
    bd.MiscFlags           = 0;
    bd.StructureByteStride = 0;

    ID3D11Buffer *pBuffer = 0;
    HRESULT hr = pDevice->pDevice->CreateBuffer(&bd, pInitialData, &pBuffer);
    if (SUCCEEDED(hr))
    {
       dxConstantBufferSet(pDevice, slot, type, pBuffer);
       D3D_RELEASE(pBuffer);
    }

    return VINF_SUCCESS;
}

static int dxSetShaderResources(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderType type)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

//DEBUG_BREAKPOINT_TEST();
    AssertReturn(type >= SVGA3D_SHADERTYPE_MIN && type < SVGA3D_SHADERTYPE_MAX, VERR_INVALID_PARAMETER);
    uint32_t const idxShaderState = type - SVGA3D_SHADERTYPE_MIN;
    uint32_t const *pSRIds = &pDXContext->svgaDXContext.shaderState[idxShaderState].shaderResources[0];
    ID3D11ShaderResourceView *papShaderResourceView[SVGA3D_DX_MAX_SRVIEWS];
    for (uint32_t i = 0; i < SVGA3D_DX_MAX_SRVIEWS; ++i)
    {
        SVGA3dShaderResourceViewId shaderResourceViewId = pSRIds[i];
        if (shaderResourceViewId != SVGA3D_INVALID_ID)
        {
            ASSERT_GUEST_RETURN(shaderResourceViewId < pDXContext->pBackendDXContext->cShaderResourceView, VERR_INVALID_PARAMETER);

            DXVIEW *pDXView = &pDXContext->pBackendDXContext->paShaderResourceView[shaderResourceViewId];
            Assert(pDXView->u.pShaderResourceView);
            papShaderResourceView[i] = pDXView->u.pShaderResourceView;
        }
        else
            papShaderResourceView[i] = NULL;
    }

    dxShaderResourceViewSet(pDevice, type, 0, SVGA3D_DX_MAX_SRVIEWS, papShaderResourceView);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetShaderResources(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t startView, SVGA3dShaderType type, uint32_t cShaderResourceViewId, SVGA3dShaderResourceViewId const *paShaderResourceViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(startView, type, cShaderResourceViewId, paShaderResourceViewId);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderId shaderId, SVGA3dShaderType type)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(shaderId, type);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetSamplers(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t startSampler, SVGA3dShaderType type, uint32_t cSamplerId, SVGA3dSamplerId const *paSamplerId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11SamplerState *papSamplerState[SVGA3D_DX_MAX_SAMPLERS];
    for (uint32_t i = 0; i < cSamplerId; ++i)
    {
        SVGA3dSamplerId samplerId = paSamplerId[i];
        if (samplerId != SVGA3D_INVALID_ID)
        {
            ASSERT_GUEST_RETURN(samplerId < pDXContext->pBackendDXContext->cSamplerState, VERR_INVALID_PARAMETER);
            papSamplerState[i] = pDXContext->pBackendDXContext->papSamplerState[samplerId];
        }
        else
            papSamplerState[i] = NULL;
    }

    dxSamplerSet(pDevice, type, startSampler, cSamplerId, papSamplerState);
    return VINF_SUCCESS;
}


static void dxSetupPipeline(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    /* Make sure that any draw operations on shader resource views have finished. */
    AssertCompile(RT_ELEMENTS(pDXContext->svgaDXContext.shaderState) == SVGA3D_NUM_SHADERTYPE);
    AssertCompile(RT_ELEMENTS(pDXContext->svgaDXContext.shaderState[0].shaderResources) == SVGA3D_DX_MAX_SRVIEWS);

    int rc;

    /* Unbind render target views because they mught be (re-)used as shader resource views. */
    DXDEVICE *pDXDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    pDXDevice->pImmediateContext->OMSetRenderTargets(0, NULL, NULL);

    /*
     * Shader resources
     */

    /* Make sure that the shader resource views exist. */
    for (uint32_t idxShaderState = 0; idxShaderState < SVGA3D_NUM_SHADERTYPE_DX10 /** @todo SVGA3D_NUM_SHADERTYPE*/; ++idxShaderState)
    {
        for (uint32_t idxSR = 0; idxSR < SVGA3D_DX_MAX_SRVIEWS; ++idxSR)
        {
            SVGA3dShaderResourceViewId const shaderResourceViewId = pDXContext->svgaDXContext.shaderState[idxShaderState].shaderResources[idxSR];
            if (shaderResourceViewId != SVGA3D_INVALID_ID)
            {
                ASSERT_GUEST_RETURN_VOID(shaderResourceViewId < pDXContext->pBackendDXContext->cShaderResourceView);

                SVGACOTableDXSRViewEntry const *pSRViewEntry = dxGetShaderResourceViewEntry(pDXContext, shaderResourceViewId);
                AssertContinue(pSRViewEntry != NULL);

                uint32_t const sid = pSRViewEntry->sid;

                PVMSVGA3DSURFACE pSurface;
                rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, sid, &pSurface);
                AssertRCReturnVoid(rc);

                /* The guest might have invalidated the surface in which case pSurface->pBackendSurface is NULL. */
                /** @todo This is not needed for "single DX device" mode. */
                if (pSurface->pBackendSurface)
                {
                    /* Wait for the surface to finish drawing. */
                    dxSurfaceWait(pThisCC->svga.p3dState, pSurface, pDXContext->cid);
                }

                /* If a view has not been created yet, do it now. */
                if (!pDXContext->pBackendDXContext->paShaderResourceView[shaderResourceViewId].u.pView)
                {
//DEBUG_BREAKPOINT_TEST();
                    LogFunc(("Re-creating SRV: sid=%u srvid = %u\n", sid, shaderResourceViewId));
                    rc = dxDefineShaderResourceView(pThisCC, pDXContext, shaderResourceViewId, pSRViewEntry);
                    AssertContinue(RT_SUCCESS(rc));
                }

                LogFunc(("srv[%d][%d] sid = %u, srvid = %u\n", idxShaderState, idxSR, sid, shaderResourceViewId));

#ifdef DUMP_BITMAPS
                SVGA3dSurfaceImageId image;
                image.sid = sid;
                image.face = 0;
                image.mipmap = 0;
                VMSVGA3D_MAPPED_SURFACE map;
                int rc2 = vmsvga3dSurfaceMap(pThisCC, &image, NULL, VMSVGA3D_SURFACE_MAP_READ, &map);
                if (RT_SUCCESS(rc2))
                {
                    vmsvga3dMapWriteBmpFile(&map, "sr-");
                    vmsvga3dSurfaceUnmap(pThisCC, &image, &map, /* fWritten =  */ false);
                }
                else
                    Log(("Map failed %Rrc\n", rc));
#endif
            }
        }

        /* Set shader resources. */
        rc = dxSetShaderResources(pThisCC, pDXContext, (SVGA3dShaderType)(idxShaderState + SVGA3D_SHADERTYPE_MIN));
        AssertRC(rc);
    }

    /*
     * Render targets
     */

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturnVoid(pDevice->pDevice);

    /* Make sure that the render target views exist. Similar to SRVs. */
    if (pDXContext->svgaDXContext.renderState.depthStencilViewId != SVGA3D_INVALID_ID)
    {
        uint32_t const viewId = pDXContext->svgaDXContext.renderState.depthStencilViewId;

        ASSERT_GUEST_RETURN_VOID(viewId < pDXContext->pBackendDXContext->cDepthStencilView);

        SVGACOTableDXDSViewEntry const *pDSViewEntry = dxGetDepthStencilViewEntry(pDXContext, viewId);
        AssertReturnVoid(pDSViewEntry != NULL);

        PVMSVGA3DSURFACE pSurface;
        rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pDSViewEntry->sid, &pSurface);
        AssertRCReturnVoid(rc);

        /* If a view has not been created yet, do it now. */
        if (!pDXContext->pBackendDXContext->paDepthStencilView[viewId].u.pView)
        {
//DEBUG_BREAKPOINT_TEST();
            LogFunc(("Re-creating DSV: sid=%u dsvid = %u\n", pDSViewEntry->sid, viewId));
            rc = dxDefineDepthStencilView(pThisCC, pDXContext, viewId, pDSViewEntry);
            AssertReturnVoid(RT_SUCCESS(rc));
        }
    }

    for (uint32_t i = 0; i < SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS; ++i)
    {
        if (pDXContext->svgaDXContext.renderState.renderTargetViewIds[i] != SVGA3D_INVALID_ID)
        {
            uint32_t const viewId = pDXContext->svgaDXContext.renderState.renderTargetViewIds[i];

            ASSERT_GUEST_RETURN_VOID(viewId < pDXContext->pBackendDXContext->cRenderTargetView);

            SVGACOTableDXRTViewEntry const *pRTViewEntry = dxGetRenderTargetViewEntry(pDXContext, viewId);
            AssertReturnVoid(pRTViewEntry != NULL);

            PVMSVGA3DSURFACE pSurface;
            rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pRTViewEntry->sid, &pSurface);
            AssertRCReturnVoid(rc);

            /* If a view has not been created yet, do it now. */
            if (!pDXContext->pBackendDXContext->paRenderTargetView[viewId].u.pView)
            {
//DEBUG_BREAKPOINT_TEST();
                LogFunc(("Re-creating RTV: sid=%u rtvid = %u\n", pRTViewEntry->sid, viewId));
                rc = dxDefineRenderTargetView(pThisCC, pDXContext, viewId, pRTViewEntry);
                AssertReturnVoid(RT_SUCCESS(rc));
            }
        }
    }

    /* Set render targets. */
    rc = dxSetRenderTargets(pThisCC, pDXContext);
    AssertRC(rc);

    /*
     * Shaders
     */

    for (uint32_t idxShaderState = 0; idxShaderState < SVGA3D_NUM_SHADERTYPE_DX10 /** @todo SVGA3D_NUM_SHADERTYPE*/; ++idxShaderState)
    {
        DXSHADER *pDXShader;
        SVGA3dShaderType const shaderType = (SVGA3dShaderType)(idxShaderState + SVGA3D_SHADERTYPE_MIN);
        SVGA3dShaderId const shaderId = pDXContext->svgaDXContext.shaderState[idxShaderState].shaderId;

        if (shaderId != SVGA3D_INVALID_ID)
        {
            pDXShader = &pDXContext->pBackendDXContext->paShader[shaderId];
            if (pDXShader->pShader == NULL)
            {
                /* Create a new shader. */
                Log(("Shader: cid=%u shid=%u type=%d\n", pDXContext->cid, shaderId, pDXShader->enmShaderType));

                /* Apply resource types to a pixel shader. */
                if (shaderType == SVGA3D_SHADERTYPE_PS)
                {
                    SVGA3dResourceType aResourceType[SVGA3D_DX_MAX_SRVIEWS];
                    RT_ZERO(aResourceType);
                    uint32_t cResourceType = 0;

                    for (uint32_t idxSR = 0; idxSR < SVGA3D_DX_MAX_SRVIEWS; ++idxSR)
                    {
                        SVGA3dShaderResourceViewId const shaderResourceViewId = pDXContext->svgaDXContext.shaderState[idxShaderState].shaderResources[idxSR];
                        if (shaderResourceViewId != SVGA3D_INVALID_ID)
                        {
                            SVGACOTableDXSRViewEntry const *pSRViewEntry = dxGetShaderResourceViewEntry(pDXContext, shaderResourceViewId);
                            AssertContinue(pSRViewEntry != NULL);

                            aResourceType[idxSR] = pSRViewEntry->resourceDimension;
                            cResourceType = idxSR + 1;
                        }
                    }

                    rc = DXShaderUpdateResourceTypes(&pDXShader->shaderInfo, aResourceType, cResourceType);
                    AssertRC(rc); /* Ignore rc because the shader will most likely work anyway. */
                }

                rc = DXShaderCreateDXBC(&pDXShader->shaderInfo, &pDXShader->pvDXBC, &pDXShader->cbDXBC);
                if (RT_SUCCESS(rc))
                {
#ifdef LOG_ENABLED
                    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
                    if (pBackend->pfnD3DDisassemble && LogIs6Enabled())
                    {
                        ID3D10Blob *pBlob = 0;
                        HRESULT hr2 = pBackend->pfnD3DDisassemble(pDXShader->pvDXBC, pDXShader->cbDXBC, 0, NULL, &pBlob);
                        if (SUCCEEDED(hr2) && pBlob && pBlob->GetBufferSize())
                            Log6(("%s\n", pBlob->GetBufferPointer()));
                        else
                            AssertFailed();
                        D3D_RELEASE(pBlob);
                    }
#endif

                    HRESULT hr = dxShaderCreate(pThisCC, pDXContext, pDXShader);
                    if (FAILED(hr))
                        rc = VERR_INVALID_STATE;
                }
                else
                    rc = VERR_NO_MEMORY;
            }
        }
        else
            pDXShader = NULL;

        if (RT_SUCCESS(rc))
            dxShaderSet(pThisCC, pDXContext, shaderType, pDXShader);

        AssertRC(rc);
    }

    /*
     * InputLayout
     */
    SVGA3dElementLayoutId const elementLayoutId = pDXContext->svgaDXContext.inputAssembly.layoutId;
    ID3D11InputLayout *pInputLayout = NULL;
    if (elementLayoutId != SVGA3D_INVALID_ID)
    {
        DXELEMENTLAYOUT *pDXElementLayout = &pDXContext->pBackendDXContext->paElementLayout[elementLayoutId];
        if (!pDXElementLayout->pElementLayout)
        {
            uint32_t const idxShaderState = SVGA3D_SHADERTYPE_VS - SVGA3D_SHADERTYPE_MIN;
            uint32_t const shid = pDXContext->svgaDXContext.shaderState[idxShaderState].shaderId;
            if (shid < pDXContext->pBackendDXContext->cShader)
            {
                DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[shid];
                if (pDXShader->pvDXBC)
                {
                    HRESULT hr = pDevice->pDevice->CreateInputLayout(pDXElementLayout->aElementDesc,
                                                                     pDXElementLayout->cElementDesc,
                                                                     pDXShader->pvDXBC,
                                                                     pDXShader->cbDXBC,
                                                                     &pDXElementLayout->pElementLayout);
                    Assert(SUCCEEDED(hr)); RT_NOREF(hr);
                }
                else
                    LogRelMax(16, ("VMSVGA: DX shader bytecode is not available in DXSetInputLayout: shid = %u\n", shid));
            }
            else
                LogRelMax(16, ("VMSVGA: DX shader is not set in DXSetInputLayout: shid = 0x%x\n", shid));
        }

        pInputLayout = pDXElementLayout->pElementLayout;
    }

    pDevice->pImmediateContext->IASetInputLayout(pInputLayout);
}


static DECLCALLBACK(int) vmsvga3dBackDXDraw(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t vertexCount, uint32_t startVertexLocation)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    dxSetupPipeline(pThisCC, pDXContext);

    if (pDXContext->svgaDXContext.inputAssembly.topology != SVGA3D_PRIMITIVE_TRIANGLEFAN)
        pDevice->pImmediateContext->Draw(vertexCount, startVertexLocation);
    else
    {
        /*
         * Emulate SVGA3D_PRIMITIVE_TRIANGLEFAN using an indexed draw of a triangle list.
         */

        /* Make sure that 16 bit indices are enough. 20000 ~= 65536 / 3 */
        AssertReturn(vertexCount <= 20000, VERR_NOT_SUPPORTED);

        /* Generate indices. */
        UINT const IndexCount = 3 * (vertexCount - 2); /* 3_per_triangle * num_triangles */
        UINT const cbAlloc = IndexCount * sizeof(USHORT);
        USHORT *paIndices = (USHORT *)RTMemAlloc(cbAlloc);
        AssertReturn(paIndices, VERR_NO_MEMORY);
        USHORT iVertex = 1;
        for (UINT i = 0; i < IndexCount; i+= 3)
        {
            paIndices[i] = 0;
            paIndices[i + 1] = iVertex;
            ++iVertex;
            paIndices[i + 2] = iVertex;
        }

        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem          = paIndices;
        InitData.SysMemPitch      = cbAlloc;
        InitData.SysMemSlicePitch = cbAlloc;

        D3D11_BUFFER_DESC bd;
        RT_ZERO(bd);
        bd.ByteWidth           = cbAlloc;
        bd.Usage               = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags           = D3D11_BIND_INDEX_BUFFER;
        //bd.CPUAccessFlags      = 0;
        //bd.MiscFlags           = 0;
        //bd.StructureByteStride = 0;

        ID3D11Buffer *pIndexBuffer = 0;
        HRESULT hr = pDevice->pDevice->CreateBuffer(&bd, &InitData, &pIndexBuffer);
        Assert(SUCCEEDED(hr));RT_NOREF(hr);

        /* Save the current index buffer. */
        ID3D11Buffer *pSavedIndexBuffer = 0;
        DXGI_FORMAT  SavedFormat = DXGI_FORMAT_UNKNOWN;
        UINT         SavedOffset = 0;
        pDevice->pImmediateContext->IAGetIndexBuffer(&pSavedIndexBuffer, &SavedFormat, &SavedOffset);

        /* Set up the device state. */
        pDevice->pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
        pDevice->pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        UINT const StartIndexLocation = 0;
        INT const BaseVertexLocation = startVertexLocation;
        pDevice->pImmediateContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);

        /* Restore the device state. */
        pDevice->pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        pDevice->pImmediateContext->IASetIndexBuffer(pSavedIndexBuffer, SavedFormat, SavedOffset);
        D3D_RELEASE(pSavedIndexBuffer);

        /* Cleanup. */
        D3D_RELEASE(pIndexBuffer);
        RTMemFree(paIndices);
    }

    /* Note which surfaces are being drawn. */
    dxTrackRenderTargets(pThisCC, pDXContext);

    return VINF_SUCCESS;
}

static int dxReadBuffer(DXDEVICE *pDevice, ID3D11Buffer *pBuffer, UINT Offset, UINT Bytes, void **ppvData, uint32_t *pcbData)
{
    D3D11_BUFFER_DESC desc;
    RT_ZERO(desc);
    pBuffer->GetDesc(&desc);

    AssertReturn(   Offset < desc.ByteWidth
                 && Bytes <= desc.ByteWidth - Offset, VERR_INVALID_STATE);

    void *pvData = RTMemAlloc(Bytes);
    if (!pvData)
        return VERR_NO_MEMORY;

    *ppvData = pvData;
    *pcbData = Bytes;

    int rc = dxStagingBufferRealloc(pDevice, Bytes);
    if (RT_SUCCESS(rc))
    {
        /* Copy from the buffer to the staging buffer. */
        ID3D11Resource *pDstResource = pDevice->pStagingBuffer;
        UINT DstSubresource = 0;
        UINT DstX = Offset;
        UINT DstY = 0;
        UINT DstZ = 0;
        ID3D11Resource *pSrcResource = pBuffer;
        UINT SrcSubresource = 0;
        D3D11_BOX SrcBox;
        SrcBox.left   = 0;
        SrcBox.top    = 0;
        SrcBox.front  = 0;
        SrcBox.right  = Bytes;
        SrcBox.bottom = 1;
        SrcBox.back   = 1;
        pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                          pSrcResource, SrcSubresource, &SrcBox);

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        UINT const Subresource = 0; /* Buffers have only one subresource. */
        HRESULT hr = pDevice->pImmediateContext->Map(pDevice->pStagingBuffer, Subresource,
                                                     D3D11_MAP_READ, /* MapFlags =  */ 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            memcpy(pvData, mappedResource.pData, Bytes);

            /* Unmap the staging buffer. */
            pDevice->pImmediateContext->Unmap(pDevice->pStagingBuffer, Subresource);
        }
        else
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);

    }

    if (RT_FAILURE(rc))
    {
        RTMemFree(*ppvData);
        *ppvData = NULL;
        *pcbData = 0;
    }

    return rc;
}


static int dxDrawIndexedTriangleFan(DXDEVICE *pDevice, uint32_t IndexCountTF, uint32_t StartIndexLocationTF, int32_t BaseVertexLocationTF)
{
    /*
     * Emulate an indexed SVGA3D_PRIMITIVE_TRIANGLEFAN using an indexed draw of triangle list.
     */

    /* Make sure that 16 bit indices are enough. 20000 ~= 65536 / 3 */
    AssertReturn(IndexCountTF <= 20000, VERR_NOT_SUPPORTED);

    /* Save the current index buffer. */
    ID3D11Buffer *pSavedIndexBuffer = 0;
    DXGI_FORMAT  SavedFormat = DXGI_FORMAT_UNKNOWN;
    UINT         SavedOffset = 0;
    pDevice->pImmediateContext->IAGetIndexBuffer(&pSavedIndexBuffer, &SavedFormat, &SavedOffset);

    AssertReturn(   SavedFormat == DXGI_FORMAT_R16_UINT
                 || SavedFormat == DXGI_FORMAT_R32_UINT, VERR_NOT_SUPPORTED);

    /* How many bytes are used by triangle fan indices. */
    UINT const BytesPerIndexTF = SavedFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
    UINT const BytesTF = BytesPerIndexTF * IndexCountTF;

    /* Read the current index buffer content to obtain indices. */
    void *pvDataTF;
    uint32_t cbDataTF;
    int rc = dxReadBuffer(pDevice, pSavedIndexBuffer, StartIndexLocationTF, BytesTF, &pvDataTF, &cbDataTF);
    AssertRCReturn(rc, rc);
    AssertReturnStmt(cbDataTF >= BytesPerIndexTF, RTMemFree(pvDataTF), VERR_INVALID_STATE);

    /* Generate indices for triangle list. */
    UINT const IndexCount = 3 * (IndexCountTF - 2); /* 3_per_triangle * num_triangles */
    UINT const cbAlloc = IndexCount * sizeof(USHORT);
    USHORT *paIndices = (USHORT *)RTMemAlloc(cbAlloc);
    AssertReturnStmt(paIndices, RTMemFree(pvDataTF), VERR_NO_MEMORY);

    USHORT iVertex = 1;
    if (BytesPerIndexTF == 2)
    {
        USHORT *paIndicesTF = (USHORT *)pvDataTF;
        for (UINT i = 0; i < IndexCount; i+= 3)
        {
            paIndices[i] = paIndicesTF[0];
            AssertBreakStmt(iVertex < IndexCountTF, rc = VERR_INVALID_STATE);
            paIndices[i + 1] = paIndicesTF[iVertex];
            ++iVertex;
            AssertBreakStmt(iVertex < IndexCountTF, rc = VERR_INVALID_STATE);
            paIndices[i + 2] = paIndicesTF[iVertex];
        }
    }
    else
    {
        UINT *paIndicesTF = (UINT *)pvDataTF;
        for (UINT i = 0; i < IndexCount; i+= 3)
        {
            paIndices[i] = paIndicesTF[0];
            AssertBreakStmt(iVertex < IndexCountTF, rc = VERR_INVALID_STATE);
            paIndices[i + 1] = paIndicesTF[iVertex];
            ++iVertex;
            AssertBreakStmt(iVertex < IndexCountTF, rc = VERR_INVALID_STATE);
            paIndices[i + 2] = paIndicesTF[iVertex];
        }
    }

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem          = paIndices;
    InitData.SysMemPitch      = cbAlloc;
    InitData.SysMemSlicePitch = cbAlloc;

    D3D11_BUFFER_DESC bd;
    RT_ZERO(bd);
    bd.ByteWidth           = cbAlloc;
    bd.Usage               = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags           = D3D11_BIND_INDEX_BUFFER;
    //bd.CPUAccessFlags      = 0;
    //bd.MiscFlags           = 0;
    //bd.StructureByteStride = 0;

    ID3D11Buffer *pIndexBuffer = 0;
    HRESULT hr = pDevice->pDevice->CreateBuffer(&bd, &InitData, &pIndexBuffer);
    Assert(SUCCEEDED(hr));RT_NOREF(hr);

    /* Set up the device state. */
    pDevice->pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    pDevice->pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT const StartIndexLocation = 0;
    INT const BaseVertexLocation = BaseVertexLocationTF;
    pDevice->pImmediateContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);

    /* Restore the device state. */
    pDevice->pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pDevice->pImmediateContext->IASetIndexBuffer(pSavedIndexBuffer, SavedFormat, SavedOffset);
    D3D_RELEASE(pSavedIndexBuffer);

    /* Cleanup. */
    D3D_RELEASE(pIndexBuffer);
    RTMemFree(paIndices);
    RTMemFree(pvDataTF);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawIndexed(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    dxSetupPipeline(pThisCC, pDXContext);

    if (pDXContext->svgaDXContext.inputAssembly.topology != SVGA3D_PRIMITIVE_TRIANGLEFAN)
        pDevice->pImmediateContext->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
    else
    {
        dxDrawIndexedTriangleFan(pDevice, indexCount, startIndexLocation, baseVertexLocation);
    }

    /* Note which surfaces are being drawn. */
    dxTrackRenderTargets(pThisCC, pDXContext);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawInstanced(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext,
                                                     uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    dxSetupPipeline(pThisCC, pDXContext);

    Assert(pDXContext->svgaDXContext.inputAssembly.topology != SVGA3D_PRIMITIVE_TRIANGLEFAN);

    pDevice->pImmediateContext->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);

    /* Note which surfaces are being drawn. */
    dxTrackRenderTargets(pThisCC, pDXContext);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawIndexedInstanced(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext,
                                                            uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    dxSetupPipeline(pThisCC, pDXContext);

    Assert(pDXContext->svgaDXContext.inputAssembly.topology != SVGA3D_PRIMITIVE_TRIANGLEFAN);

    pDevice->pImmediateContext->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);

    /* Note which surfaces are being drawn. */
    dxTrackRenderTargets(pThisCC, pDXContext);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawAuto(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    Assert(pDXContext->svgaDXContext.inputAssembly.topology != SVGA3D_PRIMITIVE_TRIANGLEFAN);
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    dxSetupPipeline(pThisCC, pDXContext);

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetInputLayout(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dElementLayoutId elementLayoutId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(elementLayoutId);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetVertexBuffers(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t startBuffer, uint32_t cVertexBuffer, SVGA3dVertexBuffer const *paVertexBuffer)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* For each paVertexBuffer[i]:
     *   If the vertex buffer object does not exist then create it.
     *   If the surface has been updated by the guest then update the buffer object.
     * Use IASetVertexBuffers to set the buffers.
     */

    ID3D11Buffer *paResources[SVGA3D_DX_MAX_VERTEXBUFFERS];
    UINT paStride[SVGA3D_DX_MAX_VERTEXBUFFERS];
    UINT paOffset[SVGA3D_DX_MAX_VERTEXBUFFERS];

    for (uint32_t i = 0; i < cVertexBuffer; ++i)
    {
        uint32_t const idxVertexBuffer = startBuffer + i;

        /* Get corresponding resource. Create the buffer if does not yet exist. */
        if (paVertexBuffer[i].sid != SVGA_ID_INVALID)
        {
            PVMSVGA3DSURFACE pSurface;
            int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, paVertexBuffer[i].sid, &pSurface);
            AssertRCReturn(rc, rc);

            if (pSurface->pBackendSurface == NULL)
            {
                /* Create the resource and initialize it with the current surface data. */
                rc = vmsvga3dBackSurfaceCreateBuffer(pThisCC, pDXContext, pSurface);
                AssertRCReturn(rc, rc);
            }

            Assert(pSurface->pBackendSurface->u.pBuffer);
            paResources[idxVertexBuffer] = pSurface->pBackendSurface->u.pBuffer;
            paStride[idxVertexBuffer] = paVertexBuffer[i].stride;
            paOffset[idxVertexBuffer] = paVertexBuffer[i].offset;
        }
        else
        {
            paResources[idxVertexBuffer] = NULL;
            paStride[idxVertexBuffer] = 0;
            paOffset[idxVertexBuffer] = 0;
        }
    }

    pDevice->pImmediateContext->IASetVertexBuffers(startBuffer, cVertexBuffer,
                                                   &paResources[startBuffer], &paStride[startBuffer], &paOffset[startBuffer]);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetIndexBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dSurfaceId sid, SVGA3dSurfaceFormat format, uint32_t offset)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* Get corresponding resource. Create the buffer if does not yet exist. */
    ID3D11Buffer *pResource;
    DXGI_FORMAT enmDxgiFormat;

    if (sid != SVGA_ID_INVALID)
    {
        PVMSVGA3DSURFACE pSurface;
        int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, sid, &pSurface);
        AssertRCReturn(rc, rc);

        if (pSurface->pBackendSurface == NULL)
        {
            /* Create the resource and initialize it with the current surface data. */
            rc = vmsvga3dBackSurfaceCreateBuffer(pThisCC, pDXContext, pSurface);
            AssertRCReturn(rc, rc);
        }

        pResource = pSurface->pBackendSurface->u.pBuffer;
        enmDxgiFormat = vmsvgaDXSurfaceFormat2Dxgi(format);
        AssertReturn(enmDxgiFormat == DXGI_FORMAT_R16_UINT || enmDxgiFormat == DXGI_FORMAT_R32_UINT, VERR_INVALID_PARAMETER);
    }
    else
    {
        pResource = NULL;
        enmDxgiFormat = DXGI_FORMAT_UNKNOWN;
    }

    pDevice->pImmediateContext->IASetIndexBuffer(pResource, enmDxgiFormat, offset);
    return VINF_SUCCESS;
}

static D3D11_PRIMITIVE_TOPOLOGY dxTopology(SVGA3dPrimitiveType primitiveType)
{
    static D3D11_PRIMITIVE_TOPOLOGY const aD3D11PrimitiveTopology[SVGA3D_PRIMITIVE_MAX] =
    {
        D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
        D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
        D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, /* SVGA3D_PRIMITIVE_TRIANGLEFAN: No FAN in D3D11. */
        D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ,
        D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST,
        D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST,
    };
    return aD3D11PrimitiveTopology[primitiveType];
}

static DECLCALLBACK(int) vmsvga3dBackDXSetTopology(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dPrimitiveType topology)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    D3D11_PRIMITIVE_TOPOLOGY const enmTopology = dxTopology(topology);
    pDevice->pImmediateContext->IASetPrimitiveTopology(enmTopology);
    return VINF_SUCCESS;
}


static int dxSetRenderTargets(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11RenderTargetView *apRenderTargetViews[SVGA3D_MAX_RENDER_TARGETS];
    RT_ZERO(apRenderTargetViews);
    for (uint32_t i = 0; i < SVGA3D_MAX_RENDER_TARGETS; ++i)
    {
        SVGA3dRenderTargetViewId const renderTargetViewId = pDXContext->svgaDXContext.renderState.renderTargetViewIds[i];
        if (renderTargetViewId != SVGA3D_INVALID_ID)
        {
            ASSERT_GUEST_RETURN(renderTargetViewId < pDXContext->pBackendDXContext->cRenderTargetView, VERR_INVALID_PARAMETER);
            apRenderTargetViews[i] = pDXContext->pBackendDXContext->paRenderTargetView[renderTargetViewId].u.pRenderTargetView;
        }
    }

    ID3D11DepthStencilView *pDepthStencilView = NULL;
    SVGA3dDepthStencilViewId const depthStencilViewId = pDXContext->svgaDXContext.renderState.depthStencilViewId;
    if (depthStencilViewId != SVGA_ID_INVALID)
        pDepthStencilView = pDXContext->pBackendDXContext->paDepthStencilView[depthStencilViewId].u.pDepthStencilView;

    pDevice->pImmediateContext->OMSetRenderTargets(SVGA3D_MAX_RENDER_TARGETS,
                                                   apRenderTargetViews,
                                                   pDepthStencilView);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetRenderTargets(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilViewId depthStencilViewId, uint32_t cRenderTargetViewId, SVGA3dRenderTargetViewId const *paRenderTargetViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(depthStencilViewId, cRenderTargetViewId, paRenderTargetViewId);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetBlendState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dBlendStateId blendId, float const blendFactor[4], uint32_t sampleMask)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    if (blendId != SVGA3D_INVALID_ID)
    {
        ID3D11BlendState *pBlendState = pDXContext->pBackendDXContext->papBlendState[blendId];
        pDevice->pImmediateContext->OMSetBlendState(pBlendState, blendFactor, sampleMask);
    }
    else
        pDevice->pImmediateContext->OMSetBlendState(NULL, NULL, 0);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetDepthStencilState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilStateId depthStencilId, uint32_t stencilRef)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    if (depthStencilId != SVGA3D_INVALID_ID)
    {
        ID3D11DepthStencilState *pDepthStencilState = pDXContext->pBackendDXContext->papDepthStencilState[depthStencilId];
        pDevice->pImmediateContext->OMSetDepthStencilState(pDepthStencilState, stencilRef);
    }
    else
        pDevice->pImmediateContext->OMSetDepthStencilState(NULL, 0);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetRasterizerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRasterizerStateId rasterizerId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    if (rasterizerId != SVGA3D_INVALID_ID)
    {
        ID3D11RasterizerState *pRasterizerState = pDXContext->pBackendDXContext->papRasterizerState[rasterizerId];
        pDevice->pImmediateContext->RSSetState(pRasterizerState);
    }
    else
        pDevice->pImmediateContext->RSSetState(NULL);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetQueryOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBeginQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXEndQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXReadbackQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetPredication(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetSOTargets(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t cSOTarget, SVGA3dSoTarget const *paSoTarget)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* For each paSoTarget[i]:
     *   If the stream outout buffer object does not exist then create it.
     *   If the surface has been updated by the guest then update the buffer object.
     * Use SOSetTargets to set the buffers.
     */

    ID3D11Buffer *paResource[SVGA3D_DX_MAX_SOTARGETS];
    UINT paOffset[SVGA3D_DX_MAX_SOTARGETS];

    /* Always re-bind all 4 SO targets. They can be NULL. */
    for (uint32_t i = 0; i < SVGA3D_DX_MAX_SOTARGETS; ++i)
    {
        /* Get corresponding resource. Create the buffer if does not yet exist. */
        if (i < cSOTarget && paSoTarget[i].sid != SVGA_ID_INVALID)
        {
            PVMSVGA3DSURFACE pSurface;
            int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, paSoTarget[i].sid, &pSurface);
            AssertRCReturn(rc, rc);

            if (pSurface->pBackendSurface == NULL)
            {
                /* Create the resource. */
                rc = vmsvga3dBackSurfaceCreateSoBuffer(pThisCC, pDXContext, pSurface);
                AssertRCReturn(rc, rc);
            }

            /** @todo How paSoTarget[i].sizeInBytes is used? Maybe when the buffer is created? */
            paResource[i] = pSurface->pBackendSurface->u.pBuffer;
            paOffset[i] = paSoTarget[i].offset;
        }
        else
        {
            paResource[i] = NULL;
            paOffset[i] = 0;
        }
    }

    pDevice->pImmediateContext->SOSetTargets(SVGA3D_DX_MAX_SOTARGETS, paResource, paOffset);

    pDXContext->pBackendDXContext->cSOTarget = cSOTarget;

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetViewports(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t cViewport, SVGA3dViewport const *paViewport)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    /* D3D11_VIEWPORT is identical to SVGA3dViewport. */
    D3D11_VIEWPORT *pViewports = (D3D11_VIEWPORT *)paViewport;

    pDevice->pImmediateContext->RSSetViewports(cViewport, pViewports);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetScissorRects(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t cRect, SVGASignedRect const *paRect)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /* D3D11_RECT is identical to SVGASignedRect. */
    D3D11_RECT *pRects = (D3D11_RECT *)paRect;

    pDevice->pImmediateContext->RSSetScissorRects(cRect, pRects);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXClearRenderTargetView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRenderTargetViewId renderTargetViewId, SVGA3dRGBAFloat const *pRGBA)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    DXVIEW *pDXView = &pDXContext->pBackendDXContext->paRenderTargetView[renderTargetViewId];
    if (!pDXView->u.pRenderTargetView)
    {
//DEBUG_BREAKPOINT_TEST();
        /* (Re-)create the render target view, because a creation of a view is deferred until a draw or a clear call. */
        SVGACOTableDXRTViewEntry const *pEntry = &pDXContext->cot.paRTView[renderTargetViewId];
        int rc = dxDefineRenderTargetView(pThisCC, pDXContext, renderTargetViewId, pEntry);
        AssertRCReturn(rc, rc);
    }
    pDevice->pImmediateContext->ClearRenderTargetView(pDXView->u.pRenderTargetView, pRGBA->value);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXClearDepthStencilView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, uint32_t flags, SVGA3dDepthStencilViewId depthStencilViewId, float depth, uint8_t stencil)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    DXVIEW *pDXView = &pDXContext->pBackendDXContext->paDepthStencilView[depthStencilViewId];
    if (!pDXView->u.pDepthStencilView)
    {
//DEBUG_BREAKPOINT_TEST();
        /* (Re-)create the depth stencil view, because a creation of a view is deferred until a draw or a clear call. */
        SVGACOTableDXDSViewEntry const *pEntry = &pDXContext->cot.paDSView[depthStencilViewId];
        int rc = dxDefineDepthStencilView(pThisCC, pDXContext, depthStencilViewId, pEntry);
        AssertRCReturn(rc, rc);
    }
    pDevice->pImmediateContext->ClearDepthStencilView(pDXView->u.pDepthStencilView, flags, depth, stencil);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredCopyRegion(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dSurfaceId dstSid, uint32_t dstSubResource, SVGA3dSurfaceId srcSid, uint32_t srcSubResource, SVGA3dCopyBox const *pBox)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    PVMSVGA3DSURFACE pSrcSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, srcSid, &pSrcSurface);
    AssertRCReturn(rc, rc);

    PVMSVGA3DSURFACE pDstSurface;
    rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, dstSid, &pDstSurface);
    AssertRCReturn(rc, rc);

    if (pSrcSurface->pBackendSurface == NULL)
    {
        /* Create the resource. */
        rc = vmsvga3dBackSurfaceCreateTexture(pThisCC, pDXContext, pSrcSurface);
        AssertRCReturn(rc, rc);
    }

    if (pDstSurface->pBackendSurface == NULL)
    {
        /* Create the resource. */
        rc = vmsvga3dBackSurfaceCreateTexture(pThisCC, pDXContext, pDstSurface);
        AssertRCReturn(rc, rc);
    }

    LogFunc(("cid %d: src cid %d%s -> dst cid %d%s\n",
             pDXContext->cid, pSrcSurface->idAssociatedContext,
             (pSrcSurface->surfaceFlags & SVGA3D_SURFACE_SCREENTARGET) ? " st" : "",
             pDstSurface->idAssociatedContext,
             (pDstSurface->surfaceFlags & SVGA3D_SURFACE_SCREENTARGET) ? " st" : ""));

    /* Clip the box. */
    /** @todo Use [src|dst]SubResource to index p[Src|Dst]Surface->paMipmapLevels array directly. */
    uint32_t iSrcFace;
    uint32_t iSrcMipmap;
    vmsvga3dCalcMipmapAndFace(pSrcSurface->cLevels, srcSubResource, &iSrcMipmap, &iSrcFace);

    uint32_t iDstFace;
    uint32_t iDstMipmap;
    vmsvga3dCalcMipmapAndFace(pDstSurface->cLevels, dstSubResource, &iDstMipmap, &iDstFace);

    PVMSVGA3DMIPMAPLEVEL pSrcMipLevel;
    rc = vmsvga3dMipmapLevel(pSrcSurface, iSrcFace, iSrcMipmap, &pSrcMipLevel);
    ASSERT_GUEST_RETURN(RT_SUCCESS(rc), rc);

    PVMSVGA3DMIPMAPLEVEL pDstMipLevel;
    rc = vmsvga3dMipmapLevel(pDstSurface, iDstFace, iDstMipmap, &pDstMipLevel);
    ASSERT_GUEST_RETURN(RT_SUCCESS(rc), rc);

    SVGA3dCopyBox clipBox = *pBox;
    vmsvgaR3ClipCopyBox(&pSrcMipLevel->mipmapSize, &pDstMipLevel->mipmapSize, &clipBox);

    UINT DstSubresource = dstSubResource;
    UINT DstX = clipBox.x;
    UINT DstY = clipBox.y;
    UINT DstZ = clipBox.z;

    UINT SrcSubresource = srcSubResource;
    D3D11_BOX SrcBox;
    SrcBox.left   = clipBox.srcx;
    SrcBox.top    = clipBox.srcy;
    SrcBox.front  = clipBox.srcz;
    SrcBox.right  = clipBox.srcx + clipBox.w;
    SrcBox.bottom = clipBox.srcy + clipBox.h;
    SrcBox.back   = clipBox.srcz + clipBox.d;

    ID3D11Resource *pDstResource;
    ID3D11Resource *pSrcResource;

    pDstResource = dxResource(pThisCC->svga.p3dState, pDstSurface, pDXContext);
    pSrcResource = dxResource(pThisCC->svga.p3dState, pSrcSurface, pDXContext);

    pDevice->pImmediateContext->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
                                                      pSrcResource, SrcSubresource, &SrcBox);

    pDstSurface->pBackendSurface->cidDrawing = pDXContext->cid;
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPresentBlt(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXGenMips(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderResourceViewId shaderResourceViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    ID3D11ShaderResourceView *pShaderResourceView = pDXContext->pBackendDXContext->paShaderResourceView[shaderResourceViewId].u.pShaderResourceView;
    AssertReturn(pShaderResourceView, VERR_INVALID_STATE);

    SVGACOTableDXSRViewEntry const *pSRViewEntry = dxGetShaderResourceViewEntry(pDXContext, shaderResourceViewId);
    AssertReturn(pSRViewEntry, VERR_INVALID_STATE);

    uint32_t const sid = pSRViewEntry->sid;

    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, sid, &pSurface);
    AssertRCReturn(rc, rc);
    AssertReturn(pSurface->pBackendSurface, VERR_INVALID_STATE);

    pDevice->pImmediateContext->GenerateMips(pShaderResourceView);

    pSurface->pBackendSurface->cidDrawing = pDXContext->cid;
    return VINF_SUCCESS;
}


static int dxDefineShaderResourceView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderResourceViewId shaderResourceViewId, SVGACOTableDXSRViewEntry const *pEntry)
{
    /* Get corresponding resource for pEntry->sid. Create the surface if does not yet exist. */
    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pEntry->sid, &pSurface);
    AssertRCReturn(rc, rc);

    ID3D11ShaderResourceView *pShaderResourceView;
    DXVIEW *pView = &pDXContext->pBackendDXContext->paShaderResourceView[shaderResourceViewId];
    Assert(pView->u.pView == NULL);

    if (pSurface->pBackendSurface == NULL)
    {
        /* Create the actual texture. */
        rc = vmsvga3dBackSurfaceCreateTexture(pThisCC, pDXContext, pSurface);
        AssertRCReturn(rc, rc);
    }

    HRESULT hr = dxShaderResourceViewCreate(pThisCC, pDXContext, pEntry, pSurface, &pShaderResourceView);
    AssertReturn(SUCCEEDED(hr), VERR_INVALID_STATE);

    return dxViewInit(pView, pSurface, pDXContext, shaderResourceViewId, VMSVGA3D_VIEWTYPE_SHADERRESOURCE, pShaderResourceView);
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineShaderResourceView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderResourceViewId shaderResourceViewId, SVGACOTableDXSRViewEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    /** @todo Probably not necessary because SRVs are defined in setupPipeline. */
    return dxDefineShaderResourceView(pThisCC, pDXContext, shaderResourceViewId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyShaderResourceView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderResourceViewId shaderResourceViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    return dxViewDestroy(&pDXContext->pBackendDXContext->paShaderResourceView[shaderResourceViewId]);
}


static int dxDefineRenderTargetView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRenderTargetViewId renderTargetViewId, SVGACOTableDXRTViewEntry const *pEntry)
{
    /* Get corresponding resource for pEntry->sid. Create the surface if does not yet exist. */
    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pEntry->sid, &pSurface);
    AssertRCReturn(rc, rc);

    DXVIEW *pView = &pDXContext->pBackendDXContext->paRenderTargetView[renderTargetViewId];
    Assert(pView->u.pView == NULL);

    if (pSurface->pBackendSurface == NULL)
    {
        /* Create the actual texture. */
        rc = vmsvga3dBackSurfaceCreateTexture(pThisCC, pDXContext, pSurface);
        AssertRCReturn(rc, rc);
    }

    ID3D11RenderTargetView *pRenderTargetView;
    HRESULT hr = dxRenderTargetViewCreate(pThisCC, pDXContext, pEntry, pSurface, &pRenderTargetView);
    AssertReturn(SUCCEEDED(hr), VERR_INVALID_STATE);

    return dxViewInit(pView, pSurface, pDXContext, renderTargetViewId, VMSVGA3D_VIEWTYPE_RENDERTARGET, pRenderTargetView);
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineRenderTargetView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRenderTargetViewId renderTargetViewId, SVGACOTableDXRTViewEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    return dxDefineRenderTargetView(pThisCC, pDXContext, renderTargetViewId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyRenderTargetView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRenderTargetViewId renderTargetViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    return dxViewDestroy(&pDXContext->pBackendDXContext->paRenderTargetView[renderTargetViewId]);
}


static int dxDefineDepthStencilView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilViewId depthStencilViewId, SVGACOTableDXDSViewEntry const *pEntry)
{
    /* Get corresponding resource for pEntry->sid. Create the surface if does not yet exist. */
    PVMSVGA3DSURFACE pSurface;
    int rc = vmsvga3dSurfaceFromSid(pThisCC->svga.p3dState, pEntry->sid, &pSurface);
    AssertRCReturn(rc, rc);

    DXVIEW *pView = &pDXContext->pBackendDXContext->paDepthStencilView[depthStencilViewId];
    Assert(pView->u.pView == NULL);

    if (   pSurface->pBackendSurface != NULL
        && pDXContext->cid != pSurface->idAssociatedContext)
    {
        /* Supposed to be per context. Sometimes the guest reuses the texture in another context. */
        vmsvga3dBackSurfaceDestroy(pThisCC, pSurface);
    }

    if (pSurface->pBackendSurface == NULL)
    {
        /* Create the actual texture. */
        rc = vmsvga3dBackSurfaceCreateDepthStencilTexture(pThisCC, pDXContext, pSurface);
        AssertRCReturn(rc, rc);
    }

    ID3D11DepthStencilView *pDepthStencilView;
    HRESULT hr = dxDepthStencilViewCreate(pThisCC, pDXContext, pEntry, pSurface, &pDepthStencilView);
    AssertReturn(SUCCEEDED(hr), VERR_INVALID_STATE);

    return dxViewInit(pView, pSurface, pDXContext, depthStencilViewId, VMSVGA3D_VIEWTYPE_DEPTHSTENCIL, pDepthStencilView);
}

static DECLCALLBACK(int) vmsvga3dBackDXDefineDepthStencilView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilViewId depthStencilViewId, SVGACOTableDXDSViewEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    return dxDefineDepthStencilView(pThisCC, pDXContext, depthStencilViewId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyDepthStencilView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilViewId depthStencilViewId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    return dxViewDestroy(&pDXContext->pBackendDXContext->paDepthStencilView[depthStencilViewId]);
}


static int dxDefineElementLayout(PVMSVGA3DDXCONTEXT pDXContext, SVGA3dElementLayoutId elementLayoutId, SVGACOTableDXElementLayoutEntry const *pEntry)
{
    DXELEMENTLAYOUT *pDXElementLayout = &pDXContext->pBackendDXContext->paElementLayout[elementLayoutId];
    D3D_RELEASE(pDXElementLayout->pElementLayout);

    /* Semantic name is not interpreted by D3D, therefore arbitrary names can be used
     * if they are consistent between the element layout and shader input signature.
     * "In general, data passed between pipeline stages is completely generic and is not uniquely
     * interpreted by the system; arbitrary semantics are allowed ..."
     *
     * However D3D runtime insists that "SemanticName string ("POSITIO1") cannot end with a number."
     *
     * System-Value semantics ("SV_*") between shaders require proper names of course.
     * But they are irrelevant for input attributes.
     */
    pDXElementLayout->cElementDesc = pEntry->numDescs;
    for (uint32_t i = 0; i < pEntry->numDescs; ++i)
    {
        D3D11_INPUT_ELEMENT_DESC *pDst = &pDXElementLayout->aElementDesc[i];
        SVGA3dInputElementDesc const *pSrc = &pEntry->descs[i];
        pDst->SemanticName         = "ATTRIB";
        pDst->SemanticIndex        = i; /// @todo 'pSrc->inputRegister' is unused, maybe it should somehow.
        pDst->Format               = vmsvgaDXSurfaceFormat2Dxgi(pSrc->format);
        AssertReturn(pDst->Format != DXGI_FORMAT_UNKNOWN, VERR_NOT_IMPLEMENTED);
        pDst->InputSlot            = pSrc->inputSlot;
        pDst->AlignedByteOffset    = pSrc->alignedByteOffset;
        pDst->InputSlotClass       = (D3D11_INPUT_CLASSIFICATION)pSrc->inputSlotClass;
        pDst->InstanceDataStepRate = pSrc->instanceDataStepRate;
    }

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineElementLayout(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dElementLayoutId elementLayoutId, SVGACOTableDXElementLayoutEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    /* Not much can be done here because ID3D11Device::CreateInputLayout requires
     * a pShaderBytecodeWithInputSignature which is not known at this moment.
     * InputLayout object will be created in SVGA_3D_CMD_DX_SET_INPUT_LAYOUT.
     */

    Assert(elementLayoutId == pEntry->elid);

    return dxDefineElementLayout(pDXContext, elementLayoutId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyElementLayout(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static int dxDefineBlendState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext,
                              SVGA3dBlendStateId blendId, SVGACOTableDXBlendStateEntry const *pEntry)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    HRESULT hr = dxBlendStateCreate(pDevice, pEntry, &pDXContext->pBackendDXContext->papBlendState[blendId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}

static DECLCALLBACK(int) vmsvga3dBackDXDefineBlendState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext,
                                                        SVGA3dBlendStateId blendId, SVGACOTableDXBlendStateEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    return dxDefineBlendState(pThisCC, pDXContext, blendId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyBlendState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static int dxDefineDepthStencilState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilStateId depthStencilId, SVGACOTableDXDepthStencilEntry const *pEntry)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    HRESULT hr = dxDepthStencilStateCreate(pDevice, pEntry, &pDXContext->pBackendDXContext->papDepthStencilState[depthStencilId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineDepthStencilState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dDepthStencilStateId depthStencilId, SVGACOTableDXDepthStencilEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    return dxDefineDepthStencilState(pThisCC, pDXContext, depthStencilId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyDepthStencilState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static int dxDefineRasterizerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRasterizerStateId rasterizerId, SVGACOTableDXRasterizerStateEntry const *pEntry)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    HRESULT hr = dxRasterizerStateCreate(pDevice, pEntry, &pDXContext->pBackendDXContext->papRasterizerState[rasterizerId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineRasterizerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dRasterizerStateId rasterizerId, SVGACOTableDXRasterizerStateEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    return dxDefineRasterizerState(pThisCC, pDXContext, rasterizerId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyRasterizerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static int dxDefineSamplerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dSamplerId samplerId, SVGACOTableDXSamplerEntry const *pEntry)
{
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    HRESULT hr = dxSamplerStateCreate(pDevice, pEntry, &pDXContext->pBackendDXContext->papSamplerState[samplerId]);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;
    return VERR_INVALID_STATE;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineSamplerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dSamplerId samplerId, SVGACOTableDXSamplerEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    return dxDefineSamplerState(pThisCC, pDXContext, samplerId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroySamplerState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}



static int dxDefineShader(PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderId shaderId, SVGACOTableDXShaderEntry const *pEntry)
{
    /** @todo A common approach for creation of COTable backend objects: runtime, empty DX COTable, live DX COTable. */
    DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[shaderId];
    Assert(pDXShader->enmShaderType == SVGA3D_SHADERTYPE_INVALID);

    /* Init the backend shader structure, if the shader has not been created yet. */
    pDXShader->enmShaderType = pEntry->type;
    pDXShader->pShader = NULL;
    pDXShader->soid = SVGA_ID_INVALID;

    return VINF_SUCCESS;
}


static int dxDestroyShader(DXSHADER *pDXShader)
{
    pDXShader->enmShaderType = SVGA3D_SHADERTYPE_INVALID;
    D3D_RELEASE(pDXShader->pShader);
    RTMemFree(pDXShader->pvDXBC);
    pDXShader->pvDXBC = NULL;
    pDXShader->cbDXBC = 0;
    pDXShader->soid = SVGA_ID_INVALID;
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderId shaderId, SVGACOTableDXShaderEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    return dxDefineShader(pDXContext, shaderId, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderId shaderId)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[shaderId];
    dxDestroyShader(pDXShader);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dShaderId shaderId, DXShaderInfo const *pShaderInfo)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    DXDEVICE *pDevice = dxDeviceFromContext(pThisCC->svga.p3dState, pDXContext);
    AssertReturn(pDevice->pDevice, VERR_INVALID_STATE);

    RT_NOREF(pBackend);

    DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[shaderId];
    if (pDXShader->pvDXBC)
    {
        /* New DXBC code and new shader must be created. */
        D3D_RELEASE(pDXShader->pShader);
        RTMemFree(pDXShader->pvDXBC);
        pDXShader->pvDXBC = NULL;
        pDXShader->cbDXBC = 0;
    }

    pDXShader->shaderInfo = *pShaderInfo;

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineStreamOutput(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dStreamOutputId soid, SVGACOTableDXStreamOutputEntry const *pEntry)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXSTREAMOUTPUT *pDXStreamOutput = &pDXContext->pBackendDXContext->paStreamOutput[soid];
    dxDestroyStreamOutput(pDXStreamOutput);

    return dxDefineStreamOutput(pDXContext, soid, pEntry);
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyStreamOutput(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dStreamOutputId soid)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    DXSTREAMOUTPUT *pDXStreamOutput = &pDXContext->pBackendDXContext->paStreamOutput[soid];
    dxDestroyStreamOutput(pDXStreamOutput);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetStreamOutput(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGA3dStreamOutputId soid)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend, pDXContext, soid);

    return VINF_SUCCESS;
}


static int dxCOTableRealloc(void **ppvCOTable, uint32_t *pcCOTable, uint32_t cbEntry, uint32_t cEntries, uint32_t cValidEntries)
{
    uint32_t const cCOTableCurrent = *pcCOTable;

    if (*pcCOTable != cEntries)
    {
        /* Grow/shrink the array. */
        if (cEntries)
        {
            void *pvNew = RTMemRealloc(*ppvCOTable, cEntries * cbEntry);
            AssertReturn(pvNew, VERR_NO_MEMORY);
            *ppvCOTable = pvNew;
        }
        else
        {
            RTMemFree(*ppvCOTable);
            *ppvCOTable = NULL;
        }

        *pcCOTable = cEntries;
    }

    if (*ppvCOTable)
    {
        uint32_t const cEntriesToKeep = RT_MIN(cCOTableCurrent, cValidEntries);
        memset((uint8_t *)(*ppvCOTable) + cEntriesToKeep * cbEntry, 0, (cEntries - cEntriesToKeep) * cbEntry);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vmsvga3dBackDXSetCOTable(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, SVGACOTableType type, uint32_t cValidEntries)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;
    RT_NOREF(pBackend);

    VMSVGA3DBACKENDDXCONTEXT *pBackendDXContext = pDXContext->pBackendDXContext;

    int rc = VINF_SUCCESS;

    /*
     * 1) Release current backend table, if exists;
     * 2) Reallocate memory for the new backend table;
     * 3) If cValidEntries is not zero, then re-define corresponding backend table elements.
     */
    switch (type)
    {
        case SVGA_COTABLE_RTVIEW:
            /* Clear current entries. */
            if (pBackendDXContext->paRenderTargetView)
            {
                for (uint32_t i = 0; i < pBackendDXContext->cRenderTargetView; ++i)
                {
                    DXVIEW *pDXView = &pBackendDXContext->paRenderTargetView[i];
                    if (i < cValidEntries)
                        dxViewRemoveFromList(pDXView); /* Remove from list because DXVIEW array will be reallocated. */
                    else
                        dxViewDestroy(pDXView);
                }
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->paRenderTargetView, &pBackendDXContext->cRenderTargetView,
                                  sizeof(pBackendDXContext->paRenderTargetView[0]), pDXContext->cot.cRTView, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXRTViewEntry const *pEntry = &pDXContext->cot.paRTView[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                /* Define views which were not defined yet in backend. */
                DXVIEW *pDXView = &pBackendDXContext->paRenderTargetView[i];
                /** @todo Verify that the pEntry content still corresponds to the view. */
                if (pDXView->u.pView)
                    dxViewAddToList(pThisCC, pDXView);
                else if (pDXView->enmViewType == VMSVGA3D_VIEWTYPE_NONE)
                    dxDefineRenderTargetView(pThisCC, pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_DSVIEW:
            if (pBackendDXContext->paDepthStencilView)
            {
                for (uint32_t i = 0; i < pBackendDXContext->cDepthStencilView; ++i)
                {
                    DXVIEW *pDXView = &pBackendDXContext->paDepthStencilView[i];
                    if (i < cValidEntries)
                        dxViewRemoveFromList(pDXView); /* Remove from list because DXVIEW array will be reallocated. */
                    else
                        dxViewDestroy(pDXView);
                }
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->paDepthStencilView, &pBackendDXContext->cDepthStencilView,
                                  sizeof(pBackendDXContext->paDepthStencilView[0]), pDXContext->cot.cDSView, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXDSViewEntry const *pEntry = &pDXContext->cot.paDSView[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                /* Define views which were not defined yet in backend. */
                DXVIEW *pDXView = &pBackendDXContext->paDepthStencilView[i];
                /** @todo Verify that the pEntry content still corresponds to the view. */
                if (pDXView->u.pView)
                    dxViewAddToList(pThisCC, pDXView);
                else if (pDXView->enmViewType == VMSVGA3D_VIEWTYPE_NONE)
                    dxDefineDepthStencilView(pThisCC, pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_SRVIEW:
            if (pBackendDXContext->paShaderResourceView)
            {
                for (uint32_t i = 0; i < pBackendDXContext->cShaderResourceView; ++i)
                {
                    DXVIEW *pDXView = &pBackendDXContext->paShaderResourceView[i];
                    if (i < cValidEntries)
                        dxViewRemoveFromList(pDXView); /* Remove from list because DXVIEW array will be reallocated. */
                    else
                        dxViewDestroy(pDXView);
                }
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->paShaderResourceView, &pBackendDXContext->cShaderResourceView,
                                  sizeof(pBackendDXContext->paShaderResourceView[0]), pDXContext->cot.cSRView, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXSRViewEntry const *pEntry = &pDXContext->cot.paSRView[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                /* Define views which were not defined yet in backend. */
                DXVIEW *pDXView = &pBackendDXContext->paShaderResourceView[i];
                /** @todo Verify that the pEntry content still corresponds to the view. */
                if (pDXView->u.pView)
                    dxViewAddToList(pThisCC, pDXView);
                else if (pDXView->enmViewType == VMSVGA3D_VIEWTYPE_NONE)
                    dxDefineShaderResourceView(pThisCC, pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_ELEMENTLAYOUT:
            if (pBackendDXContext->paElementLayout)
            {
                for (uint32_t i = cValidEntries; i < pBackendDXContext->cElementLayout; ++i)
                    D3D_RELEASE(pBackendDXContext->paElementLayout[i].pElementLayout);
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->paElementLayout, &pBackendDXContext->cElementLayout,
                                  sizeof(pBackendDXContext->paElementLayout[0]), pDXContext->cot.cElementLayout, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXElementLayoutEntry const *pEntry = &pDXContext->cot.paElementLayout[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                dxDefineElementLayout(pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_BLENDSTATE:
            if (pBackendDXContext->papBlendState)
            {
                for (uint32_t i = cValidEntries; i < pBackendDXContext->cBlendState; ++i)
                    D3D_RELEASE(pBackendDXContext->papBlendState[i]);
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->papBlendState, &pBackendDXContext->cBlendState,
                                  sizeof(pBackendDXContext->papBlendState[0]), pDXContext->cot.cBlendState, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXBlendStateEntry const *pEntry = &pDXContext->cot.paBlendState[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                dxDefineBlendState(pThisCC, pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_DEPTHSTENCIL:
            if (pBackendDXContext->papDepthStencilState)
            {
                for (uint32_t i = cValidEntries; i < pBackendDXContext->cDepthStencilState; ++i)
                    D3D_RELEASE(pBackendDXContext->papDepthStencilState[i]);
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->papDepthStencilState, &pBackendDXContext->cDepthStencilState,
                                  sizeof(pBackendDXContext->papDepthStencilState[0]), pDXContext->cot.cDepthStencil, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXDepthStencilEntry const *pEntry = &pDXContext->cot.paDepthStencil[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                dxDefineDepthStencilState(pThisCC, pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_RASTERIZERSTATE:
            if (pBackendDXContext->papRasterizerState)
            {
                for (uint32_t i = cValidEntries; i < pBackendDXContext->cRasterizerState; ++i)
                    D3D_RELEASE(pBackendDXContext->papRasterizerState[i]);
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->papRasterizerState, &pBackendDXContext->cRasterizerState,
                                  sizeof(pBackendDXContext->papRasterizerState[0]), pDXContext->cot.cRasterizerState, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXRasterizerStateEntry const *pEntry = &pDXContext->cot.paRasterizerState[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                dxDefineRasterizerState(pThisCC, pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_SAMPLER:
            if (pBackendDXContext->papSamplerState)
            {
                for (uint32_t i = cValidEntries; i < pBackendDXContext->cSamplerState; ++i)
                    D3D_RELEASE(pBackendDXContext->papSamplerState[i]);
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->papSamplerState, &pBackendDXContext->cSamplerState,
                                  sizeof(pBackendDXContext->papSamplerState[0]), pDXContext->cot.cSampler, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXSamplerEntry const *pEntry = &pDXContext->cot.paSampler[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                dxDefineSamplerState(pThisCC, pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_STREAMOUTPUT:
            if (pBackendDXContext->paStreamOutput)
            {
                for (uint32_t i = cValidEntries; i < pBackendDXContext->cStreamOutput; ++i)
                    dxDestroyStreamOutput(&pBackendDXContext->paStreamOutput[i]);
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->paStreamOutput, &pBackendDXContext->cStreamOutput,
                                  sizeof(pBackendDXContext->paStreamOutput[0]), pDXContext->cot.cStreamOutput, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXStreamOutputEntry const *pEntry = &pDXContext->cot.paStreamOutput[i];
                /** @todo The caller must verify the COTable content using same rules as when a new entry is defined. */
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                dxDefineStreamOutput(pDXContext, i, pEntry);
            }
            break;
        case SVGA_COTABLE_DXQUERY:
            if (pBackendDXContext->papQuery)
            {
                for (uint32_t i = cValidEntries; i < pBackendDXContext->cQuery; ++i)
                    D3D_RELEASE(pBackendDXContext->papQuery[i]);
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->papQuery, &pBackendDXContext->cQuery,
                                  sizeof(pBackendDXContext->papQuery[0]), pDXContext->cot.cQuery, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXQueryEntry const *pEntry = &pDXContext->cot.paQuery[i];
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                AssertFailed(); /** @todo implement */
            }
            break;
        case SVGA_COTABLE_DXSHADER:
            if (pBackendDXContext->paShader)
            {
                /* Destroy the no longer used entries. */
                for (uint32_t i = cValidEntries; i < pBackendDXContext->cShader; ++i)
                    dxDestroyShader(&pBackendDXContext->paShader[i]);
            }

            rc = dxCOTableRealloc((void **)&pBackendDXContext->paShader, &pBackendDXContext->cShader,
                                  sizeof(pBackendDXContext->paShader[0]), pDXContext->cot.cShader, cValidEntries);
            AssertRCBreak(rc);

            for (uint32_t i = 0; i < cValidEntries; ++i)
            {
                SVGACOTableDXShaderEntry const *pEntry = &pDXContext->cot.paShader[i];
                /** @todo The caller must verify the COTable content using same rules as when a new entry is defined. */
                if (ASMMemFirstNonZero(pEntry, sizeof(*pEntry)) == NULL)
                    continue; /* Skip uninitialized entry. */

                /* Define shaders which were not defined yet in backend. */
                DXSHADER *pDXShader = &pBackendDXContext->paShader[i];
                if (   pEntry->type != SVGA3D_SHADERTYPE_INVALID
                    && pDXShader->enmShaderType == SVGA3D_SHADERTYPE_INVALID)
                    dxDefineShader(pDXContext, i, pEntry);
                else
                    Assert(pEntry->type == pDXShader->enmShaderType);

            }
            break;
        case SVGA_COTABLE_UAVIEW:
            AssertFailed(); /** @todo Implement */
            break;
        case SVGA_COTABLE_MAX: break; /* Compiler warning */
    }
    return rc;
}


static DECLCALLBACK(int) vmsvga3dBackDXBufferCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSurfaceCopyAndReadback(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXMoveQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindAllQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXReadbackAllQuery(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXMobFence64(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindAllShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXHint(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBufferUpdate(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetVSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetPSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetGSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetHSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetDSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetCSConstantBufferOffset(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXCondBindAllShader(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackScreenCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackGrowOTable(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXGrowCOTable(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackIntraSurfaceCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDefineGBSurface_v3(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXResolveCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredResolveCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredConvertRegion(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXPredConvert(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackWholeSurfaceCopy(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineUAView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDestroyUAView(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXClearUAViewUint(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXClearUAViewFloat(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXCopyStructureCount(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetUAViews(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawIndexedInstancedIndirect(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDrawInstancedIndirect(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDispatch(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDispatchIndirect(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackWriteZeroSurface(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackHintZeroSurface(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXTransferToBuffer(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetStructureCount(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsBitBlt(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsTransBlt(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsStretchBlt(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsColorFill(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsAlphaBlend(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackLogicOpsClearTypeBlend(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDefineGBSurface_v4(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetCSUAViews(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetMinLOD(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXDefineStreamOutputWithMob(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXSetShaderIface(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindStreamOutput(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackSurfaceStretchBltNonMSToMS(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXBindShaderIface(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext)
{
    PVMSVGA3DBACKEND pBackend = pThisCC->svga.p3dState->pBackend;

    RT_NOREF(pBackend, pDXContext);
    AssertFailed(); /** @todo Implement */
    return VERR_NOT_IMPLEMENTED;
}


static DECLCALLBACK(int) vmsvga3dBackDXLoadState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM)
{
    RT_NOREF(pThisCC);
    uint32_t u32;
    int rc;

    rc = pHlp->pfnSSMGetU32(pSSM, &u32);
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelRCReturn(u32 == pDXContext->pBackendDXContext->cShader, VERR_INVALID_STATE);

    for (uint32_t i = 0; i < pDXContext->pBackendDXContext->cShader; ++i)
    {
        DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[i];

        rc = pHlp->pfnSSMGetU32(pSSM, &u32);
        AssertLogRelRCReturn(rc, rc);
        AssertLogRelReturn((SVGA3dShaderType)u32 == pDXShader->enmShaderType, VERR_INVALID_STATE);

        if (pDXShader->enmShaderType == SVGA3D_SHADERTYPE_INVALID)
            continue;

        pHlp->pfnSSMGetU32(pSSM, &pDXShader->soid);

        pHlp->pfnSSMGetU32(pSSM, &u32);
        pDXShader->shaderInfo.enmProgramType = (VGPU10_PROGRAM_TYPE)u32;

        rc = pHlp->pfnSSMGetU32(pSSM, &pDXShader->shaderInfo.cbBytecode);
        AssertLogRelRCReturn(rc, rc);
        AssertLogRelReturn(pDXShader->shaderInfo.cbBytecode <= 2 * SVGA3D_MAX_SHADER_MEMORY_BYTES, VERR_INVALID_STATE);

        if (pDXShader->shaderInfo.cbBytecode)
        {
            pDXShader->shaderInfo.pvBytecode = RTMemAlloc(pDXShader->shaderInfo.cbBytecode);
            AssertPtrReturn(pDXShader->shaderInfo.pvBytecode, VERR_NO_MEMORY);
            pHlp->pfnSSMGetMem(pSSM, pDXShader->shaderInfo.pvBytecode, pDXShader->shaderInfo.cbBytecode);
        }

        rc = pHlp->pfnSSMGetU32(pSSM, &pDXShader->shaderInfo.cInputSignature);
        AssertLogRelRCReturn(rc, rc);
        AssertLogRelReturn(pDXShader->shaderInfo.cInputSignature <= 32, VERR_INVALID_STATE);
        if (pDXShader->shaderInfo.cInputSignature)
            pHlp->pfnSSMGetMem(pSSM, pDXShader->shaderInfo.aInputSignature, pDXShader->shaderInfo.cInputSignature * sizeof(SVGA3dDXSignatureEntry));

        rc = pHlp->pfnSSMGetU32(pSSM, &pDXShader->shaderInfo.cOutputSignature);
        AssertLogRelRCReturn(rc, rc);
        AssertLogRelReturn(pDXShader->shaderInfo.cOutputSignature <= 32, VERR_INVALID_STATE);
        if (pDXShader->shaderInfo.cOutputSignature)
            pHlp->pfnSSMGetMem(pSSM, pDXShader->shaderInfo.aOutputSignature, pDXShader->shaderInfo.cOutputSignature * sizeof(SVGA3dDXSignatureEntry));

        rc = pHlp->pfnSSMGetU32(pSSM, &pDXShader->shaderInfo.cPatchConstantSignature);
        AssertLogRelRCReturn(rc, rc);
        AssertLogRelReturn(pDXShader->shaderInfo.cPatchConstantSignature <= 32, VERR_INVALID_STATE);
        if (pDXShader->shaderInfo.cPatchConstantSignature)
            pHlp->pfnSSMGetMem(pSSM, pDXShader->shaderInfo.aPatchConstantSignature, pDXShader->shaderInfo.cPatchConstantSignature * sizeof(SVGA3dDXSignatureEntry));

        rc = pHlp->pfnSSMGetU32(pSSM, &pDXShader->shaderInfo.cDclResource);
        AssertLogRelRCReturn(rc, rc);
        AssertLogRelReturn(pDXShader->shaderInfo.cDclResource <= SVGA3D_DX_MAX_SRVIEWS, VERR_INVALID_STATE);
        if (pDXShader->shaderInfo.cDclResource)
            pHlp->pfnSSMGetMem(pSSM, pDXShader->shaderInfo.aOffDclResource, pDXShader->shaderInfo.cDclResource * sizeof(uint32_t));
    }

    rc = pHlp->pfnSSMGetU32(pSSM, &pDXContext->pBackendDXContext->cSOTarget);
    AssertLogRelRCReturn(rc, rc);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackDXSaveState(PVGASTATECC pThisCC, PVMSVGA3DDXCONTEXT pDXContext, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM)
{
    RT_NOREF(pThisCC);
    int rc;

    pHlp->pfnSSMPutU32(pSSM, pDXContext->pBackendDXContext->cShader);
    for (uint32_t i = 0; i < pDXContext->pBackendDXContext->cShader; ++i)
    {
        DXSHADER *pDXShader = &pDXContext->pBackendDXContext->paShader[i];

        pHlp->pfnSSMPutU32(pSSM, (uint32_t)pDXShader->enmShaderType);
        if (pDXShader->enmShaderType == SVGA3D_SHADERTYPE_INVALID)
            continue;

        pHlp->pfnSSMPutU32(pSSM, pDXShader->soid);

        pHlp->pfnSSMPutU32(pSSM, (uint32_t)pDXShader->shaderInfo.enmProgramType);

        pHlp->pfnSSMPutU32(pSSM, pDXShader->shaderInfo.cbBytecode);
        if (pDXShader->shaderInfo.cbBytecode)
            pHlp->pfnSSMPutMem(pSSM, pDXShader->shaderInfo.pvBytecode, pDXShader->shaderInfo.cbBytecode);

        pHlp->pfnSSMPutU32(pSSM, pDXShader->shaderInfo.cInputSignature);
        if (pDXShader->shaderInfo.cInputSignature)
            pHlp->pfnSSMPutMem(pSSM, pDXShader->shaderInfo.aInputSignature, pDXShader->shaderInfo.cInputSignature * sizeof(SVGA3dDXSignatureEntry));

        pHlp->pfnSSMPutU32(pSSM, pDXShader->shaderInfo.cOutputSignature);
        if (pDXShader->shaderInfo.cOutputSignature)
            pHlp->pfnSSMPutMem(pSSM, pDXShader->shaderInfo.aOutputSignature, pDXShader->shaderInfo.cOutputSignature * sizeof(SVGA3dDXSignatureEntry));

        pHlp->pfnSSMPutU32(pSSM, pDXShader->shaderInfo.cPatchConstantSignature);
        if (pDXShader->shaderInfo.cPatchConstantSignature)
            pHlp->pfnSSMPutMem(pSSM, pDXShader->shaderInfo.aPatchConstantSignature, pDXShader->shaderInfo.cPatchConstantSignature * sizeof(SVGA3dDXSignatureEntry));

        pHlp->pfnSSMPutU32(pSSM, pDXShader->shaderInfo.cDclResource);
        if (pDXShader->shaderInfo.cDclResource)
            pHlp->pfnSSMPutMem(pSSM, pDXShader->shaderInfo.aOffDclResource, pDXShader->shaderInfo.cDclResource * sizeof(uint32_t));
    }
    rc = pHlp->pfnSSMPutU32(pSSM, pDXContext->pBackendDXContext->cSOTarget);
    AssertLogRelRCReturn(rc, rc);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmsvga3dBackQueryInterface(PVGASTATECC pThisCC, char const *pszInterfaceName, void *pvInterfaceFuncs, size_t cbInterfaceFuncs)
{
    RT_NOREF(pThisCC);

    int rc = VINF_SUCCESS;
    if (RTStrCmp(pszInterfaceName, VMSVGA3D_BACKEND_INTERFACE_NAME_DX) == 0)
    {
        if (cbInterfaceFuncs == sizeof(VMSVGA3DBACKENDFUNCSDX))
        {
            if (pvInterfaceFuncs)
            {
                VMSVGA3DBACKENDFUNCSDX *p = (VMSVGA3DBACKENDFUNCSDX *)pvInterfaceFuncs;
                p->pfnDXSaveState                 = vmsvga3dBackDXSaveState;
                p->pfnDXLoadState                 = vmsvga3dBackDXLoadState;
                p->pfnDXDefineContext             = vmsvga3dBackDXDefineContext;
                p->pfnDXDestroyContext            = vmsvga3dBackDXDestroyContext;
                p->pfnDXBindContext               = vmsvga3dBackDXBindContext;
                p->pfnDXSwitchContext             = vmsvga3dBackDXSwitchContext;
                p->pfnDXReadbackContext           = vmsvga3dBackDXReadbackContext;
                p->pfnDXInvalidateContext         = vmsvga3dBackDXInvalidateContext;
                p->pfnDXSetSingleConstantBuffer   = vmsvga3dBackDXSetSingleConstantBuffer;
                p->pfnDXSetShaderResources        = vmsvga3dBackDXSetShaderResources;
                p->pfnDXSetShader                 = vmsvga3dBackDXSetShader;
                p->pfnDXSetSamplers               = vmsvga3dBackDXSetSamplers;
                p->pfnDXDraw                      = vmsvga3dBackDXDraw;
                p->pfnDXDrawIndexed               = vmsvga3dBackDXDrawIndexed;
                p->pfnDXDrawInstanced             = vmsvga3dBackDXDrawInstanced;
                p->pfnDXDrawIndexedInstanced      = vmsvga3dBackDXDrawIndexedInstanced;
                p->pfnDXDrawAuto                  = vmsvga3dBackDXDrawAuto;
                p->pfnDXSetInputLayout            = vmsvga3dBackDXSetInputLayout;
                p->pfnDXSetVertexBuffers          = vmsvga3dBackDXSetVertexBuffers;
                p->pfnDXSetIndexBuffer            = vmsvga3dBackDXSetIndexBuffer;
                p->pfnDXSetTopology               = vmsvga3dBackDXSetTopology;
                p->pfnDXSetRenderTargets          = vmsvga3dBackDXSetRenderTargets;
                p->pfnDXSetBlendState             = vmsvga3dBackDXSetBlendState;
                p->pfnDXSetDepthStencilState      = vmsvga3dBackDXSetDepthStencilState;
                p->pfnDXSetRasterizerState        = vmsvga3dBackDXSetRasterizerState;
                p->pfnDXDefineQuery               = vmsvga3dBackDXDefineQuery;
                p->pfnDXDestroyQuery              = vmsvga3dBackDXDestroyQuery;
                p->pfnDXBindQuery                 = vmsvga3dBackDXBindQuery;
                p->pfnDXSetQueryOffset            = vmsvga3dBackDXSetQueryOffset;
                p->pfnDXBeginQuery                = vmsvga3dBackDXBeginQuery;
                p->pfnDXEndQuery                  = vmsvga3dBackDXEndQuery;
                p->pfnDXReadbackQuery             = vmsvga3dBackDXReadbackQuery;
                p->pfnDXSetPredication            = vmsvga3dBackDXSetPredication;
                p->pfnDXSetSOTargets              = vmsvga3dBackDXSetSOTargets;
                p->pfnDXSetViewports              = vmsvga3dBackDXSetViewports;
                p->pfnDXSetScissorRects           = vmsvga3dBackDXSetScissorRects;
                p->pfnDXClearRenderTargetView     = vmsvga3dBackDXClearRenderTargetView;
                p->pfnDXClearDepthStencilView     = vmsvga3dBackDXClearDepthStencilView;
                p->pfnDXPredCopyRegion            = vmsvga3dBackDXPredCopyRegion;
                p->pfnDXPredCopy                  = vmsvga3dBackDXPredCopy;
                p->pfnDXPresentBlt                = vmsvga3dBackDXPresentBlt;
                p->pfnDXGenMips                   = vmsvga3dBackDXGenMips;
                p->pfnDXDefineShaderResourceView  = vmsvga3dBackDXDefineShaderResourceView;
                p->pfnDXDestroyShaderResourceView = vmsvga3dBackDXDestroyShaderResourceView;
                p->pfnDXDefineRenderTargetView    = vmsvga3dBackDXDefineRenderTargetView;
                p->pfnDXDestroyRenderTargetView   = vmsvga3dBackDXDestroyRenderTargetView;
                p->pfnDXDefineDepthStencilView    = vmsvga3dBackDXDefineDepthStencilView;
                p->pfnDXDestroyDepthStencilView   = vmsvga3dBackDXDestroyDepthStencilView;
                p->pfnDXDefineElementLayout       = vmsvga3dBackDXDefineElementLayout;
                p->pfnDXDestroyElementLayout      = vmsvga3dBackDXDestroyElementLayout;
                p->pfnDXDefineBlendState          = vmsvga3dBackDXDefineBlendState;
                p->pfnDXDestroyBlendState         = vmsvga3dBackDXDestroyBlendState;
                p->pfnDXDefineDepthStencilState   = vmsvga3dBackDXDefineDepthStencilState;
                p->pfnDXDestroyDepthStencilState  = vmsvga3dBackDXDestroyDepthStencilState;
                p->pfnDXDefineRasterizerState     = vmsvga3dBackDXDefineRasterizerState;
                p->pfnDXDestroyRasterizerState    = vmsvga3dBackDXDestroyRasterizerState;
                p->pfnDXDefineSamplerState        = vmsvga3dBackDXDefineSamplerState;
                p->pfnDXDestroySamplerState       = vmsvga3dBackDXDestroySamplerState;
                p->pfnDXDefineShader              = vmsvga3dBackDXDefineShader;
                p->pfnDXDestroyShader             = vmsvga3dBackDXDestroyShader;
                p->pfnDXBindShader                = vmsvga3dBackDXBindShader;
                p->pfnDXDefineStreamOutput        = vmsvga3dBackDXDefineStreamOutput;
                p->pfnDXDestroyStreamOutput       = vmsvga3dBackDXDestroyStreamOutput;
                p->pfnDXSetStreamOutput           = vmsvga3dBackDXSetStreamOutput;
                p->pfnDXSetCOTable                = vmsvga3dBackDXSetCOTable;
                p->pfnDXBufferCopy                = vmsvga3dBackDXBufferCopy;
                p->pfnDXSurfaceCopyAndReadback    = vmsvga3dBackDXSurfaceCopyAndReadback;
                p->pfnDXMoveQuery                 = vmsvga3dBackDXMoveQuery;
                p->pfnDXBindAllQuery              = vmsvga3dBackDXBindAllQuery;
                p->pfnDXReadbackAllQuery          = vmsvga3dBackDXReadbackAllQuery;
                p->pfnDXMobFence64                = vmsvga3dBackDXMobFence64;
                p->pfnDXBindAllShader             = vmsvga3dBackDXBindAllShader;
                p->pfnDXHint                      = vmsvga3dBackDXHint;
                p->pfnDXBufferUpdate              = vmsvga3dBackDXBufferUpdate;
                p->pfnDXSetVSConstantBufferOffset = vmsvga3dBackDXSetVSConstantBufferOffset;
                p->pfnDXSetPSConstantBufferOffset = vmsvga3dBackDXSetPSConstantBufferOffset;
                p->pfnDXSetGSConstantBufferOffset = vmsvga3dBackDXSetGSConstantBufferOffset;
                p->pfnDXSetHSConstantBufferOffset = vmsvga3dBackDXSetHSConstantBufferOffset;
                p->pfnDXSetDSConstantBufferOffset = vmsvga3dBackDXSetDSConstantBufferOffset;
                p->pfnDXSetCSConstantBufferOffset = vmsvga3dBackDXSetCSConstantBufferOffset;
                p->pfnDXCondBindAllShader         = vmsvga3dBackDXCondBindAllShader;
                p->pfnScreenCopy                  = vmsvga3dBackScreenCopy;
                p->pfnGrowOTable                  = vmsvga3dBackGrowOTable;
                p->pfnDXGrowCOTable               = vmsvga3dBackDXGrowCOTable;
                p->pfnIntraSurfaceCopy            = vmsvga3dBackIntraSurfaceCopy;
                p->pfnDefineGBSurface_v3          = vmsvga3dBackDefineGBSurface_v3;
                p->pfnDXResolveCopy               = vmsvga3dBackDXResolveCopy;
                p->pfnDXPredResolveCopy           = vmsvga3dBackDXPredResolveCopy;
                p->pfnDXPredConvertRegion         = vmsvga3dBackDXPredConvertRegion;
                p->pfnDXPredConvert               = vmsvga3dBackDXPredConvert;
                p->pfnWholeSurfaceCopy            = vmsvga3dBackWholeSurfaceCopy;
                p->pfnDXDefineUAView              = vmsvga3dBackDXDefineUAView;
                p->pfnDXDestroyUAView             = vmsvga3dBackDXDestroyUAView;
                p->pfnDXClearUAViewUint           = vmsvga3dBackDXClearUAViewUint;
                p->pfnDXClearUAViewFloat          = vmsvga3dBackDXClearUAViewFloat;
                p->pfnDXCopyStructureCount        = vmsvga3dBackDXCopyStructureCount;
                p->pfnDXSetUAViews                = vmsvga3dBackDXSetUAViews;
                p->pfnDXDrawIndexedInstancedIndirect = vmsvga3dBackDXDrawIndexedInstancedIndirect;
                p->pfnDXDrawInstancedIndirect     = vmsvga3dBackDXDrawInstancedIndirect;
                p->pfnDXDispatch                  = vmsvga3dBackDXDispatch;
                p->pfnDXDispatchIndirect          = vmsvga3dBackDXDispatchIndirect;
                p->pfnWriteZeroSurface            = vmsvga3dBackWriteZeroSurface;
                p->pfnHintZeroSurface             = vmsvga3dBackHintZeroSurface;
                p->pfnDXTransferToBuffer          = vmsvga3dBackDXTransferToBuffer;
                p->pfnDXSetStructureCount         = vmsvga3dBackDXSetStructureCount;
                p->pfnLogicOpsBitBlt              = vmsvga3dBackLogicOpsBitBlt;
                p->pfnLogicOpsTransBlt            = vmsvga3dBackLogicOpsTransBlt;
                p->pfnLogicOpsStretchBlt          = vmsvga3dBackLogicOpsStretchBlt;
                p->pfnLogicOpsColorFill           = vmsvga3dBackLogicOpsColorFill;
                p->pfnLogicOpsAlphaBlend          = vmsvga3dBackLogicOpsAlphaBlend;
                p->pfnLogicOpsClearTypeBlend      = vmsvga3dBackLogicOpsClearTypeBlend;
                p->pfnDefineGBSurface_v4          = vmsvga3dBackDefineGBSurface_v4;
                p->pfnDXSetCSUAViews              = vmsvga3dBackDXSetCSUAViews;
                p->pfnDXSetMinLOD                 = vmsvga3dBackDXSetMinLOD;
                p->pfnDXDefineStreamOutputWithMob = vmsvga3dBackDXDefineStreamOutputWithMob;
                p->pfnDXSetShaderIface            = vmsvga3dBackDXSetShaderIface;
                p->pfnDXBindStreamOutput          = vmsvga3dBackDXBindStreamOutput;
                p->pfnSurfaceStretchBltNonMSToMS  = vmsvga3dBackSurfaceStretchBltNonMSToMS;
                p->pfnDXBindShaderIface           = vmsvga3dBackDXBindShaderIface;
            }
        }
        else
        {
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else if (RTStrCmp(pszInterfaceName, VMSVGA3D_BACKEND_INTERFACE_NAME_MAP) == 0)
    {
        if (cbInterfaceFuncs == sizeof(VMSVGA3DBACKENDFUNCSMAP))
        {
            if (pvInterfaceFuncs)
            {
                VMSVGA3DBACKENDFUNCSMAP *p = (VMSVGA3DBACKENDFUNCSMAP *)pvInterfaceFuncs;
                p->pfnSurfaceMap   = vmsvga3dBackSurfaceMap;
                p->pfnSurfaceUnmap = vmsvga3dBackSurfaceUnmap;
            }
        }
        else
        {
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else if (RTStrCmp(pszInterfaceName, VMSVGA3D_BACKEND_INTERFACE_NAME_GBO) == 0)
    {
        if (cbInterfaceFuncs == sizeof(VMSVGA3DBACKENDFUNCSGBO))
        {
            if (pvInterfaceFuncs)
            {
                VMSVGA3DBACKENDFUNCSGBO *p = (VMSVGA3DBACKENDFUNCSGBO *)pvInterfaceFuncs;
                p->pfnScreenTargetBind   = vmsvga3dScreenTargetBind;
                p->pfnScreenTargetUpdate = vmsvga3dScreenTargetUpdate;
            }
        }
        else
        {
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else if (RTStrCmp(pszInterfaceName, VMSVGA3D_BACKEND_INTERFACE_NAME_3D) == 0)
    {
        if (cbInterfaceFuncs == sizeof(VMSVGA3DBACKENDFUNCS3D))
        {
            if (pvInterfaceFuncs)
            {
                VMSVGA3DBACKENDFUNCS3D *p = (VMSVGA3DBACKENDFUNCS3D *)pvInterfaceFuncs;
                p->pfnInit                     = vmsvga3dBackInit;
                p->pfnPowerOn                  = vmsvga3dBackPowerOn;
                p->pfnTerminate                = vmsvga3dBackTerminate;
                p->pfnReset                    = vmsvga3dBackReset;
                p->pfnQueryCaps                = vmsvga3dBackQueryCaps;
                p->pfnChangeMode               = vmsvga3dBackChangeMode;
                p->pfnCreateTexture            = vmsvga3dBackCreateTexture;
                p->pfnSurfaceDestroy           = vmsvga3dBackSurfaceDestroy;
                p->pfnSurfaceInvalidateImage   = vmsvga3dBackSurfaceInvalidateImage;
                p->pfnSurfaceCopy              = vmsvga3dBackSurfaceCopy;
                p->pfnSurfaceDMACopyBox        = vmsvga3dBackSurfaceDMACopyBox;
                p->pfnSurfaceStretchBlt        = vmsvga3dBackSurfaceStretchBlt;
                p->pfnUpdateHostScreenViewport = vmsvga3dBackUpdateHostScreenViewport;
                p->pfnDefineScreen             = vmsvga3dBackDefineScreen;
                p->pfnDestroyScreen            = vmsvga3dBackDestroyScreen;
                p->pfnSurfaceBlitToScreen      = vmsvga3dBackSurfaceBlitToScreen;
                p->pfnSurfaceUpdateHeapBuffers = vmsvga3dBackSurfaceUpdateHeapBuffers;
            }
        }
        else
        {
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else
        rc = VERR_NOT_IMPLEMENTED;
    return rc;
}


extern VMSVGA3DBACKENDDESC const g_BackendDX =
{
    "DX",
    vmsvga3dBackQueryInterface
};
