/**********************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

/**
 * @file
 * Dump SVGA commands.
 *
 * Generated automatically from svga3d_reg.h by svga_dump.py.
 * Modified for VirtualBox.
 */

#include "svga_types.h"
#include "vmsvga_headers_begin.h"
#pragma pack(1) /* VMSVGA structures are '__packed'. */
#include "svga3d_reg.h"
#pragma pack()
#include "vmsvga_headers_end.h"

#include "svga_dump.h"

#define LOG_GROUP LOG_GROUP_DEV_VMSVGA
#include <VBox/log.h>

#define _debug_printf(...) Log7((__VA_ARGS__));

#ifdef LOG_ENABLED
static void
dump_SVGASignedRect(const SVGASignedRect *cmd)
{
   _debug_printf("\t\t.left = %i\n", (*cmd).left);
   _debug_printf("\t\t.top = %i\n", (*cmd).top);
   _debug_printf("\t\t.right = %i\n", (*cmd).right);
   _debug_printf("\t\t.bottom = %i\n", (*cmd).bottom);
}

static void
dump_SVGA3dCopyRect(const SVGA3dCopyRect *cmd)
{
   _debug_printf("\t\t.x = %u\n", (*cmd).x);
   _debug_printf("\t\t.y = %u\n", (*cmd).y);
   _debug_printf("\t\t.w = %u\n", (*cmd).w);
   _debug_printf("\t\t.h = %u\n", (*cmd).h);
   _debug_printf("\t\t.srcx = %u\n", (*cmd).srcx);
   _debug_printf("\t\t.srcy = %u\n", (*cmd).srcy);
}

static void
dump_SVGA3dCopyBox(const SVGA3dCopyBox *cmd)
{
   _debug_printf("\t\t.x = %u\n", (*cmd).x);
   _debug_printf("\t\t.y = %u\n", (*cmd).y);
   _debug_printf("\t\t.z = %u\n", (*cmd).z);
   _debug_printf("\t\t.w = %u\n", (*cmd).w);
   _debug_printf("\t\t.h = %u\n", (*cmd).h);
   _debug_printf("\t\t.d = %u\n", (*cmd).d);
   _debug_printf("\t\t.srcx = %u\n", (*cmd).srcx);
   _debug_printf("\t\t.srcy = %u\n", (*cmd).srcy);
   _debug_printf("\t\t.srcz = %u\n", (*cmd).srcz);
}

static void
dump_SVGA3dRect(const SVGA3dRect *cmd)
{
   _debug_printf("\t\t.x = %u\n", (*cmd).x);
   _debug_printf("\t\t.y = %u\n", (*cmd).y);
   _debug_printf("\t\t.w = %u\n", (*cmd).w);
   _debug_printf("\t\t.h = %u\n", (*cmd).h);
}

static void
dump_SVGA3dVertexDivisor(const SVGA3dVertexDivisor *cmd)
{
   _debug_printf("\t\t.count = %u\n", (*cmd).count);
   _debug_printf("\t\t.indexedData = %u\n", (*cmd).indexedData);
   _debug_printf("\t\t.instanceData = %u\n", (*cmd).instanceData);
   _debug_printf("\t\t.value = %u\n", (*cmd).value);
}

static void
dump_SVGA3dSize(const SVGA3dSize *cmd)
{
   _debug_printf("\t\t.width = %u\n", (*cmd).width);
   _debug_printf("\t\t.height = %u\n", (*cmd).height);
   _debug_printf("\t\t.depth = %u\n", (*cmd).depth);
}

static void
dump_SVGA3dCmdDefineSurface(const SVGA3dCmdDefineSurface *cmd)
{
   _debug_printf("\t\t.sid = %u\n", (*cmd).sid);
   _debug_printf("\t\t.surfaceFlags = %u\n", (*cmd).surfaceFlags);
   switch((*cmd).format) {
   case SVGA3D_FORMAT_INVALID:
      _debug_printf("\t\t.format = SVGA3D_FORMAT_INVALID\n");
      break;
   case SVGA3D_X8R8G8B8:
      _debug_printf("\t\t.format = SVGA3D_X8R8G8B8\n");
      break;
   case SVGA3D_A8R8G8B8:
      _debug_printf("\t\t.format = SVGA3D_A8R8G8B8\n");
      break;
   case SVGA3D_R5G6B5:
      _debug_printf("\t\t.format = SVGA3D_R5G6B5\n");
      break;
   case SVGA3D_X1R5G5B5:
      _debug_printf("\t\t.format = SVGA3D_X1R5G5B5\n");
      break;
   case SVGA3D_A1R5G5B5:
      _debug_printf("\t\t.format = SVGA3D_A1R5G5B5\n");
      break;
   case SVGA3D_A4R4G4B4:
      _debug_printf("\t\t.format = SVGA3D_A4R4G4B4\n");
      break;
   case SVGA3D_Z_D32:
      _debug_printf("\t\t.format = SVGA3D_Z_D32\n");
      break;
   case SVGA3D_Z_D16:
      _debug_printf("\t\t.format = SVGA3D_Z_D16\n");
      break;
   case SVGA3D_Z_D24S8:
      _debug_printf("\t\t.format = SVGA3D_Z_D24S8\n");
      break;
   case SVGA3D_Z_D15S1:
      _debug_printf("\t\t.format = SVGA3D_Z_D15S1\n");
      break;
   case SVGA3D_LUMINANCE8:
      _debug_printf("\t\t.format = SVGA3D_LUMINANCE8\n");
      break;
   case SVGA3D_LUMINANCE4_ALPHA4:
      _debug_printf("\t\t.format = SVGA3D_LUMINANCE4_ALPHA4\n");
      break;
   case SVGA3D_LUMINANCE16:
      _debug_printf("\t\t.format = SVGA3D_LUMINANCE16\n");
      break;
   case SVGA3D_LUMINANCE8_ALPHA8:
      _debug_printf("\t\t.format = SVGA3D_LUMINANCE8_ALPHA8\n");
      break;
   case SVGA3D_DXT1:
      _debug_printf("\t\t.format = SVGA3D_DXT1\n");
      break;
   case SVGA3D_DXT2:
      _debug_printf("\t\t.format = SVGA3D_DXT2\n");
      break;
   case SVGA3D_DXT3:
      _debug_printf("\t\t.format = SVGA3D_DXT3\n");
      break;
   case SVGA3D_DXT4:
      _debug_printf("\t\t.format = SVGA3D_DXT4\n");
      break;
   case SVGA3D_DXT5:
      _debug_printf("\t\t.format = SVGA3D_DXT5\n");
      break;
   case SVGA3D_BUMPU8V8:
      _debug_printf("\t\t.format = SVGA3D_BUMPU8V8\n");
      break;
   case SVGA3D_BUMPL6V5U5:
      _debug_printf("\t\t.format = SVGA3D_BUMPL6V5U5\n");
      break;
   case SVGA3D_BUMPX8L8V8U8:
      _debug_printf("\t\t.format = SVGA3D_BUMPX8L8V8U8\n");
      break;
   case SVGA3D_FORMAT_DEAD1:
      _debug_printf("\t\t.format = SVGA3D_FORMAT_DEAD1\n");
      break;
   case SVGA3D_ARGB_S10E5:
      _debug_printf("\t\t.format = SVGA3D_ARGB_S10E5\n");
      break;
   case SVGA3D_ARGB_S23E8:
      _debug_printf("\t\t.format = SVGA3D_ARGB_S23E8\n");
      break;
   case SVGA3D_A2R10G10B10:
      _debug_printf("\t\t.format = SVGA3D_A2R10G10B10\n");
      break;
   case SVGA3D_V8U8:
      _debug_printf("\t\t.format = SVGA3D_V8U8\n");
      break;
   case SVGA3D_Q8W8V8U8:
      _debug_printf("\t\t.format = SVGA3D_Q8W8V8U8\n");
      break;
   case SVGA3D_CxV8U8:
      _debug_printf("\t\t.format = SVGA3D_CxV8U8\n");
      break;
   case SVGA3D_X8L8V8U8:
      _debug_printf("\t\t.format = SVGA3D_X8L8V8U8\n");
      break;
   case SVGA3D_A2W10V10U10:
      _debug_printf("\t\t.format = SVGA3D_A2W10V10U10\n");
      break;
   case SVGA3D_ALPHA8:
      _debug_printf("\t\t.format = SVGA3D_ALPHA8\n");
      break;
   case SVGA3D_R_S10E5:
      _debug_printf("\t\t.format = SVGA3D_R_S10E5\n");
      break;
   case SVGA3D_R_S23E8:
      _debug_printf("\t\t.format = SVGA3D_R_S23E8\n");
      break;
   case SVGA3D_RG_S10E5:
      _debug_printf("\t\t.format = SVGA3D_RG_S10E5\n");
      break;
   case SVGA3D_RG_S23E8:
      _debug_printf("\t\t.format = SVGA3D_RG_S23E8\n");
      break;
   case SVGA3D_BUFFER:
      _debug_printf("\t\t.format = SVGA3D_BUFFER\n");
      break;
   case SVGA3D_Z_D24X8:
      _debug_printf("\t\t.format = SVGA3D_Z_D24X8\n");
      break;
   case SVGA3D_V16U16:
      _debug_printf("\t\t.format = SVGA3D_V16U16\n");
      break;
   case SVGA3D_G16R16:
      _debug_printf("\t\t.format = SVGA3D_G16R16\n");
      break;
   case SVGA3D_A16B16G16R16:
      _debug_printf("\t\t.format = SVGA3D_A16B16G16R16\n");
      break;
   case SVGA3D_UYVY:
      _debug_printf("\t\t.format = SVGA3D_UYVY\n");
      break;
   case SVGA3D_YUY2:
      _debug_printf("\t\t.format = SVGA3D_YUY2\n");
      break;
   case SVGA3D_NV12:
      _debug_printf("\t\t.format = SVGA3D_NV12\n");
      break;
   case SVGA3D_FORMAT_DEAD2:
      _debug_printf("\t\t.format = SVGA3D_FORMAT_DEAD2\n");
      break;
   case SVGA3D_R32G32B32A32_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32A32_TYPELESS\n");
      break;
   case SVGA3D_R32G32B32A32_UINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32A32_UINT\n");
      break;
   case SVGA3D_R32G32B32A32_SINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32A32_SINT\n");
      break;
   case SVGA3D_R32G32B32_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32_TYPELESS\n");
      break;
   case SVGA3D_R32G32B32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32_FLOAT\n");
      break;
   case SVGA3D_R32G32B32_UINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32_UINT\n");
      break;
   case SVGA3D_R32G32B32_SINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32_SINT\n");
      break;
   case SVGA3D_R16G16B16A16_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_TYPELESS\n");
      break;
   case SVGA3D_R16G16B16A16_UINT:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_UINT\n");
      break;
   case SVGA3D_R16G16B16A16_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_SNORM\n");
      break;
   case SVGA3D_R16G16B16A16_SINT:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_SINT\n");
      break;
   case SVGA3D_R32G32_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32G32_TYPELESS\n");
      break;
   case SVGA3D_R32G32_UINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32_UINT\n");
      break;
   case SVGA3D_R32G32_SINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32_SINT\n");
      break;
   case SVGA3D_R32G8X24_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32G8X24_TYPELESS\n");
      break;
   case SVGA3D_D32_FLOAT_S8X24_UINT:
      _debug_printf("\t\t.format = SVGA3D_D32_FLOAT_S8X24_UINT\n");
      break;
   case SVGA3D_R32_FLOAT_X8X24:
      _debug_printf("\t\t.format = SVGA3D_R32_FLOAT_X8X24\n");
      break;
   case SVGA3D_X32_G8X24_UINT:
      _debug_printf("\t\t.format = SVGA3D_X32_G8X24_UINT\n");
      break;
   case SVGA3D_R10G10B10A2_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R10G10B10A2_TYPELESS\n");
      break;
   case SVGA3D_R10G10B10A2_UINT:
      _debug_printf("\t\t.format = SVGA3D_R10G10B10A2_UINT\n");
      break;
   case SVGA3D_R11G11B10_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R11G11B10_FLOAT\n");
      break;
   case SVGA3D_R8G8B8A8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_TYPELESS\n");
      break;
   case SVGA3D_R8G8B8A8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_UNORM\n");
      break;
   case SVGA3D_R8G8B8A8_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_UNORM_SRGB\n");
      break;
   case SVGA3D_R8G8B8A8_UINT:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_UINT\n");
      break;
   case SVGA3D_R8G8B8A8_SINT:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_SINT\n");
      break;
   case SVGA3D_R16G16_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R16G16_TYPELESS\n");
      break;
   case SVGA3D_R16G16_UINT:
      _debug_printf("\t\t.format = SVGA3D_R16G16_UINT\n");
      break;
   case SVGA3D_R16G16_SINT:
      _debug_printf("\t\t.format = SVGA3D_R16G16_SINT\n");
      break;
   case SVGA3D_R32_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32_TYPELESS\n");
      break;
   case SVGA3D_D32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_D32_FLOAT\n");
      break;
   case SVGA3D_R32_UINT:
      _debug_printf("\t\t.format = SVGA3D_R32_UINT\n");
      break;
   case SVGA3D_R32_SINT:
      _debug_printf("\t\t.format = SVGA3D_R32_SINT\n");
      break;
   case SVGA3D_R24G8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R24G8_TYPELESS\n");
      break;
   case SVGA3D_D24_UNORM_S8_UINT:
      _debug_printf("\t\t.format = SVGA3D_D24_UNORM_S8_UINT\n");
      break;
   case SVGA3D_R24_UNORM_X8:
      _debug_printf("\t\t.format = SVGA3D_R24_UNORM_X8\n");
      break;
   case SVGA3D_X24_G8_UINT:
      _debug_printf("\t\t.format = SVGA3D_X24_G8_UINT\n");
      break;
   case SVGA3D_R8G8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R8G8_TYPELESS\n");
      break;
   case SVGA3D_R8G8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8_UNORM\n");
      break;
   case SVGA3D_R8G8_UINT:
      _debug_printf("\t\t.format = SVGA3D_R8G8_UINT\n");
      break;
   case SVGA3D_R8G8_SINT:
      _debug_printf("\t\t.format = SVGA3D_R8G8_SINT\n");
      break;
   case SVGA3D_R16_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R16_TYPELESS\n");
      break;
   case SVGA3D_R16_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R16_UNORM\n");
      break;
   case SVGA3D_R16_UINT:
      _debug_printf("\t\t.format = SVGA3D_R16_UINT\n");
      break;
   case SVGA3D_R16_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R16_SNORM\n");
      break;
   case SVGA3D_R16_SINT:
      _debug_printf("\t\t.format = SVGA3D_R16_SINT\n");
      break;
   case SVGA3D_R8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R8_TYPELESS\n");
      break;
   case SVGA3D_R8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R8_UNORM\n");
      break;
   case SVGA3D_R8_UINT:
      _debug_printf("\t\t.format = SVGA3D_R8_UINT\n");
      break;
   case SVGA3D_R8_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R8_SNORM\n");
      break;
   case SVGA3D_R8_SINT:
      _debug_printf("\t\t.format = SVGA3D_R8_SINT\n");
      break;
   case SVGA3D_P8:
      _debug_printf("\t\t.format = SVGA3D_P8\n");
      break;
   case SVGA3D_R9G9B9E5_SHAREDEXP:
      _debug_printf("\t\t.format = SVGA3D_R9G9B9E5_SHAREDEXP\n");
      break;
   case SVGA3D_R8G8_B8G8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8_B8G8_UNORM\n");
      break;
   case SVGA3D_G8R8_G8B8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_G8R8_G8B8_UNORM\n");
      break;
   case SVGA3D_BC1_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC1_TYPELESS\n");
      break;
   case SVGA3D_BC1_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_BC1_UNORM_SRGB\n");
      break;
   case SVGA3D_BC2_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC2_TYPELESS\n");
      break;
   case SVGA3D_BC2_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_BC2_UNORM_SRGB\n");
      break;
   case SVGA3D_BC3_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC3_TYPELESS\n");
      break;
   case SVGA3D_BC3_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_BC3_UNORM_SRGB\n");
      break;
   case SVGA3D_BC4_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC4_TYPELESS\n");
      break;
   case SVGA3D_ATI1:
      _debug_printf("\t\t.format = SVGA3D_ATI1\n");
      break;
   case SVGA3D_BC4_SNORM:
      _debug_printf("\t\t.format = SVGA3D_BC4_SNORM\n");
      break;
   case SVGA3D_BC5_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC5_TYPELESS\n");
      break;
   case SVGA3D_ATI2:
      _debug_printf("\t\t.format = SVGA3D_ATI2\n");
      break;
   case SVGA3D_BC5_SNORM:
      _debug_printf("\t\t.format = SVGA3D_BC5_SNORM\n");
      break;
   case SVGA3D_R10G10B10_XR_BIAS_A2_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R10G10B10_XR_BIAS_A2_UNORM\n");
      break;
   case SVGA3D_B8G8R8A8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8A8_TYPELESS\n");
      break;
   case SVGA3D_B8G8R8A8_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8A8_UNORM_SRGB\n");
      break;
   case SVGA3D_B8G8R8X8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8X8_TYPELESS\n");
      break;
   case SVGA3D_B8G8R8X8_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8X8_UNORM_SRGB\n");
      break;
   case SVGA3D_Z_DF16:
      _debug_printf("\t\t.format = SVGA3D_Z_DF16\n");
      break;
   case SVGA3D_Z_DF24:
      _debug_printf("\t\t.format = SVGA3D_Z_DF24\n");
      break;
   case SVGA3D_Z_D24S8_INT:
      _debug_printf("\t\t.format = SVGA3D_Z_D24S8_INT\n");
      break;
   case SVGA3D_YV12:
      _debug_printf("\t\t.format = SVGA3D_YV12\n");
      break;
   case SVGA3D_R32G32B32A32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32A32_FLOAT\n");
      break;
   case SVGA3D_R16G16B16A16_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_FLOAT\n");
      break;
   case SVGA3D_R16G16B16A16_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_UNORM\n");
      break;
   case SVGA3D_R32G32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R32G32_FLOAT\n");
      break;
   case SVGA3D_R10G10B10A2_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R10G10B10A2_UNORM\n");
      break;
   case SVGA3D_R8G8B8A8_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_SNORM\n");
      break;
   case SVGA3D_R16G16_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R16G16_FLOAT\n");
      break;
   case SVGA3D_R16G16_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R16G16_UNORM\n");
      break;
   case SVGA3D_R16G16_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R16G16_SNORM\n");
      break;
   case SVGA3D_R32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R32_FLOAT\n");
      break;
   case SVGA3D_R8G8_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8_SNORM\n");
      break;
   case SVGA3D_R16_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R16_FLOAT\n");
      break;
   case SVGA3D_D16_UNORM:
      _debug_printf("\t\t.format = SVGA3D_D16_UNORM\n");
      break;
   case SVGA3D_A8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_A8_UNORM\n");
      break;
   case SVGA3D_BC1_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC1_UNORM\n");
      break;
   case SVGA3D_BC2_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC2_UNORM\n");
      break;
   case SVGA3D_BC3_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC3_UNORM\n");
      break;
   case SVGA3D_B5G6R5_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B5G6R5_UNORM\n");
      break;
   case SVGA3D_B5G5R5A1_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B5G5R5A1_UNORM\n");
      break;
   case SVGA3D_B8G8R8A8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8A8_UNORM\n");
      break;
   case SVGA3D_B8G8R8X8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8X8_UNORM\n");
      break;
   case SVGA3D_BC4_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC4_UNORM\n");
      break;
   case SVGA3D_BC5_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC5_UNORM\n");
      break;
   case SVGA3D_B4G4R4A4_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B4G4R4A4_UNORM\n");
      break;
   case SVGA3D_BC6H_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC6H_TYPELESS\n");
      break;
   case SVGA3D_BC6H_UF16:
      _debug_printf("\t\t.format = SVGA3D_BC6H_UF16\n");
      break;
   case SVGA3D_BC6H_SF16:
      _debug_printf("\t\t.format = SVGA3D_BC6H_SF16\n");
      break;
   case SVGA3D_BC7_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC7_TYPELESS\n");
      break;
   case SVGA3D_BC7_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC7_UNORM\n");
      break;
   case SVGA3D_BC7_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_BC7_UNORM_SRGB\n");
      break;
   case SVGA3D_AYUV:
      _debug_printf("\t\t.format = SVGA3D_AYUV\n");
      break;
   default:
      _debug_printf("\t\t.format = %i\n", (*cmd).format);
      break;
   }
   _debug_printf("\t\t.face[0].numMipLevels = %u\n", (*cmd).face[0].numMipLevels);
   _debug_printf("\t\t.face[1].numMipLevels = %u\n", (*cmd).face[1].numMipLevels);
   _debug_printf("\t\t.face[2].numMipLevels = %u\n", (*cmd).face[2].numMipLevels);
   _debug_printf("\t\t.face[3].numMipLevels = %u\n", (*cmd).face[3].numMipLevels);
   _debug_printf("\t\t.face[4].numMipLevels = %u\n", (*cmd).face[4].numMipLevels);
   _debug_printf("\t\t.face[5].numMipLevels = %u\n", (*cmd).face[5].numMipLevels);
}

