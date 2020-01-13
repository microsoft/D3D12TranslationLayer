// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#define D3DFORMATDESC 1

#define MAP_ALIGN_REQUIREMENT 16 // Map is required to return 16-byte aligned addresses

// ----------------------------------------------------------------------------
// Some enumerations used in the D3D11_FORMAT_DETAIL structure
// ----------------------------------------------------------------------------
typedef enum D3D11_FORMAT_LAYOUT
{
    D3D11FL_STANDARD = 0, // standard layout
    D3D11FL_CUSTOM   = -1  // custom layout
    // Note, 1 bit allocated for this in FORMAT_DETAIL below. If you add fields here, add bits...
    // NOTE SIGNED VALUES ARE USED SINCE COMPILER MAKES ENUMS SIGNED, AND BITFIELDS ARE SIGN EXTENDED ON READ
} D3D11_FORMAT_LAYOUT;

typedef enum D3D11_FORMAT_TYPE_LEVEL
{
    D3D11FTL_NO_TYPE      = 0,
    D3D11FTL_PARTIAL_TYPE = -2,
    D3D11FTL_FULL_TYPE    = -1,
    // Note, 2 bits allocated for this in FORMAT_DETAIL below. If you add fields here, add bits...
    // NOTE SIGNED VALUES ARE USED SINCE COMPILER MAKES ENUMS SIGNED, AND BITFIELDS ARE SIGN EXTENDED ON READ
} D3D11_FORMAT_TYPE_LEVEL;

typedef enum D3D11_FORMAT_COMPONENT_NAME
{
    D3D11FCN_R     = -4,
    D3D11FCN_G     = -3,
    D3D11FCN_B     = -2,
    D3D11FCN_A     = -1,
    D3D11FCN_D     = 0,
    D3D11FCN_S     = 1,
    D3D11FCN_X     = 2,
    // Note, 3 bits allocated for this in FORMAT_DETAIL below. If you add fields here, add bits...
    // NOTE SIGNED VALUES ARE USED SINCE COMPILER MAKES ENUMS SIGNED, AND BITFIELDS ARE SIGN EXTENDED ON READ
} D3D11_FORMAT_COMPONENT_NAME;

typedef enum D3D11_FORMAT_COMPONENT_INTERPRETATION
{
    D3D11FCI_TYPELESS    = 0,
    D3D11FCI_FLOAT       = -4,
    D3D11FCI_SNORM       = -3,
    D3D11FCI_UNORM       = -2,
    D3D11FCI_SINT        = -1,
    D3D11FCI_UINT        = 1,
    D3D11FCI_UNORM_SRGB  = 2,
    D3D11FCI_BIASED_FIXED_2_8   = 3,
    // Note, 3 bits allocated for this in FORMAT_DETAIL below. If you add fields here, add bits...
    // NOTE SIGNED VALUES ARE USED SINCE COMPILER MAKES ENUMS SIGNED, AND BITFIELDS ARE SIGN EXTENDED ON READ
} D3D11_FORMAT_COMPONENT_INTERPRETATION;

// ----------------------------------------------------------------------------
//
// CD3D11FormatHelper
//
// ----------------------------------------------------------------------------
class CD3D11FormatHelper
{
private:
    // ----------------------------------------------------------------------------
    // Information describing everything about a D3D11 Resource Format
    // ----------------------------------------------------------------------------

    // This struct holds information about formats that is feature level and driver version agnostic
    typedef struct FORMAT_DETAIL
    {
        DXGI_FORMAT                 DXGIFormat;
        DXGI_FORMAT                 ParentFormat;
        const DXGI_FORMAT*          pDefaultFormatCastSet;  // This is dependent on FL/driver version, but is here to save a lot of space
        UINT8                       BitsPerComponent[4]; // only used for D3D11FTL_PARTIAL_TYPE or FULL_TYPE
        UINT8                       BitsPerUnit;             // BitsPerUnit is bits per pixel for non-compressed formats and bits per block for compressed formats
        BOOL                        SRGBFormat : 1;
        UINT                        WidthAlignment : 4;      // number of texels to align to in a mip level.
        UINT                        HeightAlignment : 4;     // Top level dimensions must be a multiple of these
        UINT                        DepthAlignment : 1;      // values.
        D3D11_FORMAT_LAYOUT         Layout : 1;
        D3D11_FORMAT_TYPE_LEVEL     TypeLevel : 2;
        D3D11_FORMAT_COMPONENT_NAME ComponentName0 : 3; // RED    ... only used for D3D11FTL_PARTIAL_TYPE or FULL_TYPE
        D3D11_FORMAT_COMPONENT_NAME ComponentName1 : 3; // GREEN  ... only used for D3D11FTL_PARTIAL_TYPE or FULL_TYPE
        D3D11_FORMAT_COMPONENT_NAME ComponentName2 : 3; // BLUE   ... only used for D3D11FTL_PARTIAL_TYPE or FULL_TYPE
        D3D11_FORMAT_COMPONENT_NAME ComponentName3 : 3; // ALPHA  ... only used for D3D11FTL_PARTIAL_TYPE or FULL_TYPE
        D3D11_FORMAT_COMPONENT_INTERPRETATION ComponentInterpretation0 : 3; // only used for D3D11FTL_FULL_TYPE
        D3D11_FORMAT_COMPONENT_INTERPRETATION ComponentInterpretation1 : 3; // only used for D3D11FTL_FULL_TYPE
        D3D11_FORMAT_COMPONENT_INTERPRETATION ComponentInterpretation2 : 3; // only used for D3D11FTL_FULL_TYPE
        D3D11_FORMAT_COMPONENT_INTERPRETATION ComponentInterpretation3 : 3; // only used for D3D11FTL_FULL_TYPE
        bool                        bPlanar : 1;
        bool                        bYUV : 1;
    } FORMAT_DETAIL;

