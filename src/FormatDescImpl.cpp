// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include <pch.h>
#include <intsafe.h>

#define R D3D11FCN_R
#define G D3D11FCN_G
#define B D3D11FCN_B
#define A D3D11FCN_A
#define D D3D11FCN_D
#define S D3D11FCN_S
#define X D3D11FCN_X

#define _TYPELESS   D3D11FCI_TYPELESS
#define _FLOAT      D3D11FCI_FLOAT
#define _SNORM      D3D11FCI_SNORM
#define _UNORM      D3D11FCI_UNORM
#define _SINT       D3D11FCI_SINT
#define _UINT       D3D11FCI_UINT
#define _UNORM_SRGB D3D11FCI_UNORM_SRGB
#define _FIXED_2_8  D3D11FCI_BIASED_FIXED_2_8

// --------------------------------------------------------------------------------------------------------------------------------
// Format Cast Sets
const DXGI_FORMAT D3D11FCS_UNKNOWN[] =
{
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R32G32B32A32[] =
{
    DXGI_FORMAT_R32G32B32A32_TYPELESS,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_UINT,
    DXGI_FORMAT_R32G32B32A32_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R32G32B32[] =
{
    DXGI_FORMAT_R32G32B32_TYPELESS,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32_UINT,
    DXGI_FORMAT_R32G32B32_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R16G16B16A16[] =
{
    DXGI_FORMAT_R16G16B16A16_TYPELESS,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R32G32[] =
{
    DXGI_FORMAT_R32G32_TYPELESS,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32_UINT,
    DXGI_FORMAT_R32G32_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R32G8X24[] =
{
    DXGI_FORMAT_R32G8X24_TYPELESS,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R10G10B10A2[] =
{
    DXGI_FORMAT_R10G10B10A2_TYPELESS,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R11G11B10[] =
{
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R8G8B8A8[] =
{
    DXGI_FORMAT_R8G8B8A8_TYPELESS,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R16G16[] =
{
    DXGI_FORMAT_R16G16_TYPELESS,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R32[] =
{
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32_UINT,
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R24G8[] =
{
    DXGI_FORMAT_R24G8_TYPELESS,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R8G8[] =
{
    DXGI_FORMAT_R8G8_TYPELESS,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R16[] =
{
    DXGI_FORMAT_R16_TYPELESS,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R8[] =
{
    DXGI_FORMAT_R8_TYPELESS,
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_A8[] =
{
    DXGI_FORMAT_A8_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R1[] =
{
    DXGI_FORMAT_R1_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R9G9B9E5[] =
{
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R8G8_B8G8[] =
{
    DXGI_FORMAT_R8G8_B8G8_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_G8R8_G8B8[] =
{
    DXGI_FORMAT_G8R8_G8B8_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_BC1[] =
{
    DXGI_FORMAT_BC1_TYPELESS,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC1_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_BC2[] =
{
    DXGI_FORMAT_BC2_TYPELESS,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC2_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_BC3[] =
{
    DXGI_FORMAT_BC3_TYPELESS,
    DXGI_FORMAT_BC3_UNORM,
    DXGI_FORMAT_BC3_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_BC4[] =
{
    DXGI_FORMAT_BC4_TYPELESS,
    DXGI_FORMAT_BC4_UNORM,
    DXGI_FORMAT_BC4_SNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_BC5[] =
{
    DXGI_FORMAT_BC5_TYPELESS,
    DXGI_FORMAT_BC5_UNORM,
    DXGI_FORMAT_BC5_SNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_B5G6R5[] =
{
    DXGI_FORMAT_B5G6R5_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_B5G5R5A1[] =
{
    DXGI_FORMAT_B5G5R5A1_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_B8G8R8A8[] =
{
    DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_B8G8R8X8[] =
{
    DXGI_FORMAT_B8G8R8X8_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_B8G8R8A8_Win7[] =
{
    DXGI_FORMAT_B8G8R8A8_TYPELESS,
    DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_B8G8R8X8_Win7[] =
{
    DXGI_FORMAT_B8G8R8X8_TYPELESS,
    DXGI_FORMAT_B8G8R8X8_UNORM,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_R10G10B10A2_XR[] =
{
    DXGI_FORMAT_R10G10B10A2_TYPELESS,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_BC6H[] =
{
    DXGI_FORMAT_BC6H_TYPELESS,
    DXGI_FORMAT_BC6H_UF16,
    DXGI_FORMAT_BC6H_SF16,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_BC7[] =
{
    DXGI_FORMAT_BC7_TYPELESS,
    DXGI_FORMAT_BC7_UNORM,
    DXGI_FORMAT_BC7_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_AYUV[] =
{
    DXGI_FORMAT_AYUV,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_NV12[] =
{
    DXGI_FORMAT_NV12,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_YUY2[] =
{
    DXGI_FORMAT_YUY2,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_P010[] =
{
    DXGI_FORMAT_P010,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_P016[] =
{
    DXGI_FORMAT_P016,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_NV11[] =
{
    DXGI_FORMAT_NV11,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_420_OPAQUE[] =
{
    DXGI_FORMAT_420_OPAQUE,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_Y410[] =
{
    DXGI_FORMAT_Y410,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_Y416[] =
{
    DXGI_FORMAT_Y416,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_Y210[] =
{
    DXGI_FORMAT_Y210,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_Y216[] =
{
    DXGI_FORMAT_Y216,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_AI44[] =
{
    DXGI_FORMAT_AI44,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_IA44[] =
{
    DXGI_FORMAT_IA44,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_P8[] =
{
    DXGI_FORMAT_P8,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_A8P8[] =
{
    DXGI_FORMAT_A8P8,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_B4G4R4A4[] =
{
    DXGI_FORMAT_B4G4R4A4_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_P208[] =
{
    DXGI_FORMAT_P208,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_V208[] =
{
    DXGI_FORMAT_V208,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_V408[] =
{
    DXGI_FORMAT_V408,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

const DXGI_FORMAT D3D11FCS_A4B4G4R4[] =
{
    DXGI_FORMAT_A4B4G4R4_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

// ----------------------------------------------------------------------------
// As much information about D3D10's interpretation of DXGI Resource Formats should be encoded in this
// table, and everyone should query the information from here, be it for
// specs or for code. 
// The new BitsPerUnit value represents two possible values. If the format is a block compressed format
// then the value stored is bit per block. If the format is not a block compressed format then the value
// represents bits per pixel.
// ----------------------------------------------------------------------------

const CD3D11FormatHelper::FORMAT_DETAIL CD3D11FormatHelper::s_FormatDetail[] =
{
//      DXGI_FORMAT                           ParentFormat                              pDefaultFormatCastSet   BitsPerComponent[4], BitsPerUnit,    SRGB,  WidthAlignment, HeightAlignment, DepthAlignment,   Layout,             TypeLevel,              ComponentName[4],ComponentInterpretation[4],                          bPlanar, bYUV    
    {DXGI_FORMAT_UNKNOWN                     ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {DXGI_FORMAT_R32G32B32A32_TYPELESS       ,DXGI_FORMAT_R32G32B32A32_TYPELESS,        D3D11FCS_R32G32B32A32,  {32,32,32,32},       128,            FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32B32A32_FLOAT      ,DXGI_FORMAT_R32G32B32A32_TYPELESS,        D3D11FCS_R32G32B32A32,  {32,32,32,32},       128,            FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _FLOAT, _FLOAT, _FLOAT, _FLOAT,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32B32A32_UINT       ,DXGI_FORMAT_R32G32B32A32_TYPELESS,        D3D11FCS_R32G32B32A32,  {32,32,32,32},       128,            FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _UINT, _UINT, _UINT, _UINT,                          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32B32A32_SINT       ,DXGI_FORMAT_R32G32B32A32_TYPELESS,        D3D11FCS_R32G32B32A32,  {32,32,32,32},       128,            FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _SINT, _SINT, _SINT, _SINT,                          FALSE,   FALSE,  },
    {DXGI_FORMAT_R32G32B32_TYPELESS          ,DXGI_FORMAT_R32G32B32_TYPELESS,           D3D11FCS_R32G32B32,     {32,32,32,0},        96,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,B,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32B32_FLOAT         ,DXGI_FORMAT_R32G32B32_TYPELESS,           D3D11FCS_R32G32B32,     {32,32,32,0},        96,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _TYPELESS,                   FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32B32_UINT          ,DXGI_FORMAT_R32G32B32_TYPELESS,           D3D11FCS_R32G32B32,     {32,32,32,0},        96,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,X,         _UINT, _UINT, _UINT, _TYPELESS,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32B32_SINT          ,DXGI_FORMAT_R32G32B32_TYPELESS,           D3D11FCS_R32G32B32,     {32,32,32,0},        96,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,X,         _SINT, _SINT, _SINT, _TYPELESS,                      FALSE,   FALSE,  },
    {DXGI_FORMAT_R16G16B16A16_TYPELESS       ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3D11FCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {   DXGI_FORMAT_R16G16B16A16_FLOAT       ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3D11FCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _FLOAT, _FLOAT, _FLOAT, _FLOAT,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16B16A16_UNORM      ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3D11FCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16B16A16_UINT       ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3D11FCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _UINT, _UINT, _UINT, _UINT,                          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16B16A16_SNORM      ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3D11FCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _SNORM, _SNORM, _SNORM, _SNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16B16A16_SINT       ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3D11FCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _SINT, _SINT, _SINT, _SINT,                          FALSE,   FALSE,  },
    {DXGI_FORMAT_R32G32_TYPELESS             ,DXGI_FORMAT_R32G32_TYPELESS,              D3D11FCS_R32G32,        {32,32,0,0},         64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32_FLOAT            ,DXGI_FORMAT_R32G32_TYPELESS,              D3D11FCS_R32G32,        {32,32,0,0},         64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _FLOAT, _FLOAT, _TYPELESS, _TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32_UINT             ,DXGI_FORMAT_R32G32_TYPELESS,              D3D11FCS_R32G32,        {32,32,0,0},         64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _UINT, _UINT, _TYPELESS, _TYPELESS,                  FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32G32_SINT             ,DXGI_FORMAT_R32G32_TYPELESS,              D3D11FCS_R32G32,        {32,32,0,0},         64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _SINT, _SINT, _TYPELESS, _TYPELESS,                  FALSE,   FALSE,  },
    {DXGI_FORMAT_R32G8X24_TYPELESS           ,DXGI_FORMAT_R32G8X24_TYPELESS,            D3D11FCS_R32G8X24,      {32,8,24,0},         64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_D32_FLOAT_S8X24_UINT    ,DXGI_FORMAT_R32G8X24_TYPELESS,            D3D11FCS_R32G8X24,      {32,8,24,0},         64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     D,S,X,X,         _FLOAT,_UINT,_TYPELESS,_TYPELESS,                    FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,DXGI_FORMAT_R32G8X24_TYPELESS,            D3D11FCS_R32G8X24,      {32,8,24,0},         64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _FLOAT,_TYPELESS,_TYPELESS,_TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ,DXGI_FORMAT_R32G8X24_TYPELESS,            D3D11FCS_R32G8X24,      {32,8,24,0},         64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     X,G,X,X,         _TYPELESS,_UINT,_TYPELESS,_TYPELESS,                 FALSE,   FALSE,  },
    {DXGI_FORMAT_R10G10B10A2_TYPELESS        ,DXGI_FORMAT_R10G10B10A2_TYPELESS,         D3D11FCS_R10G10B10A2_XR,{10,10,10,2},        32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R10G10B10A2_UNORM       ,DXGI_FORMAT_R10G10B10A2_TYPELESS,         D3D11FCS_R10G10B10A2_XR,{10,10,10,2},        32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_R10G10B10A2_UINT        ,DXGI_FORMAT_R10G10B10A2_TYPELESS,         D3D11FCS_R10G10B10A2_XR,{10,10,10,2},        32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _UINT, _UINT, _UINT, _UINT,                          FALSE,   FALSE,  },
    {DXGI_FORMAT_R11G11B10_FLOAT             ,DXGI_FORMAT_R11G11B10_FLOAT,              D3D11FCS_R11G11B10,     {11,11,10,0},        32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _TYPELESS,                   FALSE,   FALSE,  },
    {DXGI_FORMAT_R8G8B8A8_TYPELESS           ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3D11FCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8B8A8_UNORM          ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3D11FCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB     ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3D11FCS_R8G8B8A8,      {8,8,8,8},           32,             TRUE,  1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB,  FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8B8A8_UINT           ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3D11FCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _UINT, _UINT, _UINT, _UINT,                          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8B8A8_SNORM          ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3D11FCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _SNORM, _SNORM, _SNORM, _SNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8B8A8_SINT           ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3D11FCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _SINT, _SINT, _SINT, _SINT,                          FALSE,   FALSE,  },
    {DXGI_FORMAT_R16G16_TYPELESS             ,DXGI_FORMAT_R16G16_TYPELESS,              D3D11FCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16_FLOAT            ,DXGI_FORMAT_R16G16_TYPELESS,              D3D11FCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _FLOAT, _FLOAT, _TYPELESS, _TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16_UNORM            ,DXGI_FORMAT_R16G16_TYPELESS,              D3D11FCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _UNORM, _UNORM, _TYPELESS, _TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16_UINT             ,DXGI_FORMAT_R16G16_TYPELESS,              D3D11FCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _UINT, _UINT, _TYPELESS, _TYPELESS,                  FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16_SNORM            ,DXGI_FORMAT_R16G16_TYPELESS,              D3D11FCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _SNORM, _SNORM, _TYPELESS, _TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16G16_SINT             ,DXGI_FORMAT_R16G16_TYPELESS,              D3D11FCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _SINT, _SINT, _TYPELESS, _TYPELESS,                  FALSE,   FALSE,  },
    {DXGI_FORMAT_R32_TYPELESS                ,DXGI_FORMAT_R32_TYPELESS,                 D3D11FCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_D32_FLOAT               ,DXGI_FORMAT_R32_TYPELESS,                 D3D11FCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     D,X,X,X,         _FLOAT, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32_FLOAT               ,DXGI_FORMAT_R32_TYPELESS,                 D3D11FCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _FLOAT, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32_UINT                ,DXGI_FORMAT_R32_TYPELESS,                 D3D11FCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _UINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,   FALSE,  },
    {    DXGI_FORMAT_R32_SINT                ,DXGI_FORMAT_R32_TYPELESS,                 D3D11FCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _SINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,   FALSE,  },
    {DXGI_FORMAT_R24G8_TYPELESS              ,DXGI_FORMAT_R24G8_TYPELESS,               D3D11FCS_R24G8,         {24,8,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_D24_UNORM_S8_UINT       ,DXGI_FORMAT_R24G8_TYPELESS,               D3D11FCS_R24G8,         {24,8,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     D,S,X,X,         _UNORM,_UINT,_TYPELESS,_TYPELESS,                    FALSE,   FALSE,  },
    {    DXGI_FORMAT_R24_UNORM_X8_TYPELESS   ,DXGI_FORMAT_R24G8_TYPELESS,               D3D11FCS_R24G8,         {24,8,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM,_TYPELESS,_TYPELESS,_TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_X24_TYPELESS_G8_UINT    ,DXGI_FORMAT_R24G8_TYPELESS,               D3D11FCS_R24G8,         {24,8,0,0},          32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     X,G,X,X,         _TYPELESS,_UINT,_TYPELESS,_TYPELESS,                 FALSE,   FALSE,  },
    {DXGI_FORMAT_R8G8_TYPELESS               ,DXGI_FORMAT_R8G8_TYPELESS,                D3D11FCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8_UNORM              ,DXGI_FORMAT_R8G8_TYPELESS,                D3D11FCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _UNORM, _UNORM, _TYPELESS, _TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8_UINT               ,DXGI_FORMAT_R8G8_TYPELESS,                D3D11FCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _UINT, _UINT, _TYPELESS, _TYPELESS,                  FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8_SNORM              ,DXGI_FORMAT_R8G8_TYPELESS,                D3D11FCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _SNORM, _SNORM, _TYPELESS, _TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8G8_SINT               ,DXGI_FORMAT_R8G8_TYPELESS,                D3D11FCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,X,X,         _SINT, _SINT, _TYPELESS, _TYPELESS,                  FALSE,   FALSE,  },
    {DXGI_FORMAT_R16_TYPELESS                ,DXGI_FORMAT_R16_TYPELESS,                 D3D11FCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16_FLOAT               ,DXGI_FORMAT_R16_TYPELESS,                 D3D11FCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _FLOAT, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_D16_UNORM               ,DXGI_FORMAT_R16_TYPELESS,                 D3D11FCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     D,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16_UNORM               ,DXGI_FORMAT_R16_TYPELESS,                 D3D11FCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16_UINT                ,DXGI_FORMAT_R16_TYPELESS,                 D3D11FCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _UINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16_SNORM               ,DXGI_FORMAT_R16_TYPELESS,                 D3D11FCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _SNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_R16_SINT                ,DXGI_FORMAT_R16_TYPELESS,                 D3D11FCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _SINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,   FALSE,  },
    {DXGI_FORMAT_R8_TYPELESS                 ,DXGI_FORMAT_R8_TYPELESS,                  D3D11FCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  R,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8_UNORM                ,DXGI_FORMAT_R8_TYPELESS,                  D3D11FCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8_UINT                 ,DXGI_FORMAT_R8_TYPELESS,                  D3D11FCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _UINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8_SNORM                ,DXGI_FORMAT_R8_TYPELESS,                  D3D11FCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _SNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_R8_SINT                 ,DXGI_FORMAT_R8_TYPELESS,                  D3D11FCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _SINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,   FALSE,  },
    {DXGI_FORMAT_A8_UNORM                    ,DXGI_FORMAT_A8_UNORM,                     D3D11FCS_A8,            {0,0,0,8},           8,              FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     X,X,X,A,         _TYPELESS, _TYPELESS, _TYPELESS, _UNORM,             FALSE,   FALSE,  },
    {DXGI_FORMAT_R1_UNORM                    ,DXGI_FORMAT_R1_UNORM,                     D3D11FCS_R1,            {1,0,0,0},           1,              FALSE, 8,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {DXGI_FORMAT_R9G9B9E5_SHAREDEXP          ,DXGI_FORMAT_R9G9B9E5_SHAREDEXP,           D3D11FCS_R9G9B9E5,      {0,0,0,0},           32,             FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _FLOAT,                      FALSE,   FALSE,  },
    {DXGI_FORMAT_R8G8_B8G8_UNORM             ,DXGI_FORMAT_R8G8_B8G8_UNORM,              D3D11FCS_R8G8_B8G8,     {0,0,0,0},           16,             FALSE, 2,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,   FALSE,  },
    {DXGI_FORMAT_G8R8_G8B8_UNORM             ,DXGI_FORMAT_G8R8_G8B8_UNORM,              D3D11FCS_G8R8_G8B8,     {0,0,0,0},           16,             FALSE, 2,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,   FALSE,  },
    {DXGI_FORMAT_BC1_TYPELESS                ,DXGI_FORMAT_BC1_TYPELESS,                 D3D11FCS_BC1,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC1_UNORM               ,DXGI_FORMAT_BC1_TYPELESS,                 D3D11FCS_BC1,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC1_UNORM_SRGB          ,DXGI_FORMAT_BC1_TYPELESS,                 D3D11FCS_BC1,           {0,0,0,0},           64,             TRUE,  4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM,       FALSE,   FALSE,  },
    {DXGI_FORMAT_BC2_TYPELESS                ,DXGI_FORMAT_BC2_TYPELESS,                 D3D11FCS_BC2,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC2_UNORM               ,DXGI_FORMAT_BC2_TYPELESS,                 D3D11FCS_BC2,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC2_UNORM_SRGB          ,DXGI_FORMAT_BC2_TYPELESS,                 D3D11FCS_BC2,           {0,0,0,0},           128,            TRUE,  4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM,       FALSE,   FALSE,  },
    {DXGI_FORMAT_BC3_TYPELESS                ,DXGI_FORMAT_BC3_TYPELESS,                 D3D11FCS_BC3,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC3_UNORM               ,DXGI_FORMAT_BC3_TYPELESS,                 D3D11FCS_BC3,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC3_UNORM_SRGB          ,DXGI_FORMAT_BC3_TYPELESS,                 D3D11FCS_BC3,           {0,0,0,0},           128,            TRUE,  4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM,       FALSE,   FALSE,  },
    {DXGI_FORMAT_BC4_TYPELESS                ,DXGI_FORMAT_BC4_TYPELESS,                 D3D11FCS_BC4,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_PARTIAL_TYPE,  R,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC4_UNORM               ,DXGI_FORMAT_BC4_TYPELESS,                 D3D11FCS_BC4,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC4_SNORM               ,DXGI_FORMAT_BC4_TYPELESS,                 D3D11FCS_BC4,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _SNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   FALSE,  },
    {DXGI_FORMAT_BC5_TYPELESS                ,DXGI_FORMAT_BC5_TYPELESS,                 D3D11FCS_BC5,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC5_UNORM               ,DXGI_FORMAT_BC5_TYPELESS,                 D3D11FCS_BC5,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,X,X,         _UNORM, _UNORM, _TYPELESS, _TYPELESS,                FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC5_SNORM               ,DXGI_FORMAT_BC5_TYPELESS,                 D3D11FCS_BC5,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,X,X,         _SNORM, _SNORM, _TYPELESS, _TYPELESS,                FALSE,   FALSE,  },
    {DXGI_FORMAT_B5G6R5_UNORM                ,DXGI_FORMAT_B5G6R5_UNORM,                 D3D11FCS_B5G6R5,        {5,6,5,0},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,   FALSE,  },
    {DXGI_FORMAT_B5G5R5A1_UNORM              ,DXGI_FORMAT_B5G5R5A1_UNORM,               D3D11FCS_B5G5R5A1,      {5,5,5,1},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {DXGI_FORMAT_B8G8R8A8_UNORM              ,DXGI_FORMAT_B8G8R8A8_TYPELESS,            D3D11FCS_B8G8R8A8_Win7, {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {DXGI_FORMAT_B8G8R8X8_UNORM              ,DXGI_FORMAT_B8G8R8X8_TYPELESS,            D3D11FCS_B8G8R8X8_Win7, {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,   FALSE,  },
    {DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  ,DXGI_FORMAT_R10G10B10A2_TYPELESS,         D3D11FCS_R10G10B10A2_XR,{10,10,10,2},        32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     R,G,B,A,         _FIXED_2_8, _FIXED_2_8, _FIXED_2_8, _UNORM,          FALSE,   FALSE,  },
    {DXGI_FORMAT_B8G8R8A8_TYPELESS           ,DXGI_FORMAT_B8G8R8A8_TYPELESS,            D3D11FCS_B8G8R8A8_Win7, {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  B,G,R,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB     ,DXGI_FORMAT_B8G8R8A8_TYPELESS,            D3D11FCS_B8G8R8A8_Win7, {8,8,8,8},           32,             TRUE,  1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB,  FALSE,   FALSE,  },
    {DXGI_FORMAT_B8G8R8X8_TYPELESS           ,DXGI_FORMAT_B8G8R8X8_TYPELESS,            D3D11FCS_B8G8R8X8_Win7, {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_PARTIAL_TYPE,  B,G,R,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB     ,DXGI_FORMAT_B8G8R8X8_TYPELESS,            D3D11FCS_B8G8R8X8_Win7, {8,8,8,8},           32,             TRUE,  1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,X,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _TYPELESS,    FALSE,   FALSE,  },
    {DXGI_FORMAT_BC6H_TYPELESS               ,DXGI_FORMAT_BC6H_TYPELESS,                D3D11FCS_BC6H,          {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_PARTIAL_TYPE,  R,G,B,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC6H_UF16               ,DXGI_FORMAT_BC6H_TYPELESS,                D3D11FCS_BC6H,          {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _TYPELESS,                   FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC6H_SF16               ,DXGI_FORMAT_BC6H_TYPELESS,                D3D11FCS_BC6H,          {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _TYPELESS,                   FALSE,   FALSE,  },
    {DXGI_FORMAT_BC7_TYPELESS                ,DXGI_FORMAT_BC7_TYPELESS,                 D3D11FCS_BC7,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC7_UNORM               ,DXGI_FORMAT_BC7_TYPELESS,                 D3D11FCS_BC7,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    {    DXGI_FORMAT_BC7_UNORM_SRGB          ,DXGI_FORMAT_BC7_TYPELESS,                 D3D11FCS_BC7,           {0,0,0,0},           128,            TRUE,  4,              4,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM,       FALSE,   FALSE,  },
    // YUV 4:4:4 formats
    { DXGI_FORMAT_AYUV                       ,DXGI_FORMAT_AYUV,                         D3D11FCS_AYUV,          {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   TRUE,   },
    { DXGI_FORMAT_Y410                       ,DXGI_FORMAT_Y410,                         D3D11FCS_Y410,          {10,10,10,2},        32,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   TRUE,   },
    { DXGI_FORMAT_Y416                       ,DXGI_FORMAT_Y416,                         D3D11FCS_Y416,          {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   TRUE,   },
    // YUV 4:2:0 formats
    { DXGI_FORMAT_NV12                       ,DXGI_FORMAT_NV12,                         D3D11FCS_NV12,          {0,0,0,0},           8,              FALSE, 2,              2,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             TRUE,    TRUE,   },
    { DXGI_FORMAT_P010                       ,DXGI_FORMAT_P010,                         D3D11FCS_P010,          {0,0,0,0},           16,             FALSE, 2,              2,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             TRUE,    TRUE,   },
    { DXGI_FORMAT_P016                       ,DXGI_FORMAT_P016,                         D3D11FCS_P016,          {0,0,0,0},           16,             FALSE, 2,              2,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             TRUE,    TRUE,   },
    { DXGI_FORMAT_420_OPAQUE                 ,DXGI_FORMAT_420_OPAQUE,                   D3D11FCS_420_OPAQUE,    {0,0,0,0},           8,              FALSE, 2,              2,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             TRUE,    TRUE,   },
    // YUV 4:2:2 formats
    { DXGI_FORMAT_YUY2                       ,DXGI_FORMAT_YUY2,                         D3D11FCS_YUY2,          {0,0,0,0},           16,             FALSE, 2,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,   TRUE,   },
    { DXGI_FORMAT_Y210                       ,DXGI_FORMAT_Y210,                         D3D11FCS_Y210,          {0,0,0,0},           32,             FALSE, 2,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,   TRUE,   },
    { DXGI_FORMAT_Y216                       ,DXGI_FORMAT_Y216,                         D3D11FCS_Y216,          {0,0,0,0},           32,             FALSE, 2,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,   TRUE,   },
    // YUV 4:1:1 formats
    { DXGI_FORMAT_NV11                       ,DXGI_FORMAT_NV11,                         D3D11FCS_NV11,          {0,0,0,0},           8,              FALSE, 4,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             TRUE,    TRUE,   },
    // Legacy substream formats
    { DXGI_FORMAT_AI44                       ,DXGI_FORMAT_AI44,                         D3D11FCS_AI44,          {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   TRUE,   },
    { DXGI_FORMAT_IA44                       ,DXGI_FORMAT_IA44,                         D3D11FCS_IA44,          {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   TRUE,   },
    { DXGI_FORMAT_P8                         ,DXGI_FORMAT_P8,                           D3D11FCS_P8,            {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   TRUE,   },
    { DXGI_FORMAT_A8P8                       ,DXGI_FORMAT_A8P8,                         D3D11FCS_A8P8,          {0,0,0,0},           16,             FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,   TRUE,   },
    // 
    { DXGI_FORMAT_B4G4R4A4_UNORM             ,DXGI_FORMAT_B4G4R4A4_UNORM,               D3D11FCS_B4G4R4A4,      {4,4,4,4},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
    { DXGI_FORMAT(116)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(117)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(118)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(119)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(120)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(121)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(122)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(123)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(124)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(125)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(126)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(127)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(128)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(129)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(130)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(131)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(132)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(133)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(134)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(135)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(136)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(137)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(138)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(139)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(140)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(141)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(142)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(143)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(144)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(145)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(146)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(147)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(148)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(149)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(150)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(151)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(152)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(153)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(154)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(155)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(156)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(157)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(158)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(159)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(160)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(161)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(162)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(163)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(164)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(165)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(166)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(167)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(168)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(169)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(170)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(171)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(172)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(173)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(174)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(175)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(176)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(177)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(178)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(179)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(180)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(181)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(182)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(183)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(184)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(185)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(186)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(187)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(188)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(189)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT(190)                       ,DXGI_FORMAT_UNKNOWN,                      D3D11FCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3D11FL_CUSTOM,     D3D11FTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,   FALSE,  },
    { DXGI_FORMAT_A4B4G4R4_UNORM             ,DXGI_FORMAT_A4B4G4R4_UNORM,               D3D11FCS_A4B4G4R4,      {4,4,4,4},           16,             FALSE, 1,              1,               1,                D3D11FL_STANDARD,   D3D11FTL_FULL_TYPE,     A,B,G,R,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,   FALSE,  },
};

const UINT CD3D11FormatHelper::s_NumFormats = (sizeof(CD3D11FormatHelper::s_FormatDetail)/sizeof(CD3D11FormatHelper::FORMAT_DETAIL));

#if VALIDATE_FORMAT_ORDER
#define FR( Format, A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,AA,AB,AC,AD,AE,AF,AG,AH ) { Format, A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,AA,AB,AC,AD,AE,AF,AG,AH }
#else
#define FR( Format, A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,AA,AB,AC,AD,AE,AF,AG,AH ) { A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,AA,AB,AC,AD,AE,AF,AG,AH }
#endif

//---------------------------------------------------------------------------------------------------------------------------------
// GetDetailTableIndex
UINT CD3D11FormatHelper::GetDetailTableIndex(DXGI_FORMAT  Format )
{
    if( (UINT)Format < ARRAYSIZE( s_FormatDetail ) )
    {
        assert( s_FormatDetail[(UINT)Format].DXGIFormat == Format );
        return static_cast<UINT>(Format);
    }

    return (UINT)-1;
}

//---------------------------------------------------------------------------------------------------------------------------------
// IsBlockCompressFormat - returns true if format is block compressed. This function is a helper function for GetBitsPerUnit and
// if this function returns true then GetBitsPerUnit returns block size. 
bool CD3D11FormatHelper::IsBlockCompressFormat(DXGI_FORMAT Format)
{
    // Returns true if BC1, BC2, BC3, BC4, BC5, BC6, BC7, or ASTC
    return (Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC5_SNORM) || 
           (Format >= DXGI_FORMAT_BC6H_TYPELESS && Format <= DXGI_FORMAT_BC7_UNORM_SRGB);
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetByteAlignment 
UINT CD3D11FormatHelper::GetByteAlignment(DXGI_FORMAT Format)
{
    UINT bits = GetBitsPerUnit(Format);
    if (!IsBlockCompressFormat(Format))
    {
        bits *= GetWidthAlignment(Format)*GetHeightAlignment(Format)*GetDepthAlignment(Format);
    }

    assert((bits & 0x7) == 0); // Unit must be byte-aligned
    return bits >> 3;
}

//----------------------------------------------------------------------------
// DivideAndRoundUp
inline HRESULT DivideAndRoundUp(UINT dividend, UINT divisor, _Out_ UINT& result)
{
    HRESULT hr = S_OK;

    UINT adjustedDividend;
    hr = UIntAdd(dividend, (divisor - 1), &adjustedDividend);

    result = SUCCEEDED(hr) ? (adjustedDividend / divisor) : 0;

    return hr;
}

//----------------------------------------------------------------------------
// CalculateExtraPlanarRows
HRESULT CD3D11FormatHelper::CalculateExtraPlanarRows(
    DXGI_FORMAT format,
    UINT plane0Height,
    _Out_ UINT& totalHeight
    )
{
    // blockWidth, blockHeight, and blockSize only reflect the size of plane 0.  Each planar format has additonal planes that must
    // be counted.  Each format increases size by another 0.5x, 1x, or 2x.  Grab the number of "half allocation" increments so integer
    // math can be used to calculate the extra size.
    UINT extraHalfHeight = 0;
    UINT round = 0;

    switch (format)
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
        extraHalfHeight = 1;
        round = 1;
        break;

    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_P208:
        extraHalfHeight = 2;
        round = 0;
        break;
    case DXGI_FORMAT_V208:
        extraHalfHeight = 2;
        round = 1;
        break;

    case DXGI_FORMAT_V408:
        extraHalfHeight = 4;
        round = 0;
        break;
    default:
        // Unhandled planar format.
        assert(false);
        break;
    }

    UINT extraPlaneHeight;
    if (FAILED(UIntMult(plane0Height, extraHalfHeight, &extraPlaneHeight))
        || FAILED(UIntAdd(extraPlaneHeight, round, &extraPlaneHeight))
        || FAILED(UIntAdd(plane0Height, (extraPlaneHeight >> 1), &totalHeight)))
    {
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }


    return S_OK;
}

//----------------------------------------------------------------------------
// CalculateResourceSize
HRESULT CD3D11FormatHelper::CalculateResourceSize(
    UINT width, 
    UINT height, 
    UINT depth,
    DXGI_FORMAT format, 
    UINT mipLevels, 
    UINT subresources, 
    _Out_ SIZE_T& totalByteSize, 
    _Out_writes_opt_(subresources) D3D11_MAPPED_SUBRESOURCE *pDst)
{
    UINT tableIndex = GetDetailTableIndexNoThrow( format );
    const FORMAT_DETAIL& formatDetail = s_FormatDetail[tableIndex];

    bool fIsBlockCompressedFormat = IsBlockCompressFormat(format );

    // No format currently requires depth alignment.
    assert(formatDetail.DepthAlignment == 1);

    UINT subWidth = width;
    UINT subHeight = height;
    UINT subDepth = depth;
    for (UINT s = 0, iM = 0, iA = 0; s < subresources; ++s)
    {
        UINT blockWidth;
        if (FAILED(DivideAndRoundUp(subWidth, formatDetail.WidthAlignment, /*_Out_*/ blockWidth)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        UINT blockSize, blockHeight;
        if (fIsBlockCompressedFormat)
        {
            if (FAILED(DivideAndRoundUp(subHeight, formatDetail.HeightAlignment, /*_Out_*/ blockHeight)))
            {
                return INTSAFE_E_ARITHMETIC_OVERFLOW;
            }

            // Block Compressed formats use BitsPerUnit as block size.
            blockSize = formatDetail.BitsPerUnit;
        }
        else
        {
            // The height must *not* be aligned to HeightAlign.  As there is no plane pitch/stride, the expectation is that the 2nd plane
            // begins immediately after the first.  The only formats with HeightAlignment other than 1 are planar or block compressed, and
            // block compressed is handled above.
            assert(formatDetail.bPlanar || formatDetail.HeightAlignment == 1);
            blockHeight = subHeight;

            // Combined with the division os subWidth by the width alignment above, this helps achieve rounding the stride up to an even multiple of
            // block width.  This is especially important for formats like NV12 and P208 whose chroma plane is wider than the luma.
            blockSize = formatDetail.BitsPerUnit * formatDetail.WidthAlignment;
        }

        if (DXGI_FORMAT_UNKNOWN == formatDetail.DXGIFormat)
        {
            blockSize = 8;
        }

        // Convert block width size to bytes.
        assert((blockSize & 0x7) == 0);
        blockSize = blockSize >> 3;

        if (formatDetail.bPlanar)
        {
            if (FAILED(CalculateExtraPlanarRows(format, blockHeight, /*_Out_*/ blockHeight)))
            {
                return INTSAFE_E_ARITHMETIC_OVERFLOW;
            }
        }

        // Calculate rowPitch, depthPitch, and total subresource size.
        UINT rowPitch, depthPitch;
        SIZE_T subresourceByteSize;
        if (   FAILED(UIntMult(blockWidth, blockSize, &rowPitch))
            || FAILED(UIntMult(blockHeight, rowPitch, &depthPitch))
            || FAILED(SIZETMult(subDepth, depthPitch, &subresourceByteSize)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        if (pDst)
        {
            D3D11_MAPPED_SUBRESOURCE& dst = pDst[s];

            // This data will be returned straight from the API to satisfy Map. So, strides/ alignment must be API-correct.
            dst.pData = reinterpret_cast<void*>(totalByteSize);
            assert(s != 0 || dst.pData == NULL);

            dst.RowPitch = rowPitch; 
            dst.DepthPitch = depthPitch;
        }

        // Align the subresource size.
        static_assert((MAP_ALIGN_REQUIREMENT & (MAP_ALIGN_REQUIREMENT - 1)) == 0, "This code expects MAP_ALIGN_REQUIREMENT to be a power of 2.");
        
        SIZE_T subresourceByteSizeAligned;
        if (FAILED(SIZETAdd(subresourceByteSize, MAP_ALIGN_REQUIREMENT - 1, &subresourceByteSizeAligned)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        subresourceByteSizeAligned = subresourceByteSizeAligned & ~(MAP_ALIGN_REQUIREMENT - 1);

        if (FAILED(SIZETAdd(totalByteSize, subresourceByteSizeAligned, &totalByteSize)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        // Iterate over mip levels and array elements
        if (++iM >= mipLevels)
        {
            ++iA;
            iM = 0;

            subWidth = width;
            subHeight = height;
            subDepth = depth;
        }
        else
        {
            subWidth /= (1 == subWidth ? 1 : 2);
            subHeight /= (1 == subHeight ? 1 : 2);
            subDepth /= (1 == subDepth ? 1 : 2);
        }
    }

    return S_OK;
}

inline bool IsPow2( UINT Val )
{
    return 0 == (Val & (Val - 1));
}

// This helper function calculates the Row Pitch for a given format. For Planar formats this function returns
// the row major RowPitch of the resource. The RowPitch is the same for all the planes. For Planar 
// also use the CalculateExtraPlanarRows function to calculate the corresonding height or use the CalculateMinimumRowMajorSlicePitch
// function. For Block Compressed Formats, this function returns the RowPitch of a row of blocks. For packed subsampled formats and other formats, 
// this function returns the row pitch of one single row of pixels.
HRESULT CD3D11FormatHelper::CalculateMinimumRowMajorRowPitch(DXGI_FORMAT Format, UINT Width, _Out_ UINT &RowPitch)
{
    // Early out for DXGI_FORMAT_UNKNOWN special case.
    if (Format == DXGI_FORMAT_UNKNOWN)
    {
        RowPitch = Width;
        return S_OK;
    }

    UINT WidthAlignment = GetWidthAlignment(Format);

    UINT NumUnits;
    if (IsBlockCompressFormat(Format))
    {
        // This function calculates the minimum stride needed for a block row when the format 
        // is block compressed.The GetBitsPerUnit value stored in the format table indicates 
        // the size of a compressed block for block compressed formats.
        assert(WidthAlignment != 0);
        if (FAILED(DivideAndRoundUp(Width, WidthAlignment, NumUnits)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }
    }
    else
    {
        // All other formats must have strides aligned to their width alignment requirements.
        // The Width may not be aligned to the WidthAlignment.  This is not an error for this 
        // function as we expect to allow formats like NV12 to have odd dimensions in the future.

        // The following alignement code expects only pow2 alignment requirements.  Only block 
        // compressed formats currently have non-pow2 alignment requriements.
        assert(IsPow2(WidthAlignment));

        UINT Mask = WidthAlignment - 1;
        if (FAILED(UIntAdd(Width, Mask, &NumUnits)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        NumUnits &= ~Mask;
    }

    if (FAILED(UIntMult(NumUnits, GetBitsPerUnit(Format), &RowPitch)))
    {
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    // This must to always be Byte aligned.
    assert((RowPitch & 7) == 0);
    RowPitch >>= 3;

    return S_OK;
}

// This helper function calculates the SlicePitch for a given format. For Planar formats the slice pitch includes the extra 
// planes.
HRESULT CD3D11FormatHelper::CalculateMinimumRowMajorSlicePitch(DXGI_FORMAT Format, UINT TightRowPitch, UINT Height, _Out_ UINT &SlicePitch)
{
    if (Planar(Format))
    {
        UINT PlanarHeight;
        if (FAILED(CalculateExtraPlanarRows(Format, Height, PlanarHeight)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        return UIntMult(TightRowPitch, PlanarHeight, &SlicePitch);
    }
    else if (Format == DXGI_FORMAT_UNKNOWN)
    {
        return UIntMult(TightRowPitch, Height, &SlicePitch);
    }

    UINT HeightAlignment = GetHeightAlignment(Format);

    // Caution assert to make sure that no new format breaks this assumtion that all HeightAlignment formats are BC or Planar.
    // This is to make sure that Height handled correctly for this calculation.
    assert(HeightAlignment == 1 || IsBlockCompressFormat(Format));

    UINT HeightOfPacked;
    if (FAILED(DivideAndRoundUp(Height, HeightAlignment, HeightOfPacked)))
    {
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    // Multiply by BitsPerUnit which for non block compressed formats has to be the bits per block and for all other non-planar formats
    // is bits per pixel.
    if (FAILED(UIntMult(HeightOfPacked, TightRowPitch, &SlicePitch)))
    {
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    return S_OK;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetBitsPerUnit - returns bits per pixel unless format is a block compress format then it returns bits per block. 
// use IsBlockCompressFormat() to determine if block size is returned.
UINT CD3D11FormatHelper::GetBitsPerUnit(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].BitsPerUnit;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetBitsPerElement legacy function used to maintain 10on9 only. Do not use.
UINT CD3D11FormatHelper::GetBitsPerElement(DXGI_FORMAT Format)
{
    UINT bitsPerUnit = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].BitsPerUnit;
    if (IsBlockCompressFormat( Format ))
    {
        bitsPerUnit /= GetWidthAlignment( Format ) * GetHeightAlignment( Format );
    }
    return bitsPerUnit;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetDetailTableIndexNoThrow
UINT CD3D11FormatHelper::GetDetailTableIndexNoThrow(DXGI_FORMAT  Format)
{
    UINT Index = GetDetailTableIndex( Format );
    assert( -1 != Index ); // Needs to be validated externally.
    return Index;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetNumComponentsInFormat
UINT CD3D11FormatHelper::GetNumComponentsInFormat( DXGI_FORMAT  Format )
{
    UINT n = 0;
    const UINT Index = GetDetailTableIndexNoThrow(Format);
    for( UINT comp = 0; comp < 4; comp++ )
    {
        D3D11_FORMAT_COMPONENT_NAME name = D3D11FCN_D;
        switch(comp)
        {
        case 0: name = s_FormatDetail[Index].ComponentName0; break;
        case 1: name = s_FormatDetail[Index].ComponentName1; break;
        case 2: name = s_FormatDetail[Index].ComponentName2; break;
        case 3: name = s_FormatDetail[Index].ComponentName3; break;
        }
        if( name != D3D11FCN_X )
        {
            n++;
        }
    }
    return n;
}

//---------------------------------------------------------------------------------------------------------------------------------
UINT CD3D11FormatHelper::GetWidthAlignment(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].WidthAlignment;
}

UINT CD3D11FormatHelper::GetHeightAlignment(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].HeightAlignment;
}

UINT CD3D11FormatHelper::GetDepthAlignment(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].DepthAlignment;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatDetail
const CD3D11FormatHelper::FORMAT_DETAIL* CD3D11FormatHelper::GetFormatDetail( DXGI_FORMAT  Format )
{
    const UINT Index = GetDetailTableIndex(Format);
    if( -1 == Index )
    {
        return NULL;
    }
    return &s_FormatDetail[ Index ];
}
//---------------------------------------------------------------------------------------------------------------------------------
// IsSRGBFormat
bool CD3D11FormatHelper::IsSRGBFormat(DXGI_FORMAT Format)
{
    const UINT Index = GetDetailTableIndex(Format);
    if( -1 == Index )
    {
        return false;
    }

    return s_FormatDetail[Index].SRGBFormat ? true : false;
}
//---------------------------------------------------------------------------------------------------------------------------------
// GetParentFormat
DXGI_FORMAT CD3D11FormatHelper::GetParentFormat(DXGI_FORMAT Format)
{
    return s_FormatDetail[Format].ParentFormat;
}
//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatCastSet
const DXGI_FORMAT* CD3D11FormatHelper::GetFormatCastSet(DXGI_FORMAT Format)
{
    return s_FormatDetail[Format].pDefaultFormatCastSet;
}
//---------------------------------------------------------------------------------------------------------------------------------
// GetTypeLevel
D3D11_FORMAT_TYPE_LEVEL CD3D11FormatHelper::GetTypeLevel(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].TypeLevel;
}
//---------------------------------------------------------------------------------------------------------------------------------
// GetComponentName
D3D11_FORMAT_COMPONENT_NAME CD3D11FormatHelper::GetComponentName(DXGI_FORMAT Format, UINT AbsoluteComponentIndex)
{
    D3D11_FORMAT_COMPONENT_NAME name;
    switch( AbsoluteComponentIndex )
    {
    case 0: name = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentName0; break;
    case 1: name = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentName1; break;
    case 2: name = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentName2; break;
    case 3: name = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentName3; break;
    default: throw E_FAIL;
    }
    return name;
}
//---------------------------------------------------------------------------------------------------------------------------------
// GetBitsPerComponent
UINT CD3D11FormatHelper::GetBitsPerComponent(DXGI_FORMAT Format, UINT AbsoluteComponentIndex)
{
    if( AbsoluteComponentIndex > 3 )
    {
        throw E_FAIL;
    }
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].BitsPerComponent[AbsoluteComponentIndex];
}
//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatComponentInterpretation
D3D11_FORMAT_COMPONENT_INTERPRETATION CD3D11FormatHelper::GetFormatComponentInterpretation(DXGI_FORMAT Format, UINT AbsoluteComponentIndex)
{
    D3D11_FORMAT_COMPONENT_INTERPRETATION interp;
    SecureZeroMemory(&interp, sizeof(interp));

    switch( AbsoluteComponentIndex )
    {
    case 0: interp = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentInterpretation0; break;
    case 1: interp = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentInterpretation1; break;
    case 2: interp = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentInterpretation2; break;
    case 3: interp = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentInterpretation3; break;
//    default: throw E_FAIL;
    }
    return interp;
}
//---------------------------------------------------------------------------------------------------------------------------------
// Planar
BOOL CD3D11FormatHelper::Planar(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].bPlanar;
}
//---------------------------------------------------------------------------------------------------------------------------------
// Non-opaque Planar
BOOL CD3D11FormatHelper::NonOpaquePlanar(DXGI_FORMAT Format)
{
    return Planar(Format) && !Opaque(Format);
}
//---------------------------------------------------------------------------------------------------------------------------------
// YUV
BOOL CD3D11FormatHelper::YUV(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].bYUV;
}

//---------------------------------------------------------------------------------------------------------------------------------
// Format family supports stencil
bool CD3D11FormatHelper::FamilySupportsStencil(DXGI_FORMAT Format)
{
    switch( GetParentFormat(Format) )
    {
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R24G8_TYPELESS:
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetYCbCrChromaSubsampling
void CD3D11FormatHelper::GetYCbCrChromaSubsampling(
    DXGI_FORMAT Format, 
    _Out_ UINT& HorizontalSubsampling, 
    _Out_ UINT& VerticalSubsampling
    )
{
    switch( Format)
    {
        // YCbCr 4:2:0 
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
            HorizontalSubsampling =  2;
            VerticalSubsampling = 2;
            break;

        // YCbCr 4:2:2
        case DXGI_FORMAT_P208:
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
            HorizontalSubsampling =  2;
            VerticalSubsampling = 1;
            break;

        // YCbCr 4:4:0
        case DXGI_FORMAT_V208:
            HorizontalSubsampling =  1;
            VerticalSubsampling = 2;
            break;

        // YCbCr 4:4:4
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_V408:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_Y416:
            // Fallthrough

        // YCbCr palletized  4:4:4:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
            HorizontalSubsampling =  1;
            VerticalSubsampling = 1;
            break;

        // YCbCr 4:1:1
        case DXGI_FORMAT_NV11:
            HorizontalSubsampling =  4;
            VerticalSubsampling = 1;
            break;

        default:
            // All YCbCr formats should be in this list.
            assert( !YUV(Format) );
            HorizontalSubsampling =  1;
            VerticalSubsampling = 1;
            break;
    };
}

//---------------------------------------------------------------------------------------------------------------------------------
// Plane count for non-opaque planar formats
UINT CD3D11FormatHelper::NonOpaquePlaneCount(DXGI_FORMAT Format)
{
    if (!CD3D11FormatHelper::NonOpaquePlanar(Format))
    {
        return 1;
    }

    // V208 and V408 are the only 3-plane formats. 
    return (Format == DXGI_FORMAT_V208 || Format == DXGI_FORMAT_V408) ? 3 : 2;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetTileShape
//
// Retrieve Tiled Resource tile shape
void CD3D11FormatHelper::GetTileShape(
    D3D11_TILE_SHAPE* pTileShape, 
    DXGI_FORMAT Format, 
    D3D11_RESOURCE_DIMENSION Dimension, 
    UINT SampleCount
)
{
    UINT BPU = GetBitsPerUnit(Format);
    switch(Dimension)
    {
    case D3D11_RESOURCE_DIMENSION_BUFFER:
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        {
            pTileShape->WidthInTexels = (BPU == 0) ? D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES : D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES*8 / BPU;
            pTileShape->HeightInTexels = 1;
            pTileShape->DepthInTexels = 1;
        }
        break;
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        {
            if (IsBlockCompressFormat(Format))
            {
                // Currently only supported block sizes are 64 and 128.
                // These equations calculate the size in texels for a tile. It relies on the fact that 64 * 64 blocks fit in a tile if the block size is 128 bits.
                assert(BPU == 64 || BPU == 128);
                pTileShape->WidthInTexels = 64 * GetWidthAlignment(Format);
                pTileShape->HeightInTexels = 64 * GetHeightAlignment(Format);
                pTileShape->DepthInTexels = 1;
                if (BPU == 64)
                {
                    // If bits per block are 64 we double width so it takes up the full tile size.
                    assert((Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
                           (Format >= DXGI_FORMAT_BC4_TYPELESS && Format <= DXGI_FORMAT_BC4_SNORM));
                    pTileShape->WidthInTexels *= 2;
                }
            }
            else
            {
                // Not a block format so BPU is bits per pixel.
                pTileShape->DepthInTexels = 1;
                switch(BPU)
                {
                case 8:
                    pTileShape->WidthInTexels = 256;
                    pTileShape->HeightInTexels = 256;
                    break;
                case 16:
                    pTileShape->WidthInTexels = 256;
                    pTileShape->HeightInTexels = 128;
                    break;
                case 32:
                    pTileShape->WidthInTexels = 128;
                    pTileShape->HeightInTexels = 128;
                    break;
                case 64:
                    pTileShape->WidthInTexels = 128;
                    pTileShape->HeightInTexels = 64;
                    break;
                case 128:
                    pTileShape->WidthInTexels = 64;
                    pTileShape->HeightInTexels = 64;
                    break;
                }
                switch(SampleCount)
                {
                case 1:
                    break;
                case 2:
                    pTileShape->WidthInTexels /= 2;
                    pTileShape->HeightInTexels /= 1;
                    break;
                case 4:
                    pTileShape->WidthInTexels /= 2;
                    pTileShape->HeightInTexels /= 2;
                    break;
                case 8:
                    pTileShape->WidthInTexels /= 4;
                    pTileShape->HeightInTexels /= 2;
                    break;
                case 16:
                    pTileShape->WidthInTexels /= 4;
                    pTileShape->HeightInTexels /= 4;
                    break;
                default:
                    ASSUME(false);
                }
            }
        break;
        }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        {
            if (IsBlockCompressFormat(Format))
            {
                // Currently only supported block sizes are 64 and 128.
                // These equations calculate the size in texels for a tile. It relies on the fact that 16*16*16 blocks fit in a tile if the block size is 128 bits.
                assert(BPU == 64 || BPU == 128);
                pTileShape->WidthInTexels = 16 * GetWidthAlignment(Format);
                pTileShape->HeightInTexels = 16 * GetHeightAlignment(Format);
                pTileShape->DepthInTexels = 16 * GetDepthAlignment(Format);
                if (BPU == 64)
                {
                    // If bits per block are 64 we double width so it takes up the full tile size.
                    assert((Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
                           (Format >= DXGI_FORMAT_BC4_TYPELESS && Format <= DXGI_FORMAT_BC4_SNORM));
                    pTileShape->WidthInTexels *= 2;
                }
            }
            else if (Format == DXGI_FORMAT_R8G8_B8G8_UNORM || Format == DXGI_FORMAT_G8R8_G8B8_UNORM)
            {
                //RGBG and GRGB are treated as 2x1 block format
                pTileShape->WidthInTexels = 64;
                pTileShape->HeightInTexels = 32;
                pTileShape->DepthInTexels = 16;
            }
            else
            {
                // Not a block format so BPU is bits per pixel.
                assert(GetWidthAlignment(Format) == 1 && GetHeightAlignment(Format) == 1 && GetDepthAlignment(Format));
                switch(BPU)
                {
                case 8:
                    pTileShape->WidthInTexels = 64;
                    pTileShape->HeightInTexels = 32;
                    pTileShape->DepthInTexels = 32;
                    break;
                case 16:
                    pTileShape->WidthInTexels = 32;
                    pTileShape->HeightInTexels = 32;
                    pTileShape->DepthInTexels = 32;
                    break;
                case 32:
                    pTileShape->WidthInTexels = 32;
                    pTileShape->HeightInTexels = 32;
                    pTileShape->DepthInTexels = 16;
                    break;
                case 64:
                    pTileShape->WidthInTexels = 32;
                    pTileShape->HeightInTexels = 16;
                    pTileShape->DepthInTexels = 16;
                    break;
                case 128:
                    pTileShape->WidthInTexels = 16;
                    pTileShape->HeightInTexels = 16;
                    pTileShape->DepthInTexels = 16;
                    break;
                }
            }
            break;
        }
    }
}



// End of file