static void
dump_SVGA3dCmdDestroySurface(const SVGA3dCmdDestroySurface *cmd)
{
   _debug_printf("\t\t.sid = %u\n", (*cmd).sid);
}

static void
dump_SVGA3dCmdDefineContext(const SVGA3dCmdDefineContext *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
}

static void
dump_SVGA3dCmdDestroyContext(const SVGA3dCmdDestroyContext *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
}

static void
dump_SVGA3dCmdClear(const SVGA3dCmdClear *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   switch((*cmd).clearFlag) {
   case SVGA3D_CLEAR_COLOR:
      _debug_printf("\t\t.clearFlag = SVGA3D_CLEAR_COLOR\n");
      break;
   case SVGA3D_CLEAR_DEPTH:
      _debug_printf("\t\t.clearFlag = SVGA3D_CLEAR_DEPTH\n");
      break;
   case SVGA3D_CLEAR_STENCIL:
      _debug_printf("\t\t.clearFlag = SVGA3D_CLEAR_STENCIL\n");
      break;
   case SVGA3D_CLEAR_COLORFILL:
      _debug_printf("\t\t.clearFlag = SVGA3D_CLEAR_COLORFILL\n");
      break;
   default:
      _debug_printf("\t\t.clearFlag = %i\n", (*cmd).clearFlag);
      break;
   }
   _debug_printf("\t\t.color = %u\n", (*cmd).color);
   _debug_printf("\t\t.depth = %f\n", (*cmd).depth);
   _debug_printf("\t\t.stencil = %u\n", (*cmd).stencil);
}

static void
dump_SVGA3dCmdPresent(const SVGA3dCmdPresent *cmd)
{
   _debug_printf("\t\t.sid = %u\n", (*cmd).sid);
}

static void
dump_SVGA3dRenderState(const SVGA3dRenderState *cmd)
{
   switch((*cmd).state) {
   case SVGA3D_RS_INVALID:
      _debug_printf("\t\t.state = SVGA3D_RS_INVALID\n");
      break;
   case SVGA3D_RS_ZENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_ZENABLE\n");
      break;
   case SVGA3D_RS_ZWRITEENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_ZWRITEENABLE\n");
      break;
   case SVGA3D_RS_ALPHATESTENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_ALPHATESTENABLE\n");
      break;
   case SVGA3D_RS_DITHERENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_DITHERENABLE\n");
      break;
   case SVGA3D_RS_BLENDENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_BLENDENABLE\n");
      break;
   case SVGA3D_RS_FOGENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_FOGENABLE\n");
      break;
   case SVGA3D_RS_SPECULARENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_SPECULARENABLE\n");
      break;
   case SVGA3D_RS_STENCILENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILENABLE\n");
      break;
   case SVGA3D_RS_LIGHTINGENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_LIGHTINGENABLE\n");
      break;
   case SVGA3D_RS_NORMALIZENORMALS:
      _debug_printf("\t\t.state = SVGA3D_RS_NORMALIZENORMALS\n");
      break;
   case SVGA3D_RS_POINTSPRITEENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_POINTSPRITEENABLE\n");
      break;
   case SVGA3D_RS_POINTSCALEENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_POINTSCALEENABLE\n");
      break;
   case SVGA3D_RS_STENCILREF:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILREF\n");
      break;
   case SVGA3D_RS_STENCILMASK:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILMASK\n");
      break;
   case SVGA3D_RS_STENCILWRITEMASK:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILWRITEMASK\n");
      break;
   case SVGA3D_RS_FOGSTART:
      _debug_printf("\t\t.state = SVGA3D_RS_FOGSTART\n");
      break;
   case SVGA3D_RS_FOGEND:
      _debug_printf("\t\t.state = SVGA3D_RS_FOGEND\n");
      break;
   case SVGA3D_RS_FOGDENSITY:
      _debug_printf("\t\t.state = SVGA3D_RS_FOGDENSITY\n");
      break;
   case SVGA3D_RS_POINTSIZE:
      _debug_printf("\t\t.state = SVGA3D_RS_POINTSIZE\n");
      break;
   case SVGA3D_RS_POINTSIZEMIN:
      _debug_printf("\t\t.state = SVGA3D_RS_POINTSIZEMIN\n");
      break;
   case SVGA3D_RS_POINTSIZEMAX:
      _debug_printf("\t\t.state = SVGA3D_RS_POINTSIZEMAX\n");
      break;
   case SVGA3D_RS_POINTSCALE_A:
      _debug_printf("\t\t.state = SVGA3D_RS_POINTSCALE_A\n");
      break;
   case SVGA3D_RS_POINTSCALE_B:
      _debug_printf("\t\t.state = SVGA3D_RS_POINTSCALE_B\n");
      break;
   case SVGA3D_RS_POINTSCALE_C:
      _debug_printf("\t\t.state = SVGA3D_RS_POINTSCALE_C\n");
      break;
   case SVGA3D_RS_FOGCOLOR:
      _debug_printf("\t\t.state = SVGA3D_RS_FOGCOLOR\n");
      break;
   case SVGA3D_RS_AMBIENT:
      _debug_printf("\t\t.state = SVGA3D_RS_AMBIENT\n");
      break;
   case SVGA3D_RS_CLIPPLANEENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_CLIPPLANEENABLE\n");
      break;
   case SVGA3D_RS_FOGMODE:
      _debug_printf("\t\t.state = SVGA3D_RS_FOGMODE\n");
      break;
   case SVGA3D_RS_FILLMODE:
      _debug_printf("\t\t.state = SVGA3D_RS_FILLMODE\n");
      break;
   case SVGA3D_RS_SHADEMODE:
      _debug_printf("\t\t.state = SVGA3D_RS_SHADEMODE\n");
      break;
   case SVGA3D_RS_LINEPATTERN:
      _debug_printf("\t\t.state = SVGA3D_RS_LINEPATTERN\n");
      break;
   case SVGA3D_RS_SRCBLEND:
      _debug_printf("\t\t.state = SVGA3D_RS_SRCBLEND\n");
      break;
   case SVGA3D_RS_DSTBLEND:
      _debug_printf("\t\t.state = SVGA3D_RS_DSTBLEND\n");
      break;
   case SVGA3D_RS_BLENDEQUATION:
      _debug_printf("\t\t.state = SVGA3D_RS_BLENDEQUATION\n");
      break;
   case SVGA3D_RS_CULLMODE:
      _debug_printf("\t\t.state = SVGA3D_RS_CULLMODE\n");
      break;
   case SVGA3D_RS_ZFUNC:
      _debug_printf("\t\t.state = SVGA3D_RS_ZFUNC\n");
      break;
   case SVGA3D_RS_ALPHAFUNC:
      _debug_printf("\t\t.state = SVGA3D_RS_ALPHAFUNC\n");
      break;
   case SVGA3D_RS_STENCILFUNC:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILFUNC\n");
      break;
   case SVGA3D_RS_STENCILFAIL:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILFAIL\n");
      break;
   case SVGA3D_RS_STENCILZFAIL:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILZFAIL\n");
      break;
   case SVGA3D_RS_STENCILPASS:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILPASS\n");
      break;
   case SVGA3D_RS_ALPHAREF:
      _debug_printf("\t\t.state = SVGA3D_RS_ALPHAREF\n");
      break;
   case SVGA3D_RS_FRONTWINDING:
      _debug_printf("\t\t.state = SVGA3D_RS_FRONTWINDING\n");
      break;
   case SVGA3D_RS_COORDINATETYPE:
      _debug_printf("\t\t.state = SVGA3D_RS_COORDINATETYPE\n");
      break;
   case SVGA3D_RS_ZBIAS:
      _debug_printf("\t\t.state = SVGA3D_RS_ZBIAS\n");
      break;
   case SVGA3D_RS_RANGEFOGENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_RANGEFOGENABLE\n");
      break;
   case SVGA3D_RS_COLORWRITEENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_COLORWRITEENABLE\n");
      break;
   case SVGA3D_RS_VERTEXMATERIALENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_VERTEXMATERIALENABLE\n");
      break;
   case SVGA3D_RS_DIFFUSEMATERIALSOURCE:
      _debug_printf("\t\t.state = SVGA3D_RS_DIFFUSEMATERIALSOURCE\n");
      break;
   case SVGA3D_RS_SPECULARMATERIALSOURCE:
      _debug_printf("\t\t.state = SVGA3D_RS_SPECULARMATERIALSOURCE\n");
      break;
   case SVGA3D_RS_AMBIENTMATERIALSOURCE:
      _debug_printf("\t\t.state = SVGA3D_RS_AMBIENTMATERIALSOURCE\n");
      break;
   case SVGA3D_RS_EMISSIVEMATERIALSOURCE:
      _debug_printf("\t\t.state = SVGA3D_RS_EMISSIVEMATERIALSOURCE\n");
      break;
   case SVGA3D_RS_TEXTUREFACTOR:
      _debug_printf("\t\t.state = SVGA3D_RS_TEXTUREFACTOR\n");
      break;
   case SVGA3D_RS_LOCALVIEWER:
      _debug_printf("\t\t.state = SVGA3D_RS_LOCALVIEWER\n");
      break;
   case SVGA3D_RS_SCISSORTESTENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_SCISSORTESTENABLE\n");
      break;
   case SVGA3D_RS_BLENDCOLOR:
      _debug_printf("\t\t.state = SVGA3D_RS_BLENDCOLOR\n");
      break;
   case SVGA3D_RS_STENCILENABLE2SIDED:
      _debug_printf("\t\t.state = SVGA3D_RS_STENCILENABLE2SIDED\n");
      break;
   case SVGA3D_RS_CCWSTENCILFUNC:
      _debug_printf("\t\t.state = SVGA3D_RS_CCWSTENCILFUNC\n");
      break;
   case SVGA3D_RS_CCWSTENCILFAIL:
      _debug_printf("\t\t.state = SVGA3D_RS_CCWSTENCILFAIL\n");
      break;
   case SVGA3D_RS_CCWSTENCILZFAIL:
      _debug_printf("\t\t.state = SVGA3D_RS_CCWSTENCILZFAIL\n");
      break;
   case SVGA3D_RS_CCWSTENCILPASS:
      _debug_printf("\t\t.state = SVGA3D_RS_CCWSTENCILPASS\n");
      break;
   case SVGA3D_RS_VERTEXBLEND:
      _debug_printf("\t\t.state = SVGA3D_RS_VERTEXBLEND\n");
      break;
   case SVGA3D_RS_SLOPESCALEDEPTHBIAS:
      _debug_printf("\t\t.state = SVGA3D_RS_SLOPESCALEDEPTHBIAS\n");
      break;
   case SVGA3D_RS_DEPTHBIAS:
      _debug_printf("\t\t.state = SVGA3D_RS_DEPTHBIAS\n");
      break;
   case SVGA3D_RS_OUTPUTGAMMA:
      _debug_printf("\t\t.state = SVGA3D_RS_OUTPUTGAMMA\n");
      break;
   case SVGA3D_RS_ZVISIBLE:
      _debug_printf("\t\t.state = SVGA3D_RS_ZVISIBLE\n");
      break;
   case SVGA3D_RS_LASTPIXEL:
      _debug_printf("\t\t.state = SVGA3D_RS_LASTPIXEL\n");
      break;
   case SVGA3D_RS_CLIPPING:
      _debug_printf("\t\t.state = SVGA3D_RS_CLIPPING\n");
      break;
   case SVGA3D_RS_WRAP0:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP0\n");
      break;
   case SVGA3D_RS_WRAP1:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP1\n");
      break;
   case SVGA3D_RS_WRAP2:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP2\n");
      break;
   case SVGA3D_RS_WRAP3:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP3\n");
      break;
   case SVGA3D_RS_WRAP4:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP4\n");
      break;
   case SVGA3D_RS_WRAP5:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP5\n");
      break;
   case SVGA3D_RS_WRAP6:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP6\n");
      break;
   case SVGA3D_RS_WRAP7:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP7\n");
      break;
   case SVGA3D_RS_WRAP8:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP8\n");
      break;
   case SVGA3D_RS_WRAP9:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP9\n");
      break;
   case SVGA3D_RS_WRAP10:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP10\n");
      break;
   case SVGA3D_RS_WRAP11:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP11\n");
      break;
   case SVGA3D_RS_WRAP12:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP12\n");
      break;
   case SVGA3D_RS_WRAP13:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP13\n");
      break;
   case SVGA3D_RS_WRAP14:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP14\n");
      break;
   case SVGA3D_RS_WRAP15:
      _debug_printf("\t\t.state = SVGA3D_RS_WRAP15\n");
      break;
   case SVGA3D_RS_MULTISAMPLEANTIALIAS:
      _debug_printf("\t\t.state = SVGA3D_RS_MULTISAMPLEANTIALIAS\n");
      break;
   case SVGA3D_RS_MULTISAMPLEMASK:
      _debug_printf("\t\t.state = SVGA3D_RS_MULTISAMPLEMASK\n");
      break;
   case SVGA3D_RS_INDEXEDVERTEXBLENDENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_INDEXEDVERTEXBLENDENABLE\n");
      break;
   case SVGA3D_RS_TWEENFACTOR:
      _debug_printf("\t\t.state = SVGA3D_RS_TWEENFACTOR\n");
      break;
   case SVGA3D_RS_ANTIALIASEDLINEENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_ANTIALIASEDLINEENABLE\n");
      break;
   case SVGA3D_RS_COLORWRITEENABLE1:
      _debug_printf("\t\t.state = SVGA3D_RS_COLORWRITEENABLE1\n");
      break;
   case SVGA3D_RS_COLORWRITEENABLE2:
      _debug_printf("\t\t.state = SVGA3D_RS_COLORWRITEENABLE2\n");
      break;
   case SVGA3D_RS_COLORWRITEENABLE3:
      _debug_printf("\t\t.state = SVGA3D_RS_COLORWRITEENABLE3\n");
      break;
   case SVGA3D_RS_SEPARATEALPHABLENDENABLE:
      _debug_printf("\t\t.state = SVGA3D_RS_SEPARATEALPHABLENDENABLE\n");
      break;
   case SVGA3D_RS_SRCBLENDALPHA:
      _debug_printf("\t\t.state = SVGA3D_RS_SRCBLENDALPHA\n");
      break;
   case SVGA3D_RS_DSTBLENDALPHA:
      _debug_printf("\t\t.state = SVGA3D_RS_DSTBLENDALPHA\n");
      break;
   case SVGA3D_RS_BLENDEQUATIONALPHA:
      _debug_printf("\t\t.state = SVGA3D_RS_BLENDEQUATIONALPHA\n");
      break;
   case SVGA3D_RS_TRANSPARENCYANTIALIAS:
      _debug_printf("\t\t.state = SVGA3D_RS_TRANSPARENCYANTIALIAS\n");
      break;
   case SVGA3D_RS_LINEWIDTH:
      _debug_printf("\t\t.state = SVGA3D_RS_LINEWIDTH\n");
      break;
   default:
      _debug_printf("\t\t.state = %i\n", (*cmd).state);
      break;
   }
   _debug_printf("\t\t.uintValue = %u\n", (*cmd).uintValue);
   _debug_printf("\t\t.floatValue = %f\n", (*cmd).floatValue);
}