    static const FORMAT_DETAIL       s_FormatDetail[];
    static const LPCSTR              s_FormatNames[]; // separate from above structure so it can be compiled out of runtime.
    static const UINT                s_NumFormats;

public:
    static bool IsBlockCompressFormat(DXGI_FORMAT Format);
    static UINT GetByteAlignment(DXGI_FORMAT Format);
    static HRESULT CalculateResourceSize(UINT width, UINT height, UINT depth, DXGI_FORMAT format, UINT mipLevels, UINT subresources, _Out_ SIZE_T& totalByteSize, _Out_writes_opt_(subresources) D3D11_MAPPED_SUBRESOURCE *pDst = nullptr);
    static HRESULT CalculateExtraPlanarRows(DXGI_FORMAT format, UINT plane0Height, _Out_ UINT& totalHeight);
    static HRESULT CalculateMinimumRowMajorRowPitch(DXGI_FORMAT Format, UINT Width, _Out_ UINT& RowPitch);
    static HRESULT CalculateMinimumRowMajorSlicePitch(DXGI_FORMAT Format, UINT ContextBasedRowPitch, UINT Height, _Out_ UINT& SlicePitch);
    static bool IsSRGBFormat(DXGI_FORMAT Format);
    static UINT  GetNumComponentsInFormat( DXGI_FORMAT  Format );
    // Converts the sequential component index (range from 0 to GetNumComponentsInFormat()) to
    // the absolute component index (range 0 to 3).
    static DXGI_FORMAT                          GetParentFormat(DXGI_FORMAT Format);
    static const DXGI_FORMAT*                   GetFormatCastSet(DXGI_FORMAT Format);
    static D3D11_FORMAT_TYPE_LEVEL              GetTypeLevel(DXGI_FORMAT Format);
    static UINT                                 GetBitsPerUnit(DXGI_FORMAT Format);
    static UINT                                 GetBitsPerElement(DXGI_FORMAT Format); // Legacy function used to support D3D10on9 only. Do not use.
    static UINT                                 GetWidthAlignment(DXGI_FORMAT Format);
    static UINT                                 GetHeightAlignment(DXGI_FORMAT Format);
    static UINT                                 GetDepthAlignment(DXGI_FORMAT Format);
    static D3D11_FORMAT_COMPONENT_NAME          GetComponentName(DXGI_FORMAT Format, UINT AbsoluteComponentIndex);
    static UINT                                 GetBitsPerComponent(DXGI_FORMAT Format, UINT AbsoluteComponentIndex);
    static D3D11_FORMAT_COMPONENT_INTERPRETATION    GetFormatComponentInterpretation(DXGI_FORMAT Format, UINT AbsoluteComponentIndex);
    static BOOL                                 Planar(DXGI_FORMAT Format);
    static BOOL                                 NonOpaquePlanar(DXGI_FORMAT Format);
    static BOOL                                 YUV(DXGI_FORMAT Format);
    static BOOL                                 Opaque(DXGI_FORMAT Format) { return Format == DXGI_FORMAT_420_OPAQUE; }
    static void                                 GetTileShape(D3D11_TILE_SHAPE* pTileShape, DXGI_FORMAT Format, D3D11_RESOURCE_DIMENSION Dimension, UINT SampleCount);
    static bool                                 FamilySupportsStencil(DXGI_FORMAT Format);
    static void                                 GetYCbCrChromaSubsampling(DXGI_FORMAT Format, _Out_ UINT& HorizontalSubsampling, _Out_ UINT& VerticalSubsampling);
    static UINT                                 NonOpaquePlaneCount(DXGI_FORMAT Format);

protected:
    static UINT GetDetailTableIndex(DXGI_FORMAT  Format);
    static UINT GetDetailTableIndexNoThrow(DXGI_FORMAT  Format);
private:
    static const FORMAT_DETAIL* GetFormatDetail( DXGI_FORMAT  Format );
};

// End of file
