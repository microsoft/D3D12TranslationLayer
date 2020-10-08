// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    struct  VIDEO_PROCESS_ENUM_ARGS
    {
        D3D12_VIDEO_FIELD_TYPE   InputFieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
        DXGI_RATIONAL            InputFrameRate = {};
        UINT                     InputWidth = 0;
        UINT                     InputHeight = 0;
        DXGI_RATIONAL            OutputFrameRate = {};
        UINT                     OutputWidth = 0;
        UINT                     OutputHeight = 0;
        UINT                     MaxInputStreams = 0;
    };

    typedef enum
    {
        VIDEO_PROCESS_CONVERSION_CAPS_NONE = 0x0,
        VIDEO_PROCESS_CONVERSION_CAPS_LINEAR_SPACE = 0x1,
        VIDEO_PROCESS_CONVERSION_CAPS_xvYCC = 0x2,
        VIDEO_PROCESS_CONVERSION_CAPS_RGB_RANGE_CONVERSION = 0x4,
        VIDEO_PROCESS_CONVERSION_CAPS_YCbCr_MATRIX_CONVERSION = 0x8,
        VIDEO_PROCESS_CONVERSION_CAPS_NOMINAL_RANGE = 0x10
    } VIDEO_PROCESS_CONVERSION_CAPS;

    typedef struct
    {
        D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT dx12Support;
        VIDEO_PROCESS_CONVERSION_CAPS colorConversionCaps;
    } VIDEO_PROCESS_SUPPORT;

    typedef struct {
        DXGI_RATIONAL Input;
        DXGI_RATIONAL Output;
    } FrameRatePair;

    inline bool GetPow2ScaleExponentFromMax(UINT Size, UINT Max, UINT Min, UINT& Exp)
    {
        UINT CurrentSize = Max;
        for (Exp = 0; CurrentSize > Min; CurrentSize = (CurrentSize + 1) / 2, Exp++)
        {
            if (CurrentSize == Size)
            {
                return true;
            }
        }

        return (CurrentSize == Size);
    }

    inline bool IsScaleSupported(const D3D12_VIDEO_SCALE_SUPPORT& ScaleSupport, UINT OutputWidth, UINT OutputHeight)
    {
        bool fSupported =
            OutputWidth != 0
            && OutputWidth <= ScaleSupport.OutputSizeRange.MaxWidth
            && OutputWidth >= ScaleSupport.OutputSizeRange.MinWidth
            && OutputHeight != 0
            && OutputHeight <= ScaleSupport.OutputSizeRange.MaxHeight
            && OutputHeight >= ScaleSupport.OutputSizeRange.MinHeight;

        if (fSupported)
        {
            if ((ScaleSupport.Flags & D3D12_VIDEO_SCALE_SUPPORT_FLAG_POW2_ONLY) != 0)
            {
                UINT Pow2ScaleExpX, Pow2ScaleExpY;
                fSupported = GetPow2ScaleExponentFromMax(OutputWidth, ScaleSupport.OutputSizeRange.MaxWidth, ScaleSupport.OutputSizeRange.MinWidth, Pow2ScaleExpX)
                    && GetPow2ScaleExponentFromMax(OutputHeight, ScaleSupport.OutputSizeRange.MaxHeight, ScaleSupport.OutputSizeRange.MinHeight, Pow2ScaleExpY)
                    && Pow2ScaleExpX == Pow2ScaleExpY;
            }
            else if ((ScaleSupport.Flags & D3D12_VIDEO_SCALE_SUPPORT_FLAG_EVEN_DIMENSIONS_ONLY) != 0)
            {
                fSupported = (OutputWidth & 1) == 0 && (OutputHeight & 1) == 0;
            }
        }

        return fSupported;
    }

    struct ReferenceInfo
    {
        UINT pastFrames = 0;
        UINT futureFrames = 0;
        bool frameRateConversionSupported = false;
    };

    class VideoProcessEnum : public DeviceChild
    {
    public:
        VideoProcessEnum(_In_ ImmediateContext *pDevice) noexcept
            : DeviceChild(pDevice) {}
        virtual ~VideoProcessEnum() noexcept {}

        void Initialize();
        void CheckFeatureSupport(D3D12_FEATURE_VIDEO FeatureVideo, _Inout_updates_bytes_(FeatureSupportDataSize)void* pFeatureSupportData, UINT FeatureSupportDataSize);
        const std::vector<VIDEO_PROCESS_SUPPORT> &GetVPCapsSupportTuples() const { return m_vpCapsSupportTuples;  }
        const std::vector<DXGI_FORMAT> & GetVPInputFormats() const { return m_vpInputFormats; }
        const std::vector<DXGI_FORMAT> & GetVPOutputFormats() const { return m_vpOutputFormats; }
        UINT GetVPInputFormatCount() { return (UINT)m_vpInputFormats.size(); }
        UINT GetVPOutputFormatCount() { return (UINT)m_vpOutputFormats.size(); }
        virtual void CacheVideoProcessInfo(_In_ VIDEO_PROCESS_ENUM_ARGS &args);
        ReferenceInfo UpdateReferenceInfo(D3D12_VIDEO_PROCESS_DEINTERLACE_FLAGS DeinterlaceSupport);
        D3D12_VIDEO_PROCESS_DEINTERLACE_FLAGS GetDeinterlaceSupport() { return m_deinterlaceFlags; }
        bool IsAutoProcessingSupported() { return m_autoprocessingSupported; }

    protected:
        bool IsSupported(_In_ const D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT &dx12Support, UINT OutputWidth, UINT OutputHeight);
        void UpdateReferenceInfo(_In_ D3D12_FEATURE_DATA_VIDEO_PROCESS_REFERENCE_INFO &referenceInfo, _In_ DXGI_RATIONAL &inputFrameRate, _In_ DXGI_RATIONAL &outputFrameRate, _Inout_ UINT &pastFrames, _Inout_ UINT &futureFrames);

        unique_comptr<ID3D12VideoDevice> m_spVideoDevice;
        std::vector<VIDEO_PROCESS_SUPPORT> m_vpCapsSupportTuples;
        std::vector<DXGI_FORMAT> m_vpInputFormats;
        std::vector<DXGI_FORMAT> m_vpOutputFormats;
        D3D12_VIDEO_PROCESS_DEINTERLACE_FLAGS m_deinterlaceFlags = D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE;
        bool m_autoprocessingSupported = false;
    };
};