static void
dump_SVGA3dCmdSetRenderState(const SVGA3dCmdSetRenderState *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
}

static void
dump_SVGA3dCmdSetRenderTarget(const SVGA3dCmdSetRenderTarget *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   switch((*cmd).type) {
   case SVGA3D_RT_DEPTH:
      _debug_printf("\t\t.type = SVGA3D_RT_DEPTH\n");
      break;
   case SVGA3D_RT_STENCIL:
      _debug_printf("\t\t.type = SVGA3D_RT_STENCIL\n");
      break;
   case SVGA3D_RT_COLOR0:
      _debug_printf("\t\t.type = SVGA3D_RT_COLOR0\n");
      break;
   case SVGA3D_RT_COLOR1:
      _debug_printf("\t\t.type = SVGA3D_RT_COLOR1\n");
      break;
   case SVGA3D_RT_COLOR2:
      _debug_printf("\t\t.type = SVGA3D_RT_COLOR2\n");
      break;
   case SVGA3D_RT_COLOR3:
      _debug_printf("\t\t.type = SVGA3D_RT_COLOR3\n");
      break;
   case SVGA3D_RT_COLOR4:
      _debug_printf("\t\t.type = SVGA3D_RT_COLOR4\n");
      break;
   case SVGA3D_RT_COLOR5:
      _debug_printf("\t\t.type = SVGA3D_RT_COLOR5\n");
      break;
   case SVGA3D_RT_COLOR6:
      _debug_printf("\t\t.type = SVGA3D_RT_COLOR6\n");
      break;
   case SVGA3D_RT_COLOR7:
      _debug_printf("\t\t.type = SVGA3D_RT_COLOR7\n");
      break;
   case SVGA3D_RT_INVALID:
      _debug_printf("\t\t.type = SVGA3D_RT_INVALID\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
   _debug_printf("\t\t.target.sid = %u\n", (*cmd).target.sid);
   _debug_printf("\t\t.target.face = %u\n", (*cmd).target.face);
   _debug_printf("\t\t.target.mipmap = %u\n", (*cmd).target.mipmap);
}

static void
dump_SVGA3dCmdSurfaceCopy(const SVGA3dCmdSurfaceCopy *cmd)
{
   _debug_printf("\t\t.src.sid = %u\n", (*cmd).src.sid);
   _debug_printf("\t\t.src.face = %u\n", (*cmd).src.face);
   _debug_printf("\t\t.src.mipmap = %u\n", (*cmd).src.mipmap);
   _debug_printf("\t\t.dest.sid = %u\n", (*cmd).dest.sid);
   _debug_printf("\t\t.dest.face = %u\n", (*cmd).dest.face);
   _debug_printf("\t\t.dest.mipmap = %u\n", (*cmd).dest.mipmap);
}

static void
dump_SVGA3dCmdSurfaceStretchBlt(const SVGA3dCmdSurfaceStretchBlt *cmd)
{
   _debug_printf("\t\t.src.sid = %u\n", (*cmd).src.sid);
   _debug_printf("\t\t.src.face = %u\n", (*cmd).src.face);
   _debug_printf("\t\t.src.mipmap = %u\n", (*cmd).src.mipmap);
   _debug_printf("\t\t.dest.sid = %u\n", (*cmd).dest.sid);
   _debug_printf("\t\t.dest.face = %u\n", (*cmd).dest.face);
   _debug_printf("\t\t.dest.mipmap = %u\n", (*cmd).dest.mipmap);
   _debug_printf("\t\t.boxSrc.x = %u\n", (*cmd).boxSrc.x);
   _debug_printf("\t\t.boxSrc.y = %u\n", (*cmd).boxSrc.y);
   _debug_printf("\t\t.boxSrc.z = %u\n", (*cmd).boxSrc.z);
   _debug_printf("\t\t.boxSrc.w = %u\n", (*cmd).boxSrc.w);
   _debug_printf("\t\t.boxSrc.h = %u\n", (*cmd).boxSrc.h);
   _debug_printf("\t\t.boxSrc.d = %u\n", (*cmd).boxSrc.d);
   _debug_printf("\t\t.boxDest.x = %u\n", (*cmd).boxDest.x);
   _debug_printf("\t\t.boxDest.y = %u\n", (*cmd).boxDest.y);
   _debug_printf("\t\t.boxDest.z = %u\n", (*cmd).boxDest.z);
   _debug_printf("\t\t.boxDest.w = %u\n", (*cmd).boxDest.w);
   _debug_printf("\t\t.boxDest.h = %u\n", (*cmd).boxDest.h);
   _debug_printf("\t\t.boxDest.d = %u\n", (*cmd).boxDest.d);
   switch((*cmd).mode) {
   case SVGA3D_STRETCH_BLT_POINT:
      _debug_printf("\t\t.mode = SVGA3D_STRETCH_BLT_POINT\n");
      break;
   case SVGA3D_STRETCH_BLT_LINEAR:
      _debug_printf("\t\t.mode = SVGA3D_STRETCH_BLT_LINEAR\n");
      break;
   default:
      _debug_printf("\t\t.mode = %i\n", (*cmd).mode);
      break;
   }
}

static void
dump_SVGA3dCmdSurfaceDMA(const SVGA3dCmdSurfaceDMA *cmd)
{
   _debug_printf("\t\t.guest.ptr.gmrId = %u\n", (*cmd).guest.ptr.gmrId);
   _debug_printf("\t\t.guest.ptr.offset = %u\n", (*cmd).guest.ptr.offset);
   _debug_printf("\t\t.guest.pitch = %u\n", (*cmd).guest.pitch);
   _debug_printf("\t\t.host.sid = %u\n", (*cmd).host.sid);
   _debug_printf("\t\t.host.face = %u\n", (*cmd).host.face);
   _debug_printf("\t\t.host.mipmap = %u\n", (*cmd).host.mipmap);
   switch((*cmd).transfer) {
   case SVGA3D_WRITE_HOST_VRAM:
      _debug_printf("\t\t.transfer = SVGA3D_WRITE_HOST_VRAM\n");
      break;
   case SVGA3D_READ_HOST_VRAM:
      _debug_printf("\t\t.transfer = SVGA3D_READ_HOST_VRAM\n");
      break;
   default:
      _debug_printf("\t\t.transfer = %i\n", (*cmd).transfer);
      break;
   }
}

static void
dump_SVGA3dVertexDecl(const SVGA3dVertexDecl *cmd)
{
   switch((*cmd).identity.type) {
   case SVGA3D_DECLTYPE_FLOAT1:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_FLOAT1\n");
      break;
   case SVGA3D_DECLTYPE_FLOAT2:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_FLOAT2\n");
      break;
   case SVGA3D_DECLTYPE_FLOAT3:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_FLOAT3\n");
      break;
   case SVGA3D_DECLTYPE_FLOAT4:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_FLOAT4\n");
      break;
   case SVGA3D_DECLTYPE_D3DCOLOR:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_D3DCOLOR\n");
      break;
   case SVGA3D_DECLTYPE_UBYTE4:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_UBYTE4\n");
      break;
   case SVGA3D_DECLTYPE_SHORT2:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_SHORT2\n");
      break;
   case SVGA3D_DECLTYPE_SHORT4:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_SHORT4\n");
      break;
   case SVGA3D_DECLTYPE_UBYTE4N:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_UBYTE4N\n");
      break;
   case SVGA3D_DECLTYPE_SHORT2N:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_SHORT2N\n");
      break;
   case SVGA3D_DECLTYPE_SHORT4N:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_SHORT4N\n");
      break;
   case SVGA3D_DECLTYPE_USHORT2N:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_USHORT2N\n");
      break;
   case SVGA3D_DECLTYPE_USHORT4N:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_USHORT4N\n");
      break;
   case SVGA3D_DECLTYPE_UDEC3:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_UDEC3\n");
      break;
   case SVGA3D_DECLTYPE_DEC3N:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_DEC3N\n");
      break;
   case SVGA3D_DECLTYPE_FLOAT16_2:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_FLOAT16_2\n");
      break;
   case SVGA3D_DECLTYPE_FLOAT16_4:
      _debug_printf("\t\t.identity.type = SVGA3D_DECLTYPE_FLOAT16_4\n");
      break;
   default:
      _debug_printf("\t\t.identity.type = %i\n", (*cmd).identity.type);
      break;
   }
   switch((*cmd).identity.method) {
   case SVGA3D_DECLMETHOD_DEFAULT:
      _debug_printf("\t\t.identity.method = SVGA3D_DECLMETHOD_DEFAULT\n");
      break;
   case SVGA3D_DECLMETHOD_PARTIALU:
      _debug_printf("\t\t.identity.method = SVGA3D_DECLMETHOD_PARTIALU\n");
      break;
   case SVGA3D_DECLMETHOD_PARTIALV:
      _debug_printf("\t\t.identity.method = SVGA3D_DECLMETHOD_PARTIALV\n");
      break;
   case SVGA3D_DECLMETHOD_CROSSUV:
      _debug_printf("\t\t.identity.method = SVGA3D_DECLMETHOD_CROSSUV\n");
      break;
   case SVGA3D_DECLMETHOD_UV:
      _debug_printf("\t\t.identity.method = SVGA3D_DECLMETHOD_UV\n");
      break;
   case SVGA3D_DECLMETHOD_LOOKUP:
      _debug_printf("\t\t.identity.method = SVGA3D_DECLMETHOD_LOOKUP\n");
      break;
   case SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED:
      _debug_printf("\t\t.identity.method = SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED\n");
      break;
   default:
      _debug_printf("\t\t.identity.method = %i\n", (*cmd).identity.method);
      break;
   }
   switch((*cmd).identity.usage) {
   case SVGA3D_DECLUSAGE_POSITION:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_POSITION\n");
      break;
   case SVGA3D_DECLUSAGE_BLENDWEIGHT:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_BLENDWEIGHT\n");
      break;
   case SVGA3D_DECLUSAGE_BLENDINDICES:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_BLENDINDICES\n");
      break;
   case SVGA3D_DECLUSAGE_NORMAL:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_NORMAL\n");
      break;
   case SVGA3D_DECLUSAGE_PSIZE:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_PSIZE\n");
      break;
   case SVGA3D_DECLUSAGE_TEXCOORD:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_TEXCOORD\n");
      break;
   case SVGA3D_DECLUSAGE_TANGENT:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_TANGENT\n");
      break;
   case SVGA3D_DECLUSAGE_BINORMAL:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_BINORMAL\n");
      break;
   case SVGA3D_DECLUSAGE_TESSFACTOR:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_TESSFACTOR\n");
      break;
   case SVGA3D_DECLUSAGE_POSITIONT:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_POSITIONT\n");
      break;
   case SVGA3D_DECLUSAGE_COLOR:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_COLOR\n");
      break;
   case SVGA3D_DECLUSAGE_FOG:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_FOG\n");
      break;
   case SVGA3D_DECLUSAGE_DEPTH:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_DEPTH\n");
      break;
   case SVGA3D_DECLUSAGE_SAMPLE:
      _debug_printf("\t\t.identity.usage = SVGA3D_DECLUSAGE_SAMPLE\n");
      break;
   default:
      _debug_printf("\t\t.identity.usage = %i\n", (*cmd).identity.usage);
      break;
   }
   _debug_printf("\t\t.identity.usageIndex = %u\n", (*cmd).identity.usageIndex);
   _debug_printf("\t\t.array.surfaceId = %u\n", (*cmd).array.surfaceId);
   _debug_printf("\t\t.array.offset = %u\n", (*cmd).array.offset);
   _debug_printf("\t\t.array.stride = %u\n", (*cmd).array.stride);
   _debug_printf("\t\t.rangeHint.first = %u\n", (*cmd).rangeHint.first);
   _debug_printf("\t\t.rangeHint.last = %u\n", (*cmd).rangeHint.last);
}

static void
dump_SVGA3dPrimitiveRange(const SVGA3dPrimitiveRange *cmd)
{
   switch((*cmd).primType) {
   case SVGA3D_PRIMITIVE_INVALID:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_INVALID\n");
      break;
   case SVGA3D_PRIMITIVE_TRIANGLELIST:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_TRIANGLELIST\n");
      break;
   case SVGA3D_PRIMITIVE_POINTLIST:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_POINTLIST\n");
      break;
   case SVGA3D_PRIMITIVE_LINELIST:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_LINELIST\n");
      break;
   case SVGA3D_PRIMITIVE_LINESTRIP:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_LINESTRIP\n");
      break;
   case SVGA3D_PRIMITIVE_TRIANGLESTRIP:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_TRIANGLESTRIP\n");
      break;
   case SVGA3D_PRIMITIVE_TRIANGLEFAN:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_TRIANGLEFAN\n");
      break;
   case SVGA3D_PRIMITIVE_LINELIST_ADJ:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_LINELIST_ADJ\n");
      break;
   case SVGA3D_PRIMITIVE_LINESTRIP_ADJ:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_LINESTRIP_ADJ\n");
      break;
   case SVGA3D_PRIMITIVE_TRIANGLELIST_ADJ:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_TRIANGLELIST_ADJ\n");
      break;
   case SVGA3D_PRIMITIVE_TRIANGLESTRIP_ADJ:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_TRIANGLESTRIP_ADJ\n");
      break;
   case SVGA3D_PRIMITIVE_1_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_1_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_2_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_2_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_3_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_3_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_4_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_4_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_5_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_5_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_6_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_6_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_7_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_7_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_8_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_8_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_9_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_9_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_10_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_10_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_11_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_11_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_12_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_12_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_13_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_13_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_14_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_14_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_15_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_15_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_16_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_16_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_17_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_17_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_18_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_18_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_19_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_19_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_20_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_20_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_21_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_21_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_22_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_22_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_23_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_23_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_24_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_24_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_25_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_25_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_26_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_26_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_27_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_27_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_28_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_28_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_29_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_29_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_30_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_30_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_31_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_31_CONTROL_POINT_PATCH\n");
      break;
   case SVGA3D_PRIMITIVE_32_CONTROL_POINT_PATCH:
      _debug_printf("\t\t.primType = SVGA3D_PRIMITIVE_32_CONTROL_POINT_PATCH\n");
      break;
   default:
      _debug_printf("\t\t.primType = %i\n", (*cmd).primType);
      break;
   }
   _debug_printf("\t\t.primitiveCount = %u\n", (*cmd).primitiveCount);
   _debug_printf("\t\t.indexArray.surfaceId = %u\n", (*cmd).indexArray.surfaceId);
   _debug_printf("\t\t.indexArray.offset = %u\n", (*cmd).indexArray.offset);
   _debug_printf("\t\t.indexArray.stride = %u\n", (*cmd).indexArray.stride);
   _debug_printf("\t\t.indexWidth = %u\n", (*cmd).indexWidth);
   _debug_printf("\t\t.indexBias = %i\n", (*cmd).indexBias);
}

static void
dump_SVGA3dCmdDrawPrimitives(const SVGA3dCmdDrawPrimitives *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.numVertexDecls = %u\n", (*cmd).numVertexDecls);
   _debug_printf("\t\t.numRanges = %u\n", (*cmd).numRanges);
}

static void
dump_SVGA3dTextureState(const SVGA3dTextureState *cmd)
{
   _debug_printf("\t\t.stage = %u\n", (*cmd).stage);
   switch((*cmd).name) {
   case SVGA3D_TS_INVALID:
      _debug_printf("\t\t.name = SVGA3D_TS_INVALID\n");
      break;
   case SVGA3D_TS_BIND_TEXTURE:
      _debug_printf("\t\t.name = SVGA3D_TS_BIND_TEXTURE\n");
      break;
   case SVGA3D_TS_COLOROP:
      _debug_printf("\t\t.name = SVGA3D_TS_COLOROP\n");
      break;
   case SVGA3D_TS_COLORARG1:
      _debug_printf("\t\t.name = SVGA3D_TS_COLORARG1\n");
      break;
   case SVGA3D_TS_COLORARG2:
      _debug_printf("\t\t.name = SVGA3D_TS_COLORARG2\n");
      break;
   case SVGA3D_TS_ALPHAOP:
      _debug_printf("\t\t.name = SVGA3D_TS_ALPHAOP\n");
      break;
   case SVGA3D_TS_ALPHAARG1:
      _debug_printf("\t\t.name = SVGA3D_TS_ALPHAARG1\n");
      break;
   case SVGA3D_TS_ALPHAARG2:
      _debug_printf("\t\t.name = SVGA3D_TS_ALPHAARG2\n");
      break;
   case SVGA3D_TS_ADDRESSU:
      _debug_printf("\t\t.name = SVGA3D_TS_ADDRESSU\n");
      break;
   case SVGA3D_TS_ADDRESSV:
      _debug_printf("\t\t.name = SVGA3D_TS_ADDRESSV\n");
      break;
   case SVGA3D_TS_MIPFILTER:
      _debug_printf("\t\t.name = SVGA3D_TS_MIPFILTER\n");
      break;
   case SVGA3D_TS_MAGFILTER:
      _debug_printf("\t\t.name = SVGA3D_TS_MAGFILTER\n");
      break;
   case SVGA3D_TS_MINFILTER:
      _debug_printf("\t\t.name = SVGA3D_TS_MINFILTER\n");
      break;
   case SVGA3D_TS_BORDERCOLOR:
      _debug_printf("\t\t.name = SVGA3D_TS_BORDERCOLOR\n");
      break;
   case SVGA3D_TS_TEXCOORDINDEX:
      _debug_printf("\t\t.name = SVGA3D_TS_TEXCOORDINDEX\n");
      break;
   case SVGA3D_TS_TEXTURETRANSFORMFLAGS:
      _debug_printf("\t\t.name = SVGA3D_TS_TEXTURETRANSFORMFLAGS\n");
      break;
   case SVGA3D_TS_TEXCOORDGEN:
      _debug_printf("\t\t.name = SVGA3D_TS_TEXCOORDGEN\n");
      break;
   case SVGA3D_TS_BUMPENVMAT00:
      _debug_printf("\t\t.name = SVGA3D_TS_BUMPENVMAT00\n");
      break;
   case SVGA3D_TS_BUMPENVMAT01:
      _debug_printf("\t\t.name = SVGA3D_TS_BUMPENVMAT01\n");
      break;
   case SVGA3D_TS_BUMPENVMAT10:
      _debug_printf("\t\t.name = SVGA3D_TS_BUMPENVMAT10\n");
      break;
   case SVGA3D_TS_BUMPENVMAT11:
      _debug_printf("\t\t.name = SVGA3D_TS_BUMPENVMAT11\n");
      break;
   case SVGA3D_TS_TEXTURE_MIPMAP_LEVEL:
      _debug_printf("\t\t.name = SVGA3D_TS_TEXTURE_MIPMAP_LEVEL\n");
      break;
   case SVGA3D_TS_TEXTURE_LOD_BIAS:
      _debug_printf("\t\t.name = SVGA3D_TS_TEXTURE_LOD_BIAS\n");
      break;
   case SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL:
      _debug_printf("\t\t.name = SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL\n");
      break;
   case SVGA3D_TS_ADDRESSW:
      _debug_printf("\t\t.name = SVGA3D_TS_ADDRESSW\n");
      break;
   case SVGA3D_TS_GAMMA:
      _debug_printf("\t\t.name = SVGA3D_TS_GAMMA\n");
      break;
   case SVGA3D_TS_BUMPENVLSCALE:
      _debug_printf("\t\t.name = SVGA3D_TS_BUMPENVLSCALE\n");
      break;
   case SVGA3D_TS_BUMPENVLOFFSET:
      _debug_printf("\t\t.name = SVGA3D_TS_BUMPENVLOFFSET\n");
      break;
   case SVGA3D_TS_COLORARG0:
      _debug_printf("\t\t.name = SVGA3D_TS_COLORARG0\n");
      break;
   case SVGA3D_TS_ALPHAARG0:
      _debug_printf("\t\t.name = SVGA3D_TS_ALPHAARG0\n");
      break;
   case SVGA3D_TS_CONSTANT:
      _debug_printf("\t\t.name = SVGA3D_TS_CONSTANT\n");
      break;
   case SVGA3D_TS_COLOR_KEY_ENABLE:
      _debug_printf("\t\t.name = SVGA3D_TS_COLOR_KEY_ENABLE\n");
      break;
   case SVGA3D_TS_COLOR_KEY:
      _debug_printf("\t\t.name = SVGA3D_TS_COLOR_KEY\n");
      break;
   default:
      _debug_printf("\t\t.name = %i\n", (*cmd).name);
      break;
   }
   _debug_printf("\t\t.value = %u\n", (*cmd).value);
   _debug_printf("\t\t.floatValue = %f\n", (*cmd).floatValue);
}

static void
dump_SVGA3dCmdSetTextureState(const SVGA3dCmdSetTextureState *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
}

static void
dump_SVGA3dCmdSetTransform(const SVGA3dCmdSetTransform *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   switch((*cmd).type) {
   case SVGA3D_TRANSFORM_INVALID:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_INVALID\n");
      break;
   case SVGA3D_TRANSFORM_WORLD:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_WORLD\n");
      break;
   case SVGA3D_TRANSFORM_VIEW:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_VIEW\n");
      break;
   case SVGA3D_TRANSFORM_PROJECTION:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_PROJECTION\n");
      break;
   case SVGA3D_TRANSFORM_TEXTURE0:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_TEXTURE0\n");
      break;
   case SVGA3D_TRANSFORM_TEXTURE1:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_TEXTURE1\n");
      break;
   case SVGA3D_TRANSFORM_TEXTURE2:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_TEXTURE2\n");
      break;
   case SVGA3D_TRANSFORM_TEXTURE3:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_TEXTURE3\n");
      break;
   case SVGA3D_TRANSFORM_TEXTURE4:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_TEXTURE4\n");
      break;
   case SVGA3D_TRANSFORM_TEXTURE5:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_TEXTURE5\n");
      break;
   case SVGA3D_TRANSFORM_TEXTURE6:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_TEXTURE6\n");
      break;
   case SVGA3D_TRANSFORM_TEXTURE7:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_TEXTURE7\n");
      break;
   case SVGA3D_TRANSFORM_WORLD1:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_WORLD1\n");
      break;
   case SVGA3D_TRANSFORM_WORLD2:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_WORLD2\n");
      break;
   case SVGA3D_TRANSFORM_WORLD3:
      _debug_printf("\t\t.type = SVGA3D_TRANSFORM_WORLD3\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
   _debug_printf("\t\t.matrix[0] = %f\n", (*cmd).matrix[0]);
   _debug_printf("\t\t.matrix[1] = %f\n", (*cmd).matrix[1]);
   _debug_printf("\t\t.matrix[2] = %f\n", (*cmd).matrix[2]);
   _debug_printf("\t\t.matrix[3] = %f\n", (*cmd).matrix[3]);
   _debug_printf("\t\t.matrix[4] = %f\n", (*cmd).matrix[4]);
   _debug_printf("\t\t.matrix[5] = %f\n", (*cmd).matrix[5]);
   _debug_printf("\t\t.matrix[6] = %f\n", (*cmd).matrix[6]);
   _debug_printf("\t\t.matrix[7] = %f\n", (*cmd).matrix[7]);
   _debug_printf("\t\t.matrix[8] = %f\n", (*cmd).matrix[8]);
   _debug_printf("\t\t.matrix[9] = %f\n", (*cmd).matrix[9]);
   _debug_printf("\t\t.matrix[10] = %f\n", (*cmd).matrix[10]);
   _debug_printf("\t\t.matrix[11] = %f\n", (*cmd).matrix[11]);
   _debug_printf("\t\t.matrix[12] = %f\n", (*cmd).matrix[12]);
   _debug_printf("\t\t.matrix[13] = %f\n", (*cmd).matrix[13]);
   _debug_printf("\t\t.matrix[14] = %f\n", (*cmd).matrix[14]);
   _debug_printf("\t\t.matrix[15] = %f\n", (*cmd).matrix[15]);
}

static void
dump_SVGA3dCmdSetZRange(const SVGA3dCmdSetZRange *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.zRange.min = %f\n", (*cmd).zRange.min);
   _debug_printf("\t\t.zRange.max = %f\n", (*cmd).zRange.max);
}

static void
dump_SVGA3dCmdSetMaterial(const SVGA3dCmdSetMaterial *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   switch((*cmd).face) {
   case SVGA3D_FACE_INVALID:
      _debug_printf("\t\t.face = SVGA3D_FACE_INVALID\n");
      break;
   case SVGA3D_FACE_NONE:
      _debug_printf("\t\t.face = SVGA3D_FACE_NONE\n");
      break;
   case SVGA3D_FACE_FRONT:
      _debug_printf("\t\t.face = SVGA3D_FACE_FRONT\n");
      break;
   case SVGA3D_FACE_BACK:
      _debug_printf("\t\t.face = SVGA3D_FACE_BACK\n");
      break;
   case SVGA3D_FACE_FRONT_BACK:
      _debug_printf("\t\t.face = SVGA3D_FACE_FRONT_BACK\n");
      break;
   default:
      _debug_printf("\t\t.face = %i\n", (*cmd).face);
      break;
   }
   _debug_printf("\t\t.material.diffuse[0] = %f\n", (*cmd).material.diffuse[0]);
   _debug_printf("\t\t.material.diffuse[1] = %f\n", (*cmd).material.diffuse[1]);
   _debug_printf("\t\t.material.diffuse[2] = %f\n", (*cmd).material.diffuse[2]);
   _debug_printf("\t\t.material.diffuse[3] = %f\n", (*cmd).material.diffuse[3]);
   _debug_printf("\t\t.material.ambient[0] = %f\n", (*cmd).material.ambient[0]);
   _debug_printf("\t\t.material.ambient[1] = %f\n", (*cmd).material.ambient[1]);
   _debug_printf("\t\t.material.ambient[2] = %f\n", (*cmd).material.ambient[2]);
   _debug_printf("\t\t.material.ambient[3] = %f\n", (*cmd).material.ambient[3]);
   _debug_printf("\t\t.material.specular[0] = %f\n", (*cmd).material.specular[0]);
   _debug_printf("\t\t.material.specular[1] = %f\n", (*cmd).material.specular[1]);
   _debug_printf("\t\t.material.specular[2] = %f\n", (*cmd).material.specular[2]);
   _debug_printf("\t\t.material.specular[3] = %f\n", (*cmd).material.specular[3]);
   _debug_printf("\t\t.material.emissive[0] = %f\n", (*cmd).material.emissive[0]);
   _debug_printf("\t\t.material.emissive[1] = %f\n", (*cmd).material.emissive[1]);
   _debug_printf("\t\t.material.emissive[2] = %f\n", (*cmd).material.emissive[2]);
   _debug_printf("\t\t.material.emissive[3] = %f\n", (*cmd).material.emissive[3]);
   _debug_printf("\t\t.material.shininess = %f\n", (*cmd).material.shininess);
}

static void
dump_SVGA3dCmdSetLightData(const SVGA3dCmdSetLightData *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.index = %u\n", (*cmd).index);
   switch((*cmd).data.type) {
   case SVGA3D_LIGHTTYPE_INVALID:
      _debug_printf("\t\t.data.type = SVGA3D_LIGHTTYPE_INVALID\n");
      break;
   case SVGA3D_LIGHTTYPE_POINT:
      _debug_printf("\t\t.data.type = SVGA3D_LIGHTTYPE_POINT\n");
      break;
   case SVGA3D_LIGHTTYPE_SPOT1:
      _debug_printf("\t\t.data.type = SVGA3D_LIGHTTYPE_SPOT1\n");
      break;
   case SVGA3D_LIGHTTYPE_SPOT2:
      _debug_printf("\t\t.data.type = SVGA3D_LIGHTTYPE_SPOT2\n");
      break;
   case SVGA3D_LIGHTTYPE_DIRECTIONAL:
      _debug_printf("\t\t.data.type = SVGA3D_LIGHTTYPE_DIRECTIONAL\n");
      break;
   default:
      _debug_printf("\t\t.data.type = %i\n", (*cmd).data.type);
      break;
   }
   _debug_printf("\t\t.data.inWorldSpace = %u\n", (*cmd).data.inWorldSpace);
   _debug_printf("\t\t.data.diffuse[0] = %f\n", (*cmd).data.diffuse[0]);
   _debug_printf("\t\t.data.diffuse[1] = %f\n", (*cmd).data.diffuse[1]);
   _debug_printf("\t\t.data.diffuse[2] = %f\n", (*cmd).data.diffuse[2]);
   _debug_printf("\t\t.data.diffuse[3] = %f\n", (*cmd).data.diffuse[3]);
   _debug_printf("\t\t.data.specular[0] = %f\n", (*cmd).data.specular[0]);
   _debug_printf("\t\t.data.specular[1] = %f\n", (*cmd).data.specular[1]);
   _debug_printf("\t\t.data.specular[2] = %f\n", (*cmd).data.specular[2]);
   _debug_printf("\t\t.data.specular[3] = %f\n", (*cmd).data.specular[3]);
   _debug_printf("\t\t.data.ambient[0] = %f\n", (*cmd).data.ambient[0]);
   _debug_printf("\t\t.data.ambient[1] = %f\n", (*cmd).data.ambient[1]);
   _debug_printf("\t\t.data.ambient[2] = %f\n", (*cmd).data.ambient[2]);
   _debug_printf("\t\t.data.ambient[3] = %f\n", (*cmd).data.ambient[3]);
   _debug_printf("\t\t.data.position[0] = %f\n", (*cmd).data.position[0]);
   _debug_printf("\t\t.data.position[1] = %f\n", (*cmd).data.position[1]);
   _debug_printf("\t\t.data.position[2] = %f\n", (*cmd).data.position[2]);
   _debug_printf("\t\t.data.position[3] = %f\n", (*cmd).data.position[3]);
   _debug_printf("\t\t.data.direction[0] = %f\n", (*cmd).data.direction[0]);
   _debug_printf("\t\t.data.direction[1] = %f\n", (*cmd).data.direction[1]);
   _debug_printf("\t\t.data.direction[2] = %f\n", (*cmd).data.direction[2]);
   _debug_printf("\t\t.data.direction[3] = %f\n", (*cmd).data.direction[3]);
   _debug_printf("\t\t.data.range = %f\n", (*cmd).data.range);
   _debug_printf("\t\t.data.falloff = %f\n", (*cmd).data.falloff);
   _debug_printf("\t\t.data.attenuation0 = %f\n", (*cmd).data.attenuation0);
   _debug_printf("\t\t.data.attenuation1 = %f\n", (*cmd).data.attenuation1);
   _debug_printf("\t\t.data.attenuation2 = %f\n", (*cmd).data.attenuation2);
   _debug_printf("\t\t.data.theta = %f\n", (*cmd).data.theta);
   _debug_printf("\t\t.data.phi = %f\n", (*cmd).data.phi);
}

static void
dump_SVGA3dCmdSetLightEnabled(const SVGA3dCmdSetLightEnabled *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.index = %u\n", (*cmd).index);
   _debug_printf("\t\t.enabled = %u\n", (*cmd).enabled);
}

static void
dump_SVGA3dCmdSetViewport(const SVGA3dCmdSetViewport *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.rect.x = %u\n", (*cmd).rect.x);
   _debug_printf("\t\t.rect.y = %u\n", (*cmd).rect.y);
   _debug_printf("\t\t.rect.w = %u\n", (*cmd).rect.w);
   _debug_printf("\t\t.rect.h = %u\n", (*cmd).rect.h);
}

static void
dump_SVGA3dCmdSetScissorRect(const SVGA3dCmdSetScissorRect *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.rect.x = %u\n", (*cmd).rect.x);
   _debug_printf("\t\t.rect.y = %u\n", (*cmd).rect.y);
   _debug_printf("\t\t.rect.w = %u\n", (*cmd).rect.w);
   _debug_printf("\t\t.rect.h = %u\n", (*cmd).rect.h);
}

static void
dump_SVGA3dCmdSetClipPlane(const SVGA3dCmdSetClipPlane *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.index = %u\n", (*cmd).index);
   _debug_printf("\t\t.plane[0] = %f\n", (*cmd).plane[0]);
   _debug_printf("\t\t.plane[1] = %f\n", (*cmd).plane[1]);
   _debug_printf("\t\t.plane[2] = %f\n", (*cmd).plane[2]);
   _debug_printf("\t\t.plane[3] = %f\n", (*cmd).plane[3]);
}

static void
dump_SVGA3dCmdDefineShader(const SVGA3dCmdDefineShader *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.shid = %u\n", (*cmd).shid);
   switch((*cmd).type) {
   case SVGA3D_SHADERTYPE_INVALID:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_INVALID\n");
      break;
   case SVGA3D_SHADERTYPE_VS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_VS\n");
      break;
   case SVGA3D_SHADERTYPE_PS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_PS\n");
      break;
   case SVGA3D_SHADERTYPE_GS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_GS\n");
      break;
   case SVGA3D_SHADERTYPE_HS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_HS\n");
      break;
   case SVGA3D_SHADERTYPE_DS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_DS\n");
      break;
   case SVGA3D_SHADERTYPE_CS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_CS\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
}

static void
dump_SVGA3dCmdDestroyShader(const SVGA3dCmdDestroyShader *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.shid = %u\n", (*cmd).shid);
   switch((*cmd).type) {
   case SVGA3D_SHADERTYPE_INVALID:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_INVALID\n");
      break;
   case SVGA3D_SHADERTYPE_VS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_VS\n");
      break;
   case SVGA3D_SHADERTYPE_PS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_PS\n");
      break;
   case SVGA3D_SHADERTYPE_GS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_GS\n");
      break;
   case SVGA3D_SHADERTYPE_HS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_HS\n");
      break;
   case SVGA3D_SHADERTYPE_DS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_DS\n");
      break;
   case SVGA3D_SHADERTYPE_CS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_CS\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
}

static void
dump_SVGA3dCmdSetShaderConst(const SVGA3dCmdSetShaderConst *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   _debug_printf("\t\t.reg = %u\n", (*cmd).reg);
   switch((*cmd).type) {
   case SVGA3D_SHADERTYPE_INVALID:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_INVALID\n");
      break;
   case SVGA3D_SHADERTYPE_VS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_VS\n");
      break;
   case SVGA3D_SHADERTYPE_PS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_PS\n");
      break;
   case SVGA3D_SHADERTYPE_GS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_GS\n");
      break;
   case SVGA3D_SHADERTYPE_HS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_HS\n");
      break;
   case SVGA3D_SHADERTYPE_DS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_DS\n");
      break;
   case SVGA3D_SHADERTYPE_CS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_CS\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
   switch((*cmd).ctype) {
   case SVGA3D_CONST_TYPE_FLOAT:
      _debug_printf("\t\t.ctype = SVGA3D_CONST_TYPE_FLOAT\n");
      break;
   case SVGA3D_CONST_TYPE_INT:
      _debug_printf("\t\t.ctype = SVGA3D_CONST_TYPE_INT\n");
      break;
   case SVGA3D_CONST_TYPE_BOOL:
      _debug_printf("\t\t.ctype = SVGA3D_CONST_TYPE_BOOL\n");
      break;
   default:
      _debug_printf("\t\t.ctype = %i\n", (*cmd).ctype);
      break;
   }
   _debug_printf("\t\t.values[0] = %u\n", (*cmd).values[0]);
   _debug_printf("\t\t.values[1] = %u\n", (*cmd).values[1]);
   _debug_printf("\t\t.values[2] = %u\n", (*cmd).values[2]);
   _debug_printf("\t\t.values[3] = %u\n", (*cmd).values[3]);
}

static void
dump_SVGA3dCmdSetShader(const SVGA3dCmdSetShader *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   switch((*cmd).type) {
   case SVGA3D_SHADERTYPE_INVALID:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_INVALID\n");
      break;
   case SVGA3D_SHADERTYPE_VS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_VS\n");
      break;
   case SVGA3D_SHADERTYPE_PS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_PS\n");
      break;
   case SVGA3D_SHADERTYPE_GS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_GS\n");
      break;
   case SVGA3D_SHADERTYPE_HS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_HS\n");
      break;
   case SVGA3D_SHADERTYPE_DS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_DS\n");
      break;
   case SVGA3D_SHADERTYPE_CS:
      _debug_printf("\t\t.type = SVGA3D_SHADERTYPE_CS\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
   _debug_printf("\t\t.shid = %u\n", (*cmd).shid);
}

static void
dump_SVGA3dCmdBeginQuery(const SVGA3dCmdBeginQuery *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   switch((*cmd).type) {
   case SVGA3D_QUERYTYPE_INVALID:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_INVALID\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSION:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSION\n");
      break;
   case SVGA3D_QUERYTYPE_TIMESTAMP:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_TIMESTAMP\n");
      break;
   case SVGA3D_QUERYTYPE_TIMESTAMPDISJOINT:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_TIMESTAMPDISJOINT\n");
      break;
   case SVGA3D_QUERYTYPE_PIPELINESTATS:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_PIPELINESTATS\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE\n");
      break;
   case SVGA3D_QUERYTYPE_STREAMOUTPUTSTATS:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_STREAMOUTPUTSTATS\n");
      break;
   case SVGA3D_QUERYTYPE_STREAMOVERFLOWPREDICATE:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_STREAMOVERFLOWPREDICATE\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSION64:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSION64\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM0:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM0\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM1:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM1\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM2:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM2\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM3:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM3\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM0:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM0\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM1:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM1\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM2:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM2\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM3:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM3\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
}

static void
dump_SVGA3dCmdEndQuery(const SVGA3dCmdEndQuery *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   switch((*cmd).type) {
   case SVGA3D_QUERYTYPE_INVALID:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_INVALID\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSION:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSION\n");
      break;
   case SVGA3D_QUERYTYPE_TIMESTAMP:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_TIMESTAMP\n");
      break;
   case SVGA3D_QUERYTYPE_TIMESTAMPDISJOINT:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_TIMESTAMPDISJOINT\n");
      break;
   case SVGA3D_QUERYTYPE_PIPELINESTATS:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_PIPELINESTATS\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE\n");
      break;
   case SVGA3D_QUERYTYPE_STREAMOUTPUTSTATS:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_STREAMOUTPUTSTATS\n");
      break;
   case SVGA3D_QUERYTYPE_STREAMOVERFLOWPREDICATE:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_STREAMOVERFLOWPREDICATE\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSION64:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSION64\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM0:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM0\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM1:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM1\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM2:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM2\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM3:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM3\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM0:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM0\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM1:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM1\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM2:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM2\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM3:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM3\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
   _debug_printf("\t\t.guestResult.gmrId = %u\n", (*cmd).guestResult.gmrId);
   _debug_printf("\t\t.guestResult.offset = %u\n", (*cmd).guestResult.offset);
}

static void
dump_SVGA3dCmdWaitForQuery(const SVGA3dCmdWaitForQuery *cmd)
{
   _debug_printf("\t\t.cid = %u\n", (*cmd).cid);
   switch((*cmd).type) {
   case SVGA3D_QUERYTYPE_INVALID:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_INVALID\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSION:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSION\n");
      break;
   case SVGA3D_QUERYTYPE_TIMESTAMP:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_TIMESTAMP\n");
      break;
   case SVGA3D_QUERYTYPE_TIMESTAMPDISJOINT:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_TIMESTAMPDISJOINT\n");
      break;
   case SVGA3D_QUERYTYPE_PIPELINESTATS:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_PIPELINESTATS\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSIONPREDICATE\n");
      break;
   case SVGA3D_QUERYTYPE_STREAMOUTPUTSTATS:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_STREAMOUTPUTSTATS\n");
      break;
   case SVGA3D_QUERYTYPE_STREAMOVERFLOWPREDICATE:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_STREAMOVERFLOWPREDICATE\n");
      break;
   case SVGA3D_QUERYTYPE_OCCLUSION64:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_OCCLUSION64\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM0:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM0\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM1:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM1\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM2:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM2\n");
      break;
   case SVGA3D_QUERYTYPE_SOSTATS_STREAM3:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOSTATS_STREAM3\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM0:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM0\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM1:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM1\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM2:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM2\n");
      break;
   case SVGA3D_QUERYTYPE_SOP_STREAM3:
      _debug_printf("\t\t.type = SVGA3D_QUERYTYPE_SOP_STREAM3\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
   _debug_printf("\t\t.guestResult.gmrId = %u\n", (*cmd).guestResult.gmrId);
   _debug_printf("\t\t.guestResult.offset = %u\n", (*cmd).guestResult.offset);
}

static void
dump_SVGA3dCmdBlitSurfaceToScreen(const SVGA3dCmdBlitSurfaceToScreen *cmd)
{
   _debug_printf("\t\t.srcImage.sid = %u\n", (*cmd).srcImage.sid);
   _debug_printf("\t\t.srcImage.face = %u\n", (*cmd).srcImage.face);
   _debug_printf("\t\t.srcImage.mipmap = %u\n", (*cmd).srcImage.mipmap);
   _debug_printf("\t\t.srcRect.left = %i\n", (*cmd).srcRect.left);
   _debug_printf("\t\t.srcRect.top = %i\n", (*cmd).srcRect.top);
   _debug_printf("\t\t.srcRect.right = %i\n", (*cmd).srcRect.right);
   _debug_printf("\t\t.srcRect.bottom = %i\n", (*cmd).srcRect.bottom);
   _debug_printf("\t\t.destScreenId = %u\n", (*cmd).destScreenId);
   _debug_printf("\t\t.destRect.left = %i\n", (*cmd).destRect.left);
   _debug_printf("\t\t.destRect.top = %i\n", (*cmd).destRect.top);
   _debug_printf("\t\t.destRect.right = %i\n", (*cmd).destRect.right);
   _debug_printf("\t\t.destRect.bottom = %i\n", (*cmd).destRect.bottom);
}

static void
dump_SVGA3dCmdSetOTableBase64(const SVGA3dCmdSetOTableBase64 *cmd)
{
   switch((*cmd).type) {
   case SVGA_OTABLE_MOB:
      _debug_printf("\t\t.type = SVGA_OTABLE_MOB\n");
      break;
   case SVGA_OTABLE_SURFACE:
      _debug_printf("\t\t.type = SVGA_OTABLE_SURFACE\n");
      break;
   case SVGA_OTABLE_CONTEXT:
      _debug_printf("\t\t.type = SVGA_OTABLE_CONTEXT\n");
      break;
   case SVGA_OTABLE_SHADER:
      _debug_printf("\t\t.type = SVGA_OTABLE_SHADER\n");
      break;
   case SVGA_OTABLE_SCREENTARGET:
      _debug_printf("\t\t.type = SVGA_OTABLE_SCREENTARGET\n");
      break;
   case SVGA_OTABLE_DXCONTEXT:
      _debug_printf("\t\t.type = SVGA_OTABLE_DXCONTEXT\n");
      break;
   case SVGA_OTABLE_RESERVED1:
      _debug_printf("\t\t.type = SVGA_OTABLE_RESERVED1\n");
      break;
   case SVGA_OTABLE_RESERVED2:
      _debug_printf("\t\t.type = SVGA_OTABLE_RESERVED2\n");
      break;
   default:
      _debug_printf("\t\t.type = %i\n", (*cmd).type);
      break;
   }
   _debug_printf("\t\t.baseAddress = %lu\n", (*cmd).baseAddress);
   _debug_printf("\t\t.sizeInBytes = %u\n", (*cmd).sizeInBytes);
   _debug_printf("\t\t.validSizeInBytes = %u\n", (*cmd).validSizeInBytes);
   switch((*cmd).ptDepth) {
   case SVGA3D_MOBFMT_INVALID:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_INVALID\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH_0:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH_0\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH_1:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH_1\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH_2:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH_2\n");
      break;
   case SVGA3D_MOBFMT_RANGE:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_RANGE\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH64_0:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH64_0\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH64_1:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH64_1\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH64_2:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH64_2\n");
      break;
   case SVGA3D_MOBFMT_EMPTY:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_EMPTY\n");
      break;
   case SVGA3D_MOBFMT_HB:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_HB\n");
      break;
   default:
      _debug_printf("\t\t.ptDepth = %i\n", (*cmd).ptDepth);
      break;
   }
}

static void
dump_SVGA3dCmdDefineGBMob64(const SVGA3dCmdDefineGBMob64 *cmd)
{
   _debug_printf("\t\t.mobid = %u\n", (*cmd).mobid);
   switch((*cmd).ptDepth) {
   case SVGA3D_MOBFMT_INVALID:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_INVALID\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH_0:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH_0\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH_1:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH_1\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH_2:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH_2\n");
      break;
   case SVGA3D_MOBFMT_RANGE:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_RANGE\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH64_0:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH64_0\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH64_1:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH64_1\n");
      break;
   case SVGA3D_MOBFMT_PTDEPTH64_2:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_PTDEPTH64_2\n");
      break;
   case SVGA3D_MOBFMT_EMPTY:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_EMPTY\n");
      break;
   case SVGA3D_MOBFMT_HB:
      _debug_printf("\t\t.ptDepth = SVGA3D_MOBFMT_HB\n");
      break;
   default:
      _debug_printf("\t\t.ptDepth = %i\n", (*cmd).ptDepth);
      break;
   }
   _debug_printf("\t\t.base = %lu\n", (*cmd).base);
   _debug_printf("\t\t.sizeInBytes = %u\n", (*cmd).sizeInBytes);
}

static void
dump_SVGA3dCmdDefineGBSurface(const SVGA3dCmdDefineGBSurface *cmd)
{
   _debug_printf("\t\t.sid = %u\n", (*cmd).sid);
   _debug_printf("\t\t.surfaceFlags = %u\n", (*cmd).surfaceFlags);
   switch((*cmd).format) {
   case SVGA3D_FORMAT_INVALID:
      _debug_printf("\t\t.format = SVGA3D_FORMAT_INVALID\n");
      break;
   case SVGA3D_X8R8G8B8:
      _debug_printf("\t\t.format = SVGA3D_X8R8G8B8\n");
      break;
   case SVGA3D_A8R8G8B8:
      _debug_printf("\t\t.format = SVGA3D_A8R8G8B8\n");
      break;
   case SVGA3D_R5G6B5:
      _debug_printf("\t\t.format = SVGA3D_R5G6B5\n");
      break;
   case SVGA3D_X1R5G5B5:
      _debug_printf("\t\t.format = SVGA3D_X1R5G5B5\n");
      break;
   case SVGA3D_A1R5G5B5:
      _debug_printf("\t\t.format = SVGA3D_A1R5G5B5\n");
      break;
   case SVGA3D_A4R4G4B4:
      _debug_printf("\t\t.format = SVGA3D_A4R4G4B4\n");
      break;
   case SVGA3D_Z_D32:
      _debug_printf("\t\t.format = SVGA3D_Z_D32\n");
      break;
   case SVGA3D_Z_D16:
      _debug_printf("\t\t.format = SVGA3D_Z_D16\n");
      break;
   case SVGA3D_Z_D24S8:
      _debug_printf("\t\t.format = SVGA3D_Z_D24S8\n");
      break;
   case SVGA3D_Z_D15S1:
      _debug_printf("\t\t.format = SVGA3D_Z_D15S1\n");
      break;
   case SVGA3D_LUMINANCE8:
      _debug_printf("\t\t.format = SVGA3D_LUMINANCE8\n");
      break;
   case SVGA3D_LUMINANCE4_ALPHA4:
      _debug_printf("\t\t.format = SVGA3D_LUMINANCE4_ALPHA4\n");
      break;
   case SVGA3D_LUMINANCE16:
      _debug_printf("\t\t.format = SVGA3D_LUMINANCE16\n");
      break;
   case SVGA3D_LUMINANCE8_ALPHA8:
      _debug_printf("\t\t.format = SVGA3D_LUMINANCE8_ALPHA8\n");
      break;
   case SVGA3D_DXT1:
      _debug_printf("\t\t.format = SVGA3D_DXT1\n");
      break;
   case SVGA3D_DXT2:
      _debug_printf("\t\t.format = SVGA3D_DXT2\n");
      break;
   case SVGA3D_DXT3:
      _debug_printf("\t\t.format = SVGA3D_DXT3\n");
      break;
   case SVGA3D_DXT4:
      _debug_printf("\t\t.format = SVGA3D_DXT4\n");
      break;
   case SVGA3D_DXT5:
      _debug_printf("\t\t.format = SVGA3D_DXT5\n");
      break;
   case SVGA3D_BUMPU8V8:
      _debug_printf("\t\t.format = SVGA3D_BUMPU8V8\n");
      break;
   case SVGA3D_BUMPL6V5U5:
      _debug_printf("\t\t.format = SVGA3D_BUMPL6V5U5\n");
      break;
   case SVGA3D_BUMPX8L8V8U8:
      _debug_printf("\t\t.format = SVGA3D_BUMPX8L8V8U8\n");
      break;
   case SVGA3D_FORMAT_DEAD1:
      _debug_printf("\t\t.format = SVGA3D_FORMAT_DEAD1\n");
      break;
   case SVGA3D_ARGB_S10E5:
      _debug_printf("\t\t.format = SVGA3D_ARGB_S10E5\n");
      break;
   case SVGA3D_ARGB_S23E8:
      _debug_printf("\t\t.format = SVGA3D_ARGB_S23E8\n");
      break;
   case SVGA3D_A2R10G10B10:
      _debug_printf("\t\t.format = SVGA3D_A2R10G10B10\n");
      break;
   case SVGA3D_V8U8:
      _debug_printf("\t\t.format = SVGA3D_V8U8\n");
      break;
   case SVGA3D_Q8W8V8U8:
      _debug_printf("\t\t.format = SVGA3D_Q8W8V8U8\n");
      break;
   case SVGA3D_CxV8U8:
      _debug_printf("\t\t.format = SVGA3D_CxV8U8\n");
      break;
   case SVGA3D_X8L8V8U8:
      _debug_printf("\t\t.format = SVGA3D_X8L8V8U8\n");
      break;
   case SVGA3D_A2W10V10U10:
      _debug_printf("\t\t.format = SVGA3D_A2W10V10U10\n");
      break;
   case SVGA3D_ALPHA8:
      _debug_printf("\t\t.format = SVGA3D_ALPHA8\n");
      break;
   case SVGA3D_R_S10E5:
      _debug_printf("\t\t.format = SVGA3D_R_S10E5\n");
      break;
   case SVGA3D_R_S23E8:
      _debug_printf("\t\t.format = SVGA3D_R_S23E8\n");
      break;
   case SVGA3D_RG_S10E5:
      _debug_printf("\t\t.format = SVGA3D_RG_S10E5\n");
      break;
   case SVGA3D_RG_S23E8:
      _debug_printf("\t\t.format = SVGA3D_RG_S23E8\n");
      break;
   case SVGA3D_BUFFER:
      _debug_printf("\t\t.format = SVGA3D_BUFFER\n");
      break;
   case SVGA3D_Z_D24X8:
      _debug_printf("\t\t.format = SVGA3D_Z_D24X8\n");
      break;
   case SVGA3D_V16U16:
      _debug_printf("\t\t.format = SVGA3D_V16U16\n");
      break;
   case SVGA3D_G16R16:
      _debug_printf("\t\t.format = SVGA3D_G16R16\n");
      break;
   case SVGA3D_A16B16G16R16:
      _debug_printf("\t\t.format = SVGA3D_A16B16G16R16\n");
      break;
   case SVGA3D_UYVY:
      _debug_printf("\t\t.format = SVGA3D_UYVY\n");
      break;
   case SVGA3D_YUY2:
      _debug_printf("\t\t.format = SVGA3D_YUY2\n");
      break;
   case SVGA3D_NV12:
      _debug_printf("\t\t.format = SVGA3D_NV12\n");
      break;
   case SVGA3D_FORMAT_DEAD2:
      _debug_printf("\t\t.format = SVGA3D_FORMAT_DEAD2\n");
      break;
   case SVGA3D_R32G32B32A32_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32A32_TYPELESS\n");
      break;
   case SVGA3D_R32G32B32A32_UINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32A32_UINT\n");
      break;
   case SVGA3D_R32G32B32A32_SINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32A32_SINT\n");
      break;
   case SVGA3D_R32G32B32_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32_TYPELESS\n");
      break;
   case SVGA3D_R32G32B32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32_FLOAT\n");
      break;
   case SVGA3D_R32G32B32_UINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32_UINT\n");
      break;
   case SVGA3D_R32G32B32_SINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32_SINT\n");
      break;
   case SVGA3D_R16G16B16A16_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_TYPELESS\n");
      break;
   case SVGA3D_R16G16B16A16_UINT:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_UINT\n");
      break;
   case SVGA3D_R16G16B16A16_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_SNORM\n");
      break;
   case SVGA3D_R16G16B16A16_SINT:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_SINT\n");
      break;
   case SVGA3D_R32G32_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32G32_TYPELESS\n");
      break;
   case SVGA3D_R32G32_UINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32_UINT\n");
      break;
   case SVGA3D_R32G32_SINT:
      _debug_printf("\t\t.format = SVGA3D_R32G32_SINT\n");
      break;
   case SVGA3D_R32G8X24_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32G8X24_TYPELESS\n");
      break;
   case SVGA3D_D32_FLOAT_S8X24_UINT:
      _debug_printf("\t\t.format = SVGA3D_D32_FLOAT_S8X24_UINT\n");
      break;
   case SVGA3D_R32_FLOAT_X8X24:
      _debug_printf("\t\t.format = SVGA3D_R32_FLOAT_X8X24\n");
      break;
   case SVGA3D_X32_G8X24_UINT:
      _debug_printf("\t\t.format = SVGA3D_X32_G8X24_UINT\n");
      break;
   case SVGA3D_R10G10B10A2_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R10G10B10A2_TYPELESS\n");
      break;
   case SVGA3D_R10G10B10A2_UINT:
      _debug_printf("\t\t.format = SVGA3D_R10G10B10A2_UINT\n");
      break;
   case SVGA3D_R11G11B10_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R11G11B10_FLOAT\n");
      break;
   case SVGA3D_R8G8B8A8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_TYPELESS\n");
      break;
   case SVGA3D_R8G8B8A8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_UNORM\n");
      break;
   case SVGA3D_R8G8B8A8_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_UNORM_SRGB\n");
      break;
   case SVGA3D_R8G8B8A8_UINT:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_UINT\n");
      break;
   case SVGA3D_R8G8B8A8_SINT:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_SINT\n");
      break;
   case SVGA3D_R16G16_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R16G16_TYPELESS\n");
      break;
   case SVGA3D_R16G16_UINT:
      _debug_printf("\t\t.format = SVGA3D_R16G16_UINT\n");
      break;
   case SVGA3D_R16G16_SINT:
      _debug_printf("\t\t.format = SVGA3D_R16G16_SINT\n");
      break;
   case SVGA3D_R32_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R32_TYPELESS\n");
      break;
   case SVGA3D_D32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_D32_FLOAT\n");
      break;
   case SVGA3D_R32_UINT:
      _debug_printf("\t\t.format = SVGA3D_R32_UINT\n");
      break;
   case SVGA3D_R32_SINT:
      _debug_printf("\t\t.format = SVGA3D_R32_SINT\n");
      break;
   case SVGA3D_R24G8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R24G8_TYPELESS\n");
      break;
   case SVGA3D_D24_UNORM_S8_UINT:
      _debug_printf("\t\t.format = SVGA3D_D24_UNORM_S8_UINT\n");
      break;
   case SVGA3D_R24_UNORM_X8:
      _debug_printf("\t\t.format = SVGA3D_R24_UNORM_X8\n");
      break;
   case SVGA3D_X24_G8_UINT:
      _debug_printf("\t\t.format = SVGA3D_X24_G8_UINT\n");
      break;
   case SVGA3D_R8G8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R8G8_TYPELESS\n");
      break;
   case SVGA3D_R8G8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8_UNORM\n");
      break;
   case SVGA3D_R8G8_UINT:
      _debug_printf("\t\t.format = SVGA3D_R8G8_UINT\n");
      break;
   case SVGA3D_R8G8_SINT:
      _debug_printf("\t\t.format = SVGA3D_R8G8_SINT\n");
      break;
   case SVGA3D_R16_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R16_TYPELESS\n");
      break;
   case SVGA3D_R16_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R16_UNORM\n");
      break;
   case SVGA3D_R16_UINT:
      _debug_printf("\t\t.format = SVGA3D_R16_UINT\n");
      break;
   case SVGA3D_R16_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R16_SNORM\n");
      break;
   case SVGA3D_R16_SINT:
      _debug_printf("\t\t.format = SVGA3D_R16_SINT\n");
      break;
   case SVGA3D_R8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_R8_TYPELESS\n");
      break;
   case SVGA3D_R8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R8_UNORM\n");
      break;
   case SVGA3D_R8_UINT:
      _debug_printf("\t\t.format = SVGA3D_R8_UINT\n");
      break;
   case SVGA3D_R8_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R8_SNORM\n");
      break;
   case SVGA3D_R8_SINT:
      _debug_printf("\t\t.format = SVGA3D_R8_SINT\n");
      break;
   case SVGA3D_P8:
      _debug_printf("\t\t.format = SVGA3D_P8\n");
      break;
   case SVGA3D_R9G9B9E5_SHAREDEXP:
      _debug_printf("\t\t.format = SVGA3D_R9G9B9E5_SHAREDEXP\n");
      break;
   case SVGA3D_R8G8_B8G8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8_B8G8_UNORM\n");
      break;
   case SVGA3D_G8R8_G8B8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_G8R8_G8B8_UNORM\n");
      break;
   case SVGA3D_BC1_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC1_TYPELESS\n");
      break;
   case SVGA3D_BC1_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_BC1_UNORM_SRGB\n");
      break;
   case SVGA3D_BC2_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC2_TYPELESS\n");
      break;
   case SVGA3D_BC2_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_BC2_UNORM_SRGB\n");
      break;
   case SVGA3D_BC3_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC3_TYPELESS\n");
      break;
   case SVGA3D_BC3_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_BC3_UNORM_SRGB\n");
      break;
   case SVGA3D_BC4_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC4_TYPELESS\n");
      break;
   case SVGA3D_ATI1:
      _debug_printf("\t\t.format = SVGA3D_ATI1\n");
      break;
   case SVGA3D_BC4_SNORM:
      _debug_printf("\t\t.format = SVGA3D_BC4_SNORM\n");
      break;
   case SVGA3D_BC5_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC5_TYPELESS\n");
      break;
   case SVGA3D_ATI2:
      _debug_printf("\t\t.format = SVGA3D_ATI2\n");
      break;
   case SVGA3D_BC5_SNORM:
      _debug_printf("\t\t.format = SVGA3D_BC5_SNORM\n");
      break;
   case SVGA3D_R10G10B10_XR_BIAS_A2_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R10G10B10_XR_BIAS_A2_UNORM\n");
      break;
   case SVGA3D_B8G8R8A8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8A8_TYPELESS\n");
      break;
   case SVGA3D_B8G8R8A8_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8A8_UNORM_SRGB\n");
      break;
   case SVGA3D_B8G8R8X8_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8X8_TYPELESS\n");
      break;
   case SVGA3D_B8G8R8X8_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8X8_UNORM_SRGB\n");
      break;
   case SVGA3D_Z_DF16:
      _debug_printf("\t\t.format = SVGA3D_Z_DF16\n");
      break;
   case SVGA3D_Z_DF24:
      _debug_printf("\t\t.format = SVGA3D_Z_DF24\n");
      break;
   case SVGA3D_Z_D24S8_INT:
      _debug_printf("\t\t.format = SVGA3D_Z_D24S8_INT\n");
      break;
   case SVGA3D_YV12:
      _debug_printf("\t\t.format = SVGA3D_YV12\n");
      break;
   case SVGA3D_R32G32B32A32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R32G32B32A32_FLOAT\n");
      break;
   case SVGA3D_R16G16B16A16_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_FLOAT\n");
      break;
   case SVGA3D_R16G16B16A16_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R16G16B16A16_UNORM\n");
      break;
   case SVGA3D_R32G32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R32G32_FLOAT\n");
      break;
   case SVGA3D_R10G10B10A2_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R10G10B10A2_UNORM\n");
      break;
   case SVGA3D_R8G8B8A8_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8B8A8_SNORM\n");
      break;
   case SVGA3D_R16G16_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R16G16_FLOAT\n");
      break;
   case SVGA3D_R16G16_UNORM:
      _debug_printf("\t\t.format = SVGA3D_R16G16_UNORM\n");
      break;
   case SVGA3D_R16G16_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R16G16_SNORM\n");
      break;
   case SVGA3D_R32_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R32_FLOAT\n");
      break;
   case SVGA3D_R8G8_SNORM:
      _debug_printf("\t\t.format = SVGA3D_R8G8_SNORM\n");
      break;
   case SVGA3D_R16_FLOAT:
      _debug_printf("\t\t.format = SVGA3D_R16_FLOAT\n");
      break;
   case SVGA3D_D16_UNORM:
      _debug_printf("\t\t.format = SVGA3D_D16_UNORM\n");
      break;
   case SVGA3D_A8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_A8_UNORM\n");
      break;
   case SVGA3D_BC1_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC1_UNORM\n");
      break;
   case SVGA3D_BC2_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC2_UNORM\n");
      break;
   case SVGA3D_BC3_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC3_UNORM\n");
      break;
   case SVGA3D_B5G6R5_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B5G6R5_UNORM\n");
      break;
   case SVGA3D_B5G5R5A1_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B5G5R5A1_UNORM\n");
      break;
   case SVGA3D_B8G8R8A8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8A8_UNORM\n");
      break;
   case SVGA3D_B8G8R8X8_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B8G8R8X8_UNORM\n");
      break;
   case SVGA3D_BC4_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC4_UNORM\n");
      break;
   case SVGA3D_BC5_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC5_UNORM\n");
      break;
   case SVGA3D_B4G4R4A4_UNORM:
      _debug_printf("\t\t.format = SVGA3D_B4G4R4A4_UNORM\n");
      break;
   case SVGA3D_BC6H_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC6H_TYPELESS\n");
      break;
   case SVGA3D_BC6H_UF16:
      _debug_printf("\t\t.format = SVGA3D_BC6H_UF16\n");
      break;
   case SVGA3D_BC6H_SF16:
      _debug_printf("\t\t.format = SVGA3D_BC6H_SF16\n");
      break;
   case SVGA3D_BC7_TYPELESS:
      _debug_printf("\t\t.format = SVGA3D_BC7_TYPELESS\n");
      break;
   case SVGA3D_BC7_UNORM:
      _debug_printf("\t\t.format = SVGA3D_BC7_UNORM\n");
      break;
   case SVGA3D_BC7_UNORM_SRGB:
      _debug_printf("\t\t.format = SVGA3D_BC7_UNORM_SRGB\n");
      break;
   case SVGA3D_AYUV:
      _debug_printf("\t\t.format = SVGA3D_AYUV\n");
      break;
   default:
      _debug_printf("\t\t.format = %i\n", (*cmd).format);
      break;
   }
   _debug_printf("\t\t.numMipLevels = %u\n", (*cmd).numMipLevels);
   _debug_printf("\t\t.multisampleCount = %u\n", (*cmd).multisampleCount);
   switch((*cmd).autogenFilter) {
   case SVGA3D_TEX_FILTER_NONE:
      _debug_printf("\t\t.autogenFilter = SVGA3D_TEX_FILTER_NONE\n");
      break;
   case SVGA3D_TEX_FILTER_NEAREST:
      _debug_printf("\t\t.autogenFilter = SVGA3D_TEX_FILTER_NEAREST\n");
      break;
   case SVGA3D_TEX_FILTER_LINEAR:
      _debug_printf("\t\t.autogenFilter = SVGA3D_TEX_FILTER_LINEAR\n");
      break;
   case SVGA3D_TEX_FILTER_ANISOTROPIC:
      _debug_printf("\t\t.autogenFilter = SVGA3D_TEX_FILTER_ANISOTROPIC\n");
      break;
   case SVGA3D_TEX_FILTER_FLATCUBIC:
      _debug_printf("\t\t.autogenFilter = SVGA3D_TEX_FILTER_FLATCUBIC\n");
      break;
   case SVGA3D_TEX_FILTER_GAUSSIANCUBIC:
      _debug_printf("\t\t.autogenFilter = SVGA3D_TEX_FILTER_GAUSSIANCUBIC\n");
      break;
   case SVGA3D_TEX_FILTER_PYRAMIDALQUAD:
      _debug_printf("\t\t.autogenFilter = SVGA3D_TEX_FILTER_PYRAMIDALQUAD\n");
      break;
   case SVGA3D_TEX_FILTER_GAUSSIANQUAD:
      _debug_printf("\t\t.autogenFilter = SVGA3D_TEX_FILTER_GAUSSIANQUAD\n");
      break;
   default:
      _debug_printf("\t\t.autogenFilter = %i\n", (*cmd).autogenFilter);
      break;
   }
   _debug_printf("\t\t.size.width = %u\n", (*cmd).size.width);
   _debug_printf("\t\t.size.height = %u\n", (*cmd).size.height);
   _debug_printf("\t\t.size.depth = %u\n", (*cmd).size.depth);
}

static void
dump_SVGA3dCmdDestroyGBSurface(const SVGA3dCmdDestroyGBSurface *cmd)
{
   _debug_printf("\t\t.sid = %u\n", (*cmd).sid);
}

static void
dump_SVGA3dCmdBindGBSurface(const SVGA3dCmdBindGBSurface *cmd)
{
   _debug_printf("\t\t.sid = %u\n", (*cmd).sid);
   _debug_printf("\t\t.mobid = %u\n", (*cmd).mobid);
}

static void
dump_SVGA3dCmdUpdateGBImage(const SVGA3dCmdUpdateGBImage *cmd)
{
   _debug_printf("\t\t.image.sid = %u\n", (*cmd).image.sid);
   _debug_printf("\t\t.image.face = %u\n", (*cmd).image.face);
   _debug_printf("\t\t.image.mipmap = %u\n", (*cmd).image.mipmap);
   _debug_printf("\t\t.box.x = %u\n", (*cmd).box.x);
   _debug_printf("\t\t.box.y = %u\n", (*cmd).box.y);
   _debug_printf("\t\t.box.z = %u\n", (*cmd).box.z);
   _debug_printf("\t\t.box.w = %u\n", (*cmd).box.w);
   _debug_printf("\t\t.box.h = %u\n", (*cmd).box.h);
   _debug_printf("\t\t.box.d = %u\n", (*cmd).box.d);
}

static void
dump_SVGA3dCmdInvalidateGBSurface(const SVGA3dCmdInvalidateGBSurface *cmd)
{
   _debug_printf("\t\t.sid = %u\n", (*cmd).sid);
}

static void
dump_SVGA3dCmdDefineGBScreenTarget(const SVGA3dCmdDefineGBScreenTarget *cmd)
{
   _debug_printf("\t\t.stid = %u\n", (*cmd).stid);
   _debug_printf("\t\t.width = %u\n", (*cmd).width);
   _debug_printf("\t\t.height = %u\n", (*cmd).height);
   _debug_printf("\t\t.xRoot = %i\n", (*cmd).xRoot);
   _debug_printf("\t\t.yRoot = %i\n", (*cmd).yRoot);
   _debug_printf("\t\t.flags = %u\n", (*cmd).flags);
   _debug_printf("\t\t.dpi = %u\n", (*cmd).dpi);
}

static void
dump_SVGA3dCmdDestroyGBScreenTarget(const SVGA3dCmdDestroyGBScreenTarget *cmd)
{
   _debug_printf("\t\t.stid = %u\n", (*cmd).stid);
}

static void
dump_SVGA3dCmdBindGBScreenTarget(const SVGA3dCmdBindGBScreenTarget *cmd)
{
   _debug_printf("\t\t.stid = %u\n", (*cmd).stid);
   _debug_printf("\t\t.image.sid = %u\n", (*cmd).image.sid);
   _debug_printf("\t\t.image.face = %u\n", (*cmd).image.face);
   _debug_printf("\t\t.image.mipmap = %u\n", (*cmd).image.mipmap);
}

static void
dump_SVGA3dCmdUpdateGBScreenTarget(const SVGA3dCmdUpdateGBScreenTarget *cmd)
{
   _debug_printf("\t\t.stid = %u\n", (*cmd).stid);
   _debug_printf("\t\t.rect.x = %u\n", (*cmd).rect.x);
   _debug_printf("\t\t.rect.y = %u\n", (*cmd).rect.y);
   _debug_printf("\t\t.rect.w = %u\n", (*cmd).rect.w);
   _debug_printf("\t\t.rect.h = %u\n", (*cmd).rect.h);
}


void
svga_dump_command(uint32_t cmd_id, const void *data, uint32_t size)
{
   const uint8_t *body = (const uint8_t *)data;
   const uint8_t *next = body + size;

   switch(cmd_id) {
   case SVGA_3D_CMD_SURFACE_DEFINE:
      _debug_printf("\tSVGA_3D_CMD_SURFACE_DEFINE\n");
      {
         const SVGA3dCmdDefineSurface *cmd = (const SVGA3dCmdDefineSurface *)body;
         dump_SVGA3dCmdDefineSurface(cmd);
         body = (const uint8_t *)&cmd[1];
         while(body + sizeof(SVGA3dSize) <= next) {
            dump_SVGA3dSize((const SVGA3dSize *)body);
            body += sizeof(SVGA3dSize);
         }
      }
      break;
   case SVGA_3D_CMD_SURFACE_DESTROY:
      _debug_printf("\tSVGA_3D_CMD_SURFACE_DESTROY\n");
      {
         const SVGA3dCmdDestroySurface *cmd = (const SVGA3dCmdDestroySurface *)body;
         dump_SVGA3dCmdDestroySurface(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SURFACE_COPY:
      _debug_printf("\tSVGA_3D_CMD_SURFACE_COPY\n");
      {
         const SVGA3dCmdSurfaceCopy *cmd = (const SVGA3dCmdSurfaceCopy *)body;
         dump_SVGA3dCmdSurfaceCopy(cmd);
         body = (const uint8_t *)&cmd[1];
         while(body + sizeof(SVGA3dCopyBox) <= next) {
            dump_SVGA3dCopyBox((const SVGA3dCopyBox *)body);
            body += sizeof(SVGA3dCopyBox);
         }
      }
      break;
   case SVGA_3D_CMD_SURFACE_STRETCHBLT:
      _debug_printf("\tSVGA_3D_CMD_SURFACE_STRETCHBLT\n");
      {
         const SVGA3dCmdSurfaceStretchBlt *cmd = (const SVGA3dCmdSurfaceStretchBlt *)body;
         dump_SVGA3dCmdSurfaceStretchBlt(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SURFACE_DMA:
      _debug_printf("\tSVGA_3D_CMD_SURFACE_DMA\n");
      {
         const SVGA3dCmdSurfaceDMA *cmd = (const SVGA3dCmdSurfaceDMA *)body;
         dump_SVGA3dCmdSurfaceDMA(cmd);
         body = (const uint8_t *)&cmd[1];
         while(body + sizeof(SVGA3dCopyBox) <= next) {
            dump_SVGA3dCopyBox((const SVGA3dCopyBox *)body);
            body += sizeof(SVGA3dCopyBox);
         }
      }
      break;
   case SVGA_3D_CMD_CONTEXT_DEFINE:
      _debug_printf("\tSVGA_3D_CMD_CONTEXT_DEFINE\n");
      {
         const SVGA3dCmdDefineContext *cmd = (const SVGA3dCmdDefineContext *)body;
         dump_SVGA3dCmdDefineContext(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_CONTEXT_DESTROY:
      _debug_printf("\tSVGA_3D_CMD_CONTEXT_DESTROY\n");
      {
         const SVGA3dCmdDestroyContext *cmd = (const SVGA3dCmdDestroyContext *)body;
         dump_SVGA3dCmdDestroyContext(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SETTRANSFORM:
      _debug_printf("\tSVGA_3D_CMD_SETTRANSFORM\n");
      {
         const SVGA3dCmdSetTransform *cmd = (const SVGA3dCmdSetTransform *)body;
         dump_SVGA3dCmdSetTransform(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SETZRANGE:
      _debug_printf("\tSVGA_3D_CMD_SETZRANGE\n");
      {
         const SVGA3dCmdSetZRange *cmd = (const SVGA3dCmdSetZRange *)body;
         dump_SVGA3dCmdSetZRange(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SETRENDERSTATE:
      _debug_printf("\tSVGA_3D_CMD_SETRENDERSTATE\n");
      {
         const SVGA3dCmdSetRenderState *cmd = (const SVGA3dCmdSetRenderState *)body;
         dump_SVGA3dCmdSetRenderState(cmd);
         body = (const uint8_t *)&cmd[1];
         while(body + sizeof(SVGA3dRenderState) <= next) {
            dump_SVGA3dRenderState((const SVGA3dRenderState *)body);
            body += sizeof(SVGA3dRenderState);
         }
      }
      break;
   case SVGA_3D_CMD_SETRENDERTARGET:
      _debug_printf("\tSVGA_3D_CMD_SETRENDERTARGET\n");
      {
         const SVGA3dCmdSetRenderTarget *cmd = (const SVGA3dCmdSetRenderTarget *)body;
         dump_SVGA3dCmdSetRenderTarget(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SETTEXTURESTATE:
      _debug_printf("\tSVGA_3D_CMD_SETTEXTURESTATE\n");
      {
         const SVGA3dCmdSetTextureState *cmd = (const SVGA3dCmdSetTextureState *)body;
         dump_SVGA3dCmdSetTextureState(cmd);
         body = (const uint8_t *)&cmd[1];
         while(body + sizeof(SVGA3dTextureState) <= next) {
            dump_SVGA3dTextureState((const SVGA3dTextureState *)body);
            body += sizeof(SVGA3dTextureState);
         }
      }
      break;
   case SVGA_3D_CMD_SETMATERIAL:
      _debug_printf("\tSVGA_3D_CMD_SETMATERIAL\n");
      {
         const SVGA3dCmdSetMaterial *cmd = (const SVGA3dCmdSetMaterial *)body;
         dump_SVGA3dCmdSetMaterial(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SETLIGHTDATA:
      _debug_printf("\tSVGA_3D_CMD_SETLIGHTDATA\n");
      {
         const SVGA3dCmdSetLightData *cmd = (const SVGA3dCmdSetLightData *)body;
         dump_SVGA3dCmdSetLightData(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SETLIGHTENABLED:
      _debug_printf("\tSVGA_3D_CMD_SETLIGHTENABLED\n");
      {
         const SVGA3dCmdSetLightEnabled *cmd = (const SVGA3dCmdSetLightEnabled *)body;
         dump_SVGA3dCmdSetLightEnabled(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SETVIEWPORT:
      _debug_printf("\tSVGA_3D_CMD_SETVIEWPORT\n");
      {
         const SVGA3dCmdSetViewport *cmd = (const SVGA3dCmdSetViewport *)body;
         dump_SVGA3dCmdSetViewport(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SETCLIPPLANE:
      _debug_printf("\tSVGA_3D_CMD_SETCLIPPLANE\n");
      {
         const SVGA3dCmdSetClipPlane *cmd = (const SVGA3dCmdSetClipPlane *)body;
         dump_SVGA3dCmdSetClipPlane(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_CLEAR:
      _debug_printf("\tSVGA_3D_CMD_CLEAR\n");
      {
         const SVGA3dCmdClear *cmd = (const SVGA3dCmdClear *)body;
         dump_SVGA3dCmdClear(cmd);
         body = (const uint8_t *)&cmd[1];
         while(body + sizeof(SVGA3dRect) <= next) {
            dump_SVGA3dRect((const SVGA3dRect *)body);
            body += sizeof(SVGA3dRect);
         }
      }
      break;
   case SVGA_3D_CMD_PRESENT:
      _debug_printf("\tSVGA_3D_CMD_PRESENT\n");
      {
         const SVGA3dCmdPresent *cmd = (const SVGA3dCmdPresent *)body;
         dump_SVGA3dCmdPresent(cmd);
         body = (const uint8_t *)&cmd[1];
         while(body + sizeof(SVGA3dCopyRect) <= next) {
            dump_SVGA3dCopyRect((const SVGA3dCopyRect *)body);
            body += sizeof(SVGA3dCopyRect);
         }
      }
      break;
   case SVGA_3D_CMD_SHADER_DEFINE:
      _debug_printf("\tSVGA_3D_CMD_SHADER_DEFINE\n");
      {
         const SVGA3dCmdDefineShader *cmd = (const SVGA3dCmdDefineShader *)body;
         dump_SVGA3dCmdDefineShader(cmd);
         body = (const uint8_t *)&cmd[1];
         //svga_shader_dump((const uint32_t *)body,
         //                 (unsigned)(next - body)/sizeof(uint32_t),
         //                 FALSE);
         body = next;
      }
      break;
   case SVGA_3D_CMD_SHADER_DESTROY:
      _debug_printf("\tSVGA_3D_CMD_SHADER_DESTROY\n");
      {
         const SVGA3dCmdDestroyShader *cmd = (const SVGA3dCmdDestroyShader *)body;
         dump_SVGA3dCmdDestroyShader(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SET_SHADER:
      _debug_printf("\tSVGA_3D_CMD_SET_SHADER\n");
      {
         const SVGA3dCmdSetShader *cmd = (const SVGA3dCmdSetShader *)body;
         dump_SVGA3dCmdSetShader(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SET_SHADER_CONST:
      _debug_printf("\tSVGA_3D_CMD_SET_SHADER_CONST\n");
      {
         const SVGA3dCmdSetShaderConst *cmd = (const SVGA3dCmdSetShaderConst *)body;
         dump_SVGA3dCmdSetShaderConst(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_DRAW_PRIMITIVES:
      _debug_printf("\tSVGA_3D_CMD_DRAW_PRIMITIVES\n");
      {
         const SVGA3dCmdDrawPrimitives *cmd = (const SVGA3dCmdDrawPrimitives *)body;
         unsigned i, j;
         dump_SVGA3dCmdDrawPrimitives(cmd);
         body = (const uint8_t *)&cmd[1];
         for(i = 0; i < cmd->numVertexDecls; ++i) {
            dump_SVGA3dVertexDecl((const SVGA3dVertexDecl *)body);
            body += sizeof(SVGA3dVertexDecl);
         }
         for(j = 0; j < cmd->numRanges; ++j) {
            dump_SVGA3dPrimitiveRange((const SVGA3dPrimitiveRange *)body);
            body += sizeof(SVGA3dPrimitiveRange);
         }
         while(body + sizeof(SVGA3dVertexDivisor) <= next) {
            dump_SVGA3dVertexDivisor((const SVGA3dVertexDivisor *)body);
            body += sizeof(SVGA3dVertexDivisor);
         }
      }
      break;
   case SVGA_3D_CMD_SETSCISSORRECT:
      _debug_printf("\tSVGA_3D_CMD_SETSCISSORRECT\n");
      {
         const SVGA3dCmdSetScissorRect *cmd = (const SVGA3dCmdSetScissorRect *)body;
         dump_SVGA3dCmdSetScissorRect(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_BEGIN_QUERY:
      _debug_printf("\tSVGA_3D_CMD_BEGIN_QUERY\n");
      {
         const SVGA3dCmdBeginQuery *cmd = (const SVGA3dCmdBeginQuery *)body;
         dump_SVGA3dCmdBeginQuery(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_END_QUERY:
      _debug_printf("\tSVGA_3D_CMD_END_QUERY\n");
      {
         const SVGA3dCmdEndQuery *cmd = (const SVGA3dCmdEndQuery *)body;
         dump_SVGA3dCmdEndQuery(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_WAIT_FOR_QUERY:
      _debug_printf("\tSVGA_3D_CMD_WAIT_FOR_QUERY\n");
      {
         const SVGA3dCmdWaitForQuery *cmd = (const SVGA3dCmdWaitForQuery *)body;
         dump_SVGA3dCmdWaitForQuery(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_SET_OTABLE_BASE64:
      _debug_printf("\tSVGA_3D_CMD_SET_OTABLE_BASE64\n");
      {
         const SVGA3dCmdSetOTableBase64 *cmd = (const SVGA3dCmdSetOTableBase64 *)body;
         dump_SVGA3dCmdSetOTableBase64(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_DEFINE_GB_MOB64:
      _debug_printf("\tSVGA_3D_CMD_DEFINE_GB_MOB64\n");
      {
         const SVGA3dCmdDefineGBMob64 *cmd = (const SVGA3dCmdDefineGBMob64 *)body;
         dump_SVGA3dCmdDefineGBMob64(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN:
      _debug_printf("\tSVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN\n");
      {
         const SVGA3dCmdBlitSurfaceToScreen *cmd = (const SVGA3dCmdBlitSurfaceToScreen *)body;
         dump_SVGA3dCmdBlitSurfaceToScreen(cmd);
         body = (const uint8_t *)&cmd[1];
         while(body + sizeof(SVGASignedRect) <= next) {
            dump_SVGASignedRect((const SVGASignedRect *)body);
            body += sizeof(SVGASignedRect);
         }
      }
      break;
   case SVGA_3D_CMD_DEFINE_GB_SCREENTARGET:
      _debug_printf("\tSVGA_3D_CMD_DEFINE_GB_SCREENTARGET\n");
      {
         const SVGA3dCmdDefineGBScreenTarget *cmd = (const SVGA3dCmdDefineGBScreenTarget *)body;
         dump_SVGA3dCmdDefineGBScreenTarget(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_BIND_GB_SCREENTARGET:
      _debug_printf("\tSVGA_3D_CMD_BIND_GB_SCREENTARGET\n");
      {
         const SVGA3dCmdBindGBScreenTarget *cmd = (const SVGA3dCmdBindGBScreenTarget *)body;
         dump_SVGA3dCmdBindGBScreenTarget(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_UPDATE_GB_SCREENTARGET:
      _debug_printf("\tSVGA_3D_CMD_UPDATE_GB_SCREENTARGET\n");
      {
         const SVGA3dCmdUpdateGBScreenTarget *cmd = (const SVGA3dCmdUpdateGBScreenTarget *)body;
         dump_SVGA3dCmdUpdateGBScreenTarget(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_DESTROY_GB_SCREENTARGET:
      _debug_printf("\tSVGA_3D_CMD_DESTROY_GB_SCREENTARGET\n");
      {
         const SVGA3dCmdDestroyGBScreenTarget *cmd = (const SVGA3dCmdDestroyGBScreenTarget *)body;
         dump_SVGA3dCmdDestroyGBScreenTarget(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_UPDATE_GB_IMAGE:
      _debug_printf("\tSVGA_3D_CMD_UPDATE_GB_IMAGE\n");
      {
         const SVGA3dCmdUpdateGBImage *cmd = (const SVGA3dCmdUpdateGBImage *)body;
         dump_SVGA3dCmdUpdateGBImage(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_DEFINE_GB_SURFACE:
      _debug_printf("\tSVGA_3D_CMD_DEFINE_GB_SURFACE\n");
      {
         const SVGA3dCmdDefineGBSurface *cmd = (const SVGA3dCmdDefineGBSurface *)body;
         dump_SVGA3dCmdDefineGBSurface(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_BIND_GB_SURFACE:
      _debug_printf("\tSVGA_3D_CMD_BIND_GB_SURFACE\n");
      {
         const SVGA3dCmdBindGBSurface *cmd = (const SVGA3dCmdBindGBSurface *)body;
         dump_SVGA3dCmdBindGBSurface(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_INVALIDATE_GB_SURFACE:
      _debug_printf("\tSVGA_3D_CMD_INVALIDATE_GB_SURFACE\n");
      {
         const SVGA3dCmdInvalidateGBSurface *cmd = (const SVGA3dCmdInvalidateGBSurface *)body;
         dump_SVGA3dCmdInvalidateGBSurface(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   case SVGA_3D_CMD_DESTROY_GB_SURFACE:
      _debug_printf("\tSVGA_3D_CMD_DESTROY_GB_SURFACE\n");
      {
         const SVGA3dCmdDestroyGBSurface *cmd = (const SVGA3dCmdDestroyGBSurface *)body;
         dump_SVGA3dCmdDestroyGBSurface(cmd);
         body = (const uint8_t *)&cmd[1];
      }
      break;
   default:
      _debug_printf("\t0x%08x\n", cmd_id);
      break;
   }

   while(body + sizeof(uint32_t) <= next) {
      _debug_printf("\t\t0x%08x\n", *(const uint32_t *)body);
      body += sizeof(uint32_t);
   }
   while(body + sizeof(uint32_t) <= next)
      _debug_printf("\t\t0x%02x\n", *body++);
}


void
svga_dump_commands(const void *commands, uint32_t size)
{
   const uint8_t *next = commands;
   const uint8_t *last = next + size;

   //assert(size % sizeof(uint32_t) == 0);

   while(next < last) {
      const uint32_t cmd_id = *(const uint32_t *)next;

      if(SVGA_3D_CMD_BASE <= cmd_id && cmd_id < SVGA_3D_CMD_MAX) {
         const SVGA3dCmdHeader *header = (const SVGA3dCmdHeader *)next;
         const uint8_t *body = (const uint8_t *)&header[1];

         next = body + header->size;
         if(next > last)
            break;

         svga_dump_command(cmd_id, body, header->size);
      }
      else if(cmd_id == SVGA_CMD_FENCE) {
         _debug_printf("\tSVGA_CMD_FENCE\n");
         _debug_printf("\t\t0x%08x\n", ((const uint32_t *)next)[1]);
         next += 2*sizeof(uint32_t);
      }
      else {
         _debug_printf("\t0x%08x\n", cmd_id);
         next += sizeof(uint32_t);
      }
   }
}

#endif //LOG_ENABLED
