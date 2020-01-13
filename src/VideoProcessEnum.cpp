// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include <dxva.h>
#include <intsafe.h>

namespace D3D12TranslationLayer
{
    //----------------------------------------------------------------------------------------------------------------------------------
    void VideoProcessEnum::Initialize()
    {
        if (!m_pParent->m_pDevice12_1)
        {
            ThrowFailure(E_NOINTERFACE);
        }
        ThrowFailure(m_pParent->m_pDevice12_1->QueryInterface(&m_spVideoDevice));
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcessEnum::CheckFeatureSupport(D3D12_FEATURE_VIDEO FeatureVideo, void* pFeatureSupportData, UINT FeatureSupportDataSize)
    {
        switch (FeatureVideo)
        {
        case D3D12_FEATURE_VIDEO_PROCESS_SUPPORT:
        {
            SetFeatureDataNodeIndex<D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT>(pFeatureSupportData, FeatureSupportDataSize, m_pParent->GetNodeIndex());
            break;
        }

        case D3D12_FEATURE_VIDEO_PROCESS_MAX_INPUT_STREAMS:
        {
            SetFeatureDataNodeIndex<D3D12_FEATURE_DATA_VIDEO_PROCESS_MAX_INPUT_STREAMS>(pFeatureSupportData, FeatureSupportDataSize, m_pParent->GetNodeIndex());
            break;
        }

        case D3D12_FEATURE_VIDEO_PROCESS_REFERENCE_INFO:
        {
            SetFeatureDataNodeIndex<D3D12_FEATURE_DATA_VIDEO_PROCESS_REFERENCE_INFO>(pFeatureSupportData, FeatureSupportDataSize, m_pParent->GetNodeIndex());
            break;
        }

        default:
            ThrowFailure(E_NOTIMPL);
            break;
        }
        ThrowFailure(m_spVideoDevice->CheckFeatureSupport(FeatureVideo, pFeatureSupportData, FeatureSupportDataSize));

        if (FeatureVideo == D3D12_FEATURE_VIDEO_PROCESS_MAX_INPUT_STREAMS)
        {
            if (FeatureSupportDataSize == sizeof(D3D12_FEATURE_DATA_VIDEO_PROCESS_MAX_INPUT_STREAMS))
            {
                D3D12_FEATURE_DATA_VIDEO_PROCESS_MAX_INPUT_STREAMS *pMaxInputStreamsData = static_cast<D3D12_FEATURE_DATA_VIDEO_PROCESS_MAX_INPUT_STREAMS *>(pFeatureSupportData);
                if (pMaxInputStreamsData->MaxInputStreams < MIN_SUPPORTED_INPUT_STREAMS_VIA_EMULATION)
                {
                    pMaxInputStreamsData->MaxInputStreams = MIN_SUPPORTED_INPUT_STREAMS_VIA_EMULATION;
                }
            }
            else
            {
                ThrowFailure(E_INVALIDARG);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcessEnum::UpdateReferenceInfo(D3D12_FEATURE_DATA_VIDEO_PROCESS_REFERENCE_INFO &referenceInfo, DXGI_RATIONAL &inputFrameRate, DXGI_RATIONAL &outputFrameRate, UINT &pastFrames, UINT &futureFrames)
    {
        referenceInfo.InputFrameRate = inputFrameRate;
        referenceInfo.OutputFrameRate = outputFrameRate;
        m_spVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_REFERENCE_INFO, &referenceInfo, sizeof(referenceInfo));
        if (referenceInfo.PastFrames > pastFrames)
        {
            pastFrames = referenceInfo.PastFrames;
        }
        if (referenceInfo.FutureFrames > futureFrames)
        {
            futureFrames = referenceInfo.FutureFrames;
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    bool VideoProcessEnum::IsSupported(const D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT &dx12Support, UINT outputWidth = 0, UINT outputHeight = 0)
    {
        ThrowFailure(m_spVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_SUPPORT, (void*)&dx12Support, sizeof(dx12Support)));
        if ((dx12Support.SupportFlags & D3D12_VIDEO_PROCESS_SUPPORT_FLAG_SUPPORTED) == D3D12_VIDEO_PROCESS_SUPPORT_FLAG_SUPPORTED)
        {
            if ((outputWidth == 0 && outputHeight == 0) || IsScaleSupported(dx12Support.ScaleSupport, outputWidth, outputHeight))
            {
                return true;
            }
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    ReferenceInfo VideoProcessEnum::UpdateReferenceInfo(D3D12_VIDEO_PROCESS_DEINTERLACE_FLAGS DeinterlaceSupport)
    {
        //
        // for each of the supported tuples, get sample support for frame rate conversion/past and future frames required
        //
        FrameRatePair frameRatePairs[] =
        {
            { { 30, 1 },{ 60, 1 } },
            { { 30000, 1001 },{ 60000, 1001 } },
            { { 30000, 1001 },{ 60, 1 } },
            { { 30, 1 },{ 60000, 1001 } },
            { { 60, 1 },{ 30, 1 } },
            { { 30, 1 },{ 24, 1 } },
            { { 24, 1 },{ 30, 1 } }
        };

        ReferenceInfo updateResults;

        // TODO: inverse telecine? we could do several rates and verify the modes.

        for (auto& vpSupportTuple : m_vpCapsSupportTuples)
        {
            // past/future frames
            {
                D3D12_FEATURE_DATA_VIDEO_PROCESS_REFERENCE_INFO referenceInfo = {};
                referenceInfo.DeinterlaceMode = DeinterlaceSupport;
                referenceInfo.Filters = vpSupportTuple.dx12Support.FilterSupport;
                referenceInfo.FeatureSupport = vpSupportTuple.dx12Support.FeatureSupport;
                referenceInfo.EnableAutoProcessing = (vpSupportTuple.dx12Support.AutoProcessingSupport != 0) ? TRUE : FALSE;
                UpdateReferenceInfo(referenceInfo, vpSupportTuple.dx12Support.InputFrameRate, vpSupportTuple.dx12Support.OutputFrameRate, updateResults.pastFrames, updateResults.futureFrames);
                for (auto& frameRate : frameRatePairs)
                {
                    UpdateReferenceInfo(referenceInfo, frameRate.Input, frameRate.Output, updateResults.pastFrames, updateResults.futureFrames);
                }
            }

            // and now, we do frame rates conversion support, adding the optional input frame rates
            if (!updateResults.frameRateConversionSupported)
            {
                if (vpSupportTuple.dx12Support.InputFrameRate.Numerator != vpSupportTuple.dx12Support.OutputFrameRate.Numerator ||
                    vpSupportTuple.dx12Support.InputFrameRate.Denominator != vpSupportTuple.dx12Support.OutputFrameRate.Denominator)
                {
                    updateResults.frameRateConversionSupported = true;
                }
                else
                {
                    for (auto& frameRate : frameRatePairs)
                    {
                        D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT frameRateConversionSupport = vpSupportTuple.dx12Support;
                        frameRateConversionSupport.InputFrameRate = frameRate.Input;
                        frameRateConversionSupport.OutputFrameRate = frameRate.Output;
                        if (IsSupported(frameRateConversionSupport))
                        {
                            updateResults.frameRateConversionSupported = true;
                            break;
                        }
                    }
                }
            }
        }

        return updateResults;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcessEnum::CacheVideoProcessInfo(VIDEO_PROCESS_ENUM_ARGS &args)
    {
        VIDEO_PROCESS_SUPPORT vpRGBSupportArray[] = {
            {
                m_pParent->GetNodeIndex(),
                { args.InputWidth, args.InputHeight,{ DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 } },
                args.InputFieldType,
                D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
                args.InputFrameRate,
                { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
                D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
                args.OutputFrameRate
            },
            {
                m_pParent->GetNodeIndex(),
                { args.InputWidth, args.InputHeight,{ DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 } },
                args.InputFieldType,
                D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
                args.InputFrameRate,
                { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
                D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
                args.OutputFrameRate
            }
        };
        struct {
            DXGI_COLOR_SPACE_TYPE inputColorSpace;
            DXGI_COLOR_SPACE_TYPE outputColorSpace;
            VIDEO_PROCESS_CONVERSION_CAPS conversionCap;
        } colorSpaceTuplesRGB[] = {
            // same color space for input & output
            { DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,      DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,        VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,      DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,        VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,   DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,     VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709,    DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709,      VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020,   DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020,     VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020, DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020,   VIDEO_PROCESS_CONVERSION_CAPS_NONE },

            // full <-> studio conversions
            { DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,      DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709,      VIDEO_PROCESS_CONVERSION_CAPS_RGB_RANGE_CONVERSION },
            { DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709,    DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,        VIDEO_PROCESS_CONVERSION_CAPS_RGB_RANGE_CONVERSION },
        };

        VIDEO_PROCESS_SUPPORT vpYUVSupportArray[] = {
            {
                m_pParent->GetNodeIndex(),
                { args.InputWidth, args.InputHeight,{ DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 } },
                args.InputFieldType,
                D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
                args.InputFrameRate,
                { DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
                D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
                args.OutputFrameRate
            }
        };
        struct {
            DXGI_COLOR_SPACE_TYPE inputColorSpace;
            DXGI_COLOR_SPACE_TYPE outputColorSpace;
            VIDEO_PROCESS_CONVERSION_CAPS conversionCap;
        } colorSpaceTuplesYUV[] = {
            // same color space for input & output
            { DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709,    DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709,      VIDEO_PROCESS_CONVERSION_CAPS_NOMINAL_RANGE },
            { DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601,    DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601,      VIDEO_PROCESS_CONVERSION_CAPS_NOMINAL_RANGE },
            { DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,  DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,    VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601,  DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601,    VIDEO_PROCESS_CONVERSION_CAPS_NONE },

            // full <-> studio conversions
            { DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709,    DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,    VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601,    DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601,    VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,  DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709,      VIDEO_PROCESS_CONVERSION_CAPS_NONE },
            { DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601,  DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601,      VIDEO_PROCESS_CONVERSION_CAPS_NONE },

            // 601 <-> 709 conversions
            { DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709,    DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601,      VIDEO_PROCESS_CONVERSION_CAPS_YCbCr_MATRIX_CONVERSION },
            { DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601,    DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709,      VIDEO_PROCESS_CONVERSION_CAPS_YCbCr_MATRIX_CONVERSION },
            { DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601,  DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,    VIDEO_PROCESS_CONVERSION_CAPS_YCbCr_MATRIX_CONVERSION },
            { DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,  DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601,    VIDEO_PROCESS_CONVERSION_CAPS_YCbCr_MATRIX_CONVERSION },
        };

        //
        // Fill VP caps support sample tuple array
        //
        m_deinterlaceFlags = D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE;
        m_autoprocessingSupported = false;
        m_vpCapsSupportTuples.reserve(m_vpCapsSupportTuples.size() + _countof(colorSpaceTuplesRGB) * _countof(vpRGBSupportArray) + _countof(colorSpaceTuplesYUV) * _countof(vpYUVSupportArray));
        for (auto& vpRGBSupport : vpRGBSupportArray)
        {
            for (auto& tuple : colorSpaceTuplesRGB)
            {
                vpRGBSupport.dx12Support.InputSample.Format.ColorSpace = tuple.inputColorSpace;
                vpRGBSupport.dx12Support.OutputFormat.ColorSpace = tuple.outputColorSpace;
                if (IsSupported(vpRGBSupport.dx12Support, args.OutputWidth, args.OutputHeight))
                {
                    vpRGBSupport.colorConversionCaps = tuple.conversionCap;
                    m_vpCapsSupportTuples.push_back(vpRGBSupport);

                    m_deinterlaceFlags |= vpRGBSupport.dx12Support.DeinterlaceSupport;

                    if (vpRGBSupport.dx12Support.AutoProcessingSupport != D3D12_VIDEO_PROCESS_AUTO_PROCESSING_FLAG_NONE)
                    {
                        m_autoprocessingSupported = true;
                    }
                }
            }
        }
        for (auto& vpYUVSupport : vpYUVSupportArray)
        {
            for (auto& tuple : colorSpaceTuplesYUV)
            {
                vpYUVSupport.dx12Support.InputSample.Format.ColorSpace = tuple.inputColorSpace;
                vpYUVSupport.dx12Support.OutputFormat.ColorSpace = tuple.outputColorSpace;
                if (IsSupported(vpYUVSupport.dx12Support, args.OutputWidth, args.OutputHeight))
                {
                    vpYUVSupport.colorConversionCaps = tuple.conversionCap;
                    vpYUVSupport.dx12Support.DeinterlaceSupport |= 
                        args.InputFieldType == D3D12_VIDEO_FIELD_TYPE_NONE ?
                            D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE : D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_BOB;
                    m_vpCapsSupportTuples.push_back(vpYUVSupport);
                    m_deinterlaceFlags |= vpYUVSupport.dx12Support.DeinterlaceSupport;
                }
            }
        }

        //
        // Get sample list of possible input and output VP formats
        // 
        const D3D12_VIDEO_FORMAT videoFormats[] = {
            { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
            { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
            { DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
            { DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
            { DXGI_FORMAT_P010, DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020 },
            { DXGI_FORMAT_P016, DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020 },
            { DXGI_FORMAT_420_OPAQUE, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
            { DXGI_FORMAT_YUY2, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
            { DXGI_FORMAT_AYUV, DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 },
            { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 },
        };

        D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT dx12Support = {
            m_pParent->GetNodeIndex(),
            { args.InputWidth, args.InputHeight,{ DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 } },
            args.InputFieldType,
            D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
            args.InputFrameRate,
            { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 },
            D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
            args.OutputFrameRate
        };

        m_vpInputFormats.reserve(_countof(videoFormats));
        m_vpOutputFormats.reserve(_countof(videoFormats));
        for (auto& inputVideoFormat : videoFormats)
        {
            for (auto& outputVideoFormat : videoFormats)
            {
                dx12Support.InputSample.Format = inputVideoFormat;
                dx12Support.OutputFormat = outputVideoFormat;
                if (IsSupported(dx12Support, args.OutputWidth, args.OutputHeight))
                {
                    if (std::find(m_vpInputFormats.begin(), m_vpInputFormats.end(), inputVideoFormat.Format) == m_vpInputFormats.end())
                    {
                        assert((m_deinterlaceFlags & D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_BOB) != 0  ||
                               inputVideoFormat.Format != DXGI_FORMAT_NV12 ||
                               args.InputFieldType == D3D12_VIDEO_FIELD_TYPE_NONE);
                        m_vpInputFormats.push_back(inputVideoFormat.Format);
                    }
                    if (std::find(m_vpOutputFormats.begin(), m_vpOutputFormats.end(), outputVideoFormat.Format) == m_vpOutputFormats.end())
                    {
                        m_vpOutputFormats.push_back(outputVideoFormat.Format);
                    }
                }
            }
        }
    }
};
