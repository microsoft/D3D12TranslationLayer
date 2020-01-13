// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    struct CBufferView {};

    class CSubresourceSubset
    {
    public:
        CSubresourceSubset() noexcept {}
        explicit CSubresourceSubset(UINT8 NumMips, UINT16 NumArraySlices, UINT8 NumPlanes, UINT8 FirstMip = 0, UINT16 FirstArraySlice = 0, UINT8 FirstPlane = 0) noexcept;
        explicit CSubresourceSubset(const CBufferView&);
        explicit CSubresourceSubset(const D3D11_SHADER_RESOURCE_VIEW_DESC1&) noexcept;
        explicit CSubresourceSubset(const D3D11_UNORDERED_ACCESS_VIEW_DESC1&) noexcept;
        explicit CSubresourceSubset(const D3D11_RENDER_TARGET_VIEW_DESC1&) noexcept;
        explicit CSubresourceSubset(const D3D11_DEPTH_STENCIL_VIEW_DESC&) noexcept;
        explicit CSubresourceSubset(const D3D12_SHADER_RESOURCE_VIEW_DESC&) noexcept;
        explicit CSubresourceSubset(const D3D12_UNORDERED_ACCESS_VIEW_DESC&) noexcept;
        explicit CSubresourceSubset(const D3D12_RENDER_TARGET_VIEW_DESC&) noexcept;
        explicit CSubresourceSubset(const D3D12_DEPTH_STENCIL_VIEW_DESC&) noexcept;
        explicit CSubresourceSubset(const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC&, DXGI_FORMAT ResourceFormat) noexcept;
        explicit CSubresourceSubset(const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC&, DXGI_FORMAT ResourceFormat) noexcept;
        explicit CSubresourceSubset(const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC&, DXGI_FORMAT ResourceFormat) noexcept;
        explicit CSubresourceSubset(const VIDEO_DECODER_OUTPUT_VIEW_DESC_INTERNAL&) noexcept;
        explicit CSubresourceSubset(const VIDEO_PROCESSOR_INPUT_VIEW_DESC_INTERNAL&) noexcept;
        explicit CSubresourceSubset(const VIDEO_PROCESSOR_OUTPUT_VIEW_DESC_INTERNAL&) noexcept;

        SIZE_T DoesNotOverlap(const CSubresourceSubset&) const noexcept;
        UINT Mask() const noexcept; // Only useable/used when the result will fit in 32 bits.

        UINT NumNonExtendedSubresources() const noexcept;
        UINT NumExtendedSubresources() const noexcept;

    public:
        UINT16 m_BeginArray; // Also used to store Tex3D slices.
        UINT16 m_EndArray; // End - Begin == Array Slices
        UINT8 m_BeginMip;
        UINT8 m_EndMip; // End - Begin == Mip Levels
        UINT8 m_BeginPlane;
        UINT8 m_EndPlane;
    };

    inline void DecomposeSubresourceIdxNonExtended(UINT Subresource, UINT NumMips, _Out_ UINT& MipLevel, _Out_ UINT& ArraySlice)
    {
        MipLevel = Subresource % NumMips;
        ArraySlice = Subresource / NumMips;
    }

    inline void DecomposeSubresourceIdxNonExtended(UINT Subresource, UINT8 NumMips, _Out_ UINT8& MipLevel, _Out_ UINT16& ArraySlice)
    {
        MipLevel = Subresource % NumMips;
        ArraySlice = static_cast<UINT16>(Subresource / NumMips);
    }

    template <typename T, typename U, typename V>
    inline void DecomposeSubresourceIdxExtended(UINT Subresource, UINT NumMips, UINT ArraySize, _Out_ T& MipLevel, _Out_ U& ArraySlice, _Out_ V& PlaneSlice)
    {
        D3D12DecomposeSubresource(Subresource, NumMips, ArraySize, MipLevel, ArraySlice, PlaneSlice);
    }

    inline UINT DecomposeSubresourceIdxExtendedGetMip(UINT Subresource, UINT NumMips)
    {
        return Subresource % NumMips;
    }

    inline UINT ComposeSubresourceIdxNonExtended(UINT MipLevel, UINT ArraySlice, UINT NumMips)
    {
        return D3D11CalcSubresource(MipLevel, ArraySlice, NumMips);
    }

    inline UINT ComposeSubresourceIdxExtended(UINT MipLevel, UINT ArraySlice, UINT PlaneSlice, UINT NumMips, UINT ArraySize)
    {
        return D3D12CalcSubresource(MipLevel, ArraySlice, PlaneSlice, NumMips, ArraySize);
    }

    inline UINT ComposeSubresourceIdxArrayThenPlane(UINT NumMips, UINT PlaneCount, UINT MipLevel, UINT ArraySlice, UINT PlaneSlice)
    {
        return (ArraySlice * PlaneCount * NumMips) + (PlaneSlice * NumMips) + MipLevel;
    }

    inline UINT ConvertSubresourceIndexAddPlane(UINT Subresource, UINT NumSubresourcesPerPlane, UINT PlaneSlice)
    {
        assert(Subresource < NumSubresourcesPerPlane || PlaneSlice == 0);
        return (Subresource + NumSubresourcesPerPlane * PlaneSlice);
    }

    inline UINT ConvertSubresourceIndexRemovePlane(UINT Subresource, UINT NumSubresourcesPerPlane)
    {
        return (Subresource % NumSubresourcesPerPlane);
    }

    inline UINT GetPlaneIdxFromSubresourceIdx(UINT Subresource, UINT NumSubresourcesPerPlane)
    {
        return (Subresource / NumSubresourcesPerPlane);
    }

    class CViewSubresourceSubset : public CSubresourceSubset
    {
    public:
        enum DepthStencilMode { ReadOnly, WriteOnly, ReadOrWrite };

    public:
        CViewSubresourceSubset() {}
        explicit CViewSubresourceSubset(CSubresourceSubset const& Subresources, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        explicit CViewSubresourceSubset(const CBufferView&);
        CViewSubresourceSubset(const D3D11_SHADER_RESOURCE_VIEW_DESC1& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const D3D11_UNORDERED_ACCESS_VIEW_DESC1& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const D3D11_RENDER_TARGET_VIEW_DESC1& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const D3D11_DEPTH_STENCIL_VIEW_DESC& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const D3D12_RENDER_TARGET_VIEW_DESC& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount, DepthStencilMode DSMode = ReadOrWrite);
        CViewSubresourceSubset(const VIDEO_DECODER_OUTPUT_VIEW_DESC_INTERNAL& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const VIDEO_PROCESSOR_INPUT_VIEW_DESC_INTERNAL& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);
        CViewSubresourceSubset(const VIDEO_PROCESSOR_OUTPUT_VIEW_DESC_INTERNAL& Desc, UINT8 MipLevels, UINT16 ArraySize, UINT8 PlaneCount);

        template<typename T>
        static CViewSubresourceSubset FromView(const T* pView);

    public:
        class CViewSubresourceIterator;

    public:
        CViewSubresourceIterator begin() const;
        CViewSubresourceIterator end() const;
        bool IsWholeResource() const;
        bool IsEmpty() const;
        UINT ArraySize() const;

        UINT MinSubresource() const;
        UINT MaxSubresource() const;

    private:
        void Reduce();

    protected:
        UINT8 m_MipLevels;
        UINT16 m_ArraySlices;
        UINT8 m_PlaneCount;
    };

    // This iterator iterates over contiguous ranges of subresources within a subresource subset. eg:
    //
    // // For each contiguous subresource range.
    // for( CViewSubresourceSubset::CViewSubresourceIterator it = ViewSubset.begin(); it != ViewSubset.end(); ++it )
    // {
    //      // StartSubresource and EndSubresource members of the iterator describe the contiguous range.
    //      for( UINT SubresourceIndex = it.StartSubresource(); SubresourceIndex < it.EndSubresource(); SubresourceIndex++ )
    //      {
    //          // Action for each subresource within the current range.
    //      }
    //  }
    //
    class CViewSubresourceSubset::CViewSubresourceIterator
    {
    public:
        CViewSubresourceIterator(CViewSubresourceSubset const& SubresourceSet, UINT16 ArraySlice, UINT8 PlaneCount);
        CViewSubresourceIterator& operator++();
        CViewSubresourceIterator& operator--();

        bool operator==(CViewSubresourceIterator const& other) const;
        bool operator!=(CViewSubresourceIterator const& other) const;

        UINT StartSubresource() const;
        UINT EndSubresource() const;
        std::pair<UINT, UINT> operator*() const;

    private:
        CViewSubresourceSubset const& m_Subresources;
        UINT16 m_CurrentArraySlice;
        UINT8 m_CurrentPlaneSlice;
    };

    // Some helpers for tiled resource "flow" calculations to determine which subresources are affected by a tiled operation
    void CalcNewTileCoords(D3D11_TILED_RESOURCE_COORDINATE &Coord, UINT &NumTiles, D3D11_SUBRESOURCE_TILING const& SubresourceTiling);

    class CTileSubresourceSubset
    {
    public:
        CTileSubresourceSubset(const D3D11_TILED_RESOURCE_COORDINATE& StartCoord, const D3D11_TILE_REGION_SIZE& Region, D3D11_RESOURCE_DIMENSION ResDim,
            _In_reads_(NumStandardMips) const D3D11_SUBRESOURCE_TILING* pSubresourceTilings, UINT MipLevels, UINT NumStandardMips);

        class CIterator;
        CIterator begin() const;
        CIterator end() const;

    protected:
        UINT CalcSubresource(UINT SubresourceIdx) const;

    protected:
        bool m_bTargetingArraySlices;
        UINT m_FirstSubresource;
        UINT m_NumSubresourcesOrArraySlices;
        UINT m_MipsPerSlice;
    };

    class CTileSubresourceSubset::CIterator
    {
    public:
        CIterator(CTileSubresourceSubset const& TileSubset, UINT SubresourceIdx);
        CIterator& operator++();
        CIterator& operator--();

        bool operator==(CIterator const& other) const;
        bool operator!=(CIterator const& other) const;

        UINT operator*() const;

    private:
        CTileSubresourceSubset const& m_TileSubset;
        UINT m_SubresourceIdx;
    };

    template< bool Supported>
    struct ConvertToDescV1Support
    {
        static const bool supported = Supported;
    };

    typedef ConvertToDescV1Support<false> ConvertToDescV1NotSupported;
    typedef ConvertToDescV1Support<true> ConvertToDescV1Supported;

    template <typename TDescV1>
    struct DescToViewDimension : ConvertToDescV1NotSupported
    {
        static const UINT dimensionTexture2D = 0;
        static const UINT dimensionTexture2DArray = 0;
    };

    template <>
    struct DescToViewDimension< D3D11_SHADER_RESOURCE_VIEW_DESC1 > : ConvertToDescV1Supported
    {
        static const D3D11_SRV_DIMENSION dimensionTexture2D = D3D11_SRV_DIMENSION_TEXTURE2D;
        static const D3D11_SRV_DIMENSION dimensionTexture2DArray = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    };

    template <>
    struct DescToViewDimension< D3D11_RENDER_TARGET_VIEW_DESC1 > : ConvertToDescV1Supported
    {
        static const D3D11_RTV_DIMENSION dimensionTexture2D = D3D11_RTV_DIMENSION_TEXTURE2D;
        static const D3D11_RTV_DIMENSION dimensionTexture2DArray = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    };

    template <>
    struct DescToViewDimension< D3D11_UNORDERED_ACCESS_VIEW_DESC1 > : ConvertToDescV1Supported
    {
        static const D3D11_UAV_DIMENSION dimensionTexture2D = D3D11_UAV_DIMENSION_TEXTURE2D;
        static const D3D11_UAV_DIMENSION dimensionTexture2DArray = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
    };

    template< typename T >
    inline bool IsPow2(
        T num
        )
    {
        static_assert(static_cast<T>(-1) > 0, "Signed type passed to IsPow2");
        return !(num & (num - 1));
    }
};