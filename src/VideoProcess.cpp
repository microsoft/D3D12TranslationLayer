// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include <dxva.h>
#include <intsafe.h>
#include "VideoProcessShaders.h"

namespace D3D12TranslationLayer
{
    void ColorConvertNormalized(_In_reads_(4) const FLOAT normInput[4], DXGI_COLOR_SPACE_TYPE inputColorSpace, _Out_writes_(4) FLOAT normOutput[4], DXGI_COLOR_SPACE_TYPE outputColorSpace);

    VideoProcessor::VideoProcessor(
        ImmediateContext *pContext, 
        ID3D12VideoDevice* pVideoDeviceNoRef, 
        const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC* pOutputStreamDesc, 
        UINT NumInputStreamDescs, 
        const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC *pInputStreamDescs
        ) : DeviceChildImpl(pContext)
    {
        ThrowFailure(pVideoDeviceNoRef->CreateVideoProcessor(m_pParent->GetNodeMask(), pOutputStreamDesc, NumInputStreamDescs, pInputStreamDescs, IID_PPV_ARGS(GetForCreate())));
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void VideoProcess::Initialize()
    {
        if (!m_pParent->m_pDevice12_1)
        {
            ThrowFailure(E_NOINTERFACE);
        }
        ThrowFailure(m_pParent->m_pDevice12_1->QueryInterface(&m_spVideoDevice));

        D3D12_FEATURE_DATA_VIDEO_PROCESS_MAX_INPUT_STREAMS maxInputStreamsData = { m_pParent->GetNodeIndex() };
        ThrowFailure(m_spVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_MAX_INPUT_STREAMS, &maxInputStreamsData, sizeof(maxInputStreamsData)));
        m_driverSupportedMaxInputStreams = maxInputStreamsData.MaxInputStreams;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    VideoProcess::~VideoProcess() noexcept
    {
        m_pParent->Flush(COMMAND_LIST_TYPE_VIDEO_PROCESS_MASK);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcess::UpdateInputDescriptor(VIDEO_PROCESS_INPUT_ARGUMENTS* pInputArguments, UINT NumInputStreams, VIDEO_PROCESS_OUTPUT_ARGUMENTS* pOutputArguments, bool &updated)
    {
        if (NumInputStreams != m_creationInputDesc.size())
        {
            updated = true;
            m_creationInputDesc.resize(NumInputStreams);
        }


        for (UINT i = 0; i < NumInputStreams; i++)
        {
            D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC& inputDesc = pInputArguments->D3D12InputStreamDesc[i];

            UpdateNeededMaxPastFutureFrames(inputDesc, pOutputArguments->D3D12OutputStreamDesc);

            if (!updated  &&  memcmp(&inputDesc, &m_creationInputDesc[i], sizeof(inputDesc)) != 0)
            {
                updated = true;
            }
            if (updated)
            {
                m_creationInputDesc[i] = inputDesc;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcess::UpdateOutputDescriptor(VIDEO_PROCESS_OUTPUT_ARGUMENTS* pOutputArguments, bool &updated)
    {
        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC& outputDesc = pOutputArguments->D3D12OutputStreamDesc;

        if (memcmp(&outputDesc, &m_creationOutputDesc, sizeof(pOutputArguments->D3D12OutputStreamDesc)) != 0)
        {
            m_creationOutputDesc = outputDesc;
            updated = true;
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcess::InitializeProcessor(VIDEO_PROCESS_INPUT_ARGUMENTS* pInputArguments, UINT NumInputStreams, VIDEO_PROCESS_OUTPUT_ARGUMENTS* pOutputArguments)
    {
        bool updated = false;
        UpdateInputDescriptor(pInputArguments, NumInputStreams, pOutputArguments, updated);
        UpdateOutputDescriptor(pOutputArguments, updated);

        if (!m_spVideoProcessor  || updated)
        {
            // We need to create the video processor obeying the maximum number of input streams it supports
            NumInputStreams = NumInputStreams > m_driverSupportedMaxInputStreams ? m_driverSupportedMaxInputStreams : NumInputStreams;

            m_spVideoProcessor.reset();
            m_spVideoProcessor = std::make_unique<VideoProcessor>(m_pParent, m_spVideoDevice.get(), &m_creationOutputDesc, NumInputStreams, m_creationInputDesc.data());
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcess::UpdateNeededMaxPastFutureFrames(D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC &inputDesc, const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC &outputDesc)
    {
        D3D12_FEATURE_DATA_VIDEO_PROCESS_REFERENCE_INFO referenceInfo = {};
        referenceInfo.NodeIndex = m_pParent->GetNodeIndex();

        D3D12_VIDEO_PROCESS_FEATURE_FLAGS featureFlags = D3D12_VIDEO_PROCESS_FEATURE_FLAG_NONE;
        featureFlags |= outputDesc.AlphaFillMode ? D3D12_VIDEO_PROCESS_FEATURE_FLAG_ALPHA_FILL : D3D12_VIDEO_PROCESS_FEATURE_FLAG_NONE;
        featureFlags |= inputDesc.LumaKey.Enable ? D3D12_VIDEO_PROCESS_FEATURE_FLAG_LUMA_KEY : D3D12_VIDEO_PROCESS_FEATURE_FLAG_NONE;
        featureFlags |= (inputDesc.StereoFormat != D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE || outputDesc.EnableStereo) ? D3D12_VIDEO_PROCESS_FEATURE_FLAG_STEREO : D3D12_VIDEO_PROCESS_FEATURE_FLAG_NONE;
        featureFlags |= inputDesc.EnableOrientation ? D3D12_VIDEO_PROCESS_FEATURE_FLAG_ROTATION | D3D12_VIDEO_PROCESS_FEATURE_FLAG_FLIP : D3D12_VIDEO_PROCESS_FEATURE_FLAG_NONE;
        featureFlags |= inputDesc.EnableAlphaBlending ? D3D12_VIDEO_PROCESS_FEATURE_FLAG_ALPHA_BLENDING : D3D12_VIDEO_PROCESS_FEATURE_FLAG_NONE;

        referenceInfo.DeinterlaceMode = inputDesc.DeinterlaceMode;
        referenceInfo.Filters = inputDesc.FilterFlags;
        referenceInfo.FeatureSupport = featureFlags;
        referenceInfo.InputFrameRate = inputDesc.FrameRate;
        referenceInfo.OutputFrameRate = outputDesc.FrameRate;
        referenceInfo.EnableAutoProcessing = inputDesc.EnableAutoProcessing;

        m_spVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_REFERENCE_INFO, &referenceInfo, sizeof(referenceInfo));

        inputDesc.NumPastFrames = referenceInfo.PastFrames;
        inputDesc.NumFutureFrames = referenceInfo.FutureFrames;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcess::ProcessFrames(VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, UINT NumInputStreams, VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments)
    {
        // translate input arguments
        for (DWORD stream = 0; stream < NumInputStreams; stream++)
        {
            DWORD nViews = pInputArguments->D3D12InputStreamDesc[stream].StereoFormat == D3D12_VIDEO_FRAME_STEREO_FORMAT_SEPARATE ? 2 : 1;
            for (DWORD view = 0; view < nViews; view++)
            {
                pInputArguments->PrepareResources(stream, view);
            }
            pInputArguments->PrepareStreamArguments(stream);
        }

        // Do VP blit for the number of supported streams by the driver
        UINT NumInputStreamsForVPBlit = min(NumInputStreams, m_driverSupportedMaxInputStreams);

        // translate output arguments
        pOutputArguments->PrepareResources();
        pOutputArguments->PrepareTransform();

        // pre-process resources using shaders if necessary
        m_Deinterlace.Process(pInputArguments, NumInputStreamsForVPBlit, pOutputArguments);

        InitializeProcessor(pInputArguments, NumInputStreamsForVPBlit, pOutputArguments);

        // transition resource states.
        for (DWORD stream = 0; stream < NumInputStreamsForVPBlit; stream++)
        {
            DWORD nViews = pInputArguments->D3D12InputStreamDesc[stream].StereoFormat == D3D12_VIDEO_FRAME_STEREO_FORMAT_SEPARATE ? 2 : 1;
            for (DWORD view = 0; view < nViews; view++)
            {
                pInputArguments->TransitionResources(m_pParent, stream, view);
            }
        }

        pOutputArguments->TransitionResources(m_pParent);

        m_pParent->GetResourceStateManager().ApplyAllResourceTransitions();

        // submit ProcessFrame
        m_pParent->GetVideoProcessCommandList()->ProcessFrames1(
            m_spVideoProcessor->GetForUse(COMMAND_LIST_TYPE::VIDEO_PROCESS),
            &pOutputArguments->D3D12OutputStreamArguments,
            NumInputStreamsForVPBlit,
            pInputArguments->D3D12InputStreamArguments.data());
        m_pParent->SubmitCommandList(COMMAND_LIST_TYPE::VIDEO_PROCESS);  // throws

        // now Blit remaining streams (if needed)
        if (NumInputStreamsForVPBlit < NumInputStreams)
        {
            EmulateVPBlit(pInputArguments, NumInputStreams, pOutputArguments, NumInputStreamsForVPBlit);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    _Use_decl_annotations_
    void VideoProcess::EmulateVPBlit(VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, UINT NumInputStreams, VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments, UINT StartStream)
    {
        DWORD nDstViews = pOutputArguments->D3D12OutputStreamDesc.EnableStereo ? 2 : 1;
        for (DWORD dstView = 0; dstView < nDstViews; dstView++)
        {
            for (UINT stream = StartStream; stream < NumInputStreams; stream++)
            {
                D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS1& inputArgs = pInputArguments->D3D12InputStreamArguments[stream];
                VIDEO_PROCESS_STREAM_INFO& inputInfo = pInputArguments->StreamInfo[stream];

                DWORD nSrcViews = pInputArguments->D3D12InputStreamDesc[stream].StereoFormat == D3D12_VIDEO_FRAME_STEREO_FORMAT_SEPARATE ? 2 : 1;
                for (DWORD srcView = 0; srcView < nSrcViews; srcView++)
                {
                    m_pParent->m_BlitHelper.Blit(
                        inputInfo.ResourceSet[srcView].CurrentFrame.pResource, // pSrc
                        inputInfo.ResourceSet[srcView].CurrentFrame.SubresourceSubset.MinSubresource(), // SrcSubresourceIdx
                        inputArgs.Transform.SourceRectangle, // SrcRect
                        pOutputArguments->CurrentFrame[dstView].pResource, // pDst
                        pOutputArguments->CurrentFrame[dstView].SubresourceSubset.MinSubresource(), // DstSubresourceIdx
                        inputArgs.Transform.DestinationRectangle // DstRect
                    );
                }

            }
        }
    }

    //
    // Reads from the VIDEO_PROCESS_INPUT_ARGUMENTS' ResourceSet, building D3D12_VIDEO_PROCESS_INPUT_ARGUMENTS InputStream data
    //
    void VIDEO_PROCESS_INPUT_ARGUMENTS::PrepareResources(_In_ UINT stream, _In_ UINT view)
    {
        VIDEO_PROCESS_STREAM_INFO *pStreamInfo = &StreamInfo[stream];
        D3D12_VIDEO_PROCESS_INPUT_STREAM *pD3D12InputStream = &D3D12InputStreamArguments[stream].InputStream[view];

        // Fill current frame into D3D12 input stream arguments
        Resource *pCurFrame = pStreamInfo->ResourceSet[view].CurrentFrame.pResource;
        UINT subresource = pStreamInfo->ResourceSet[view].CurrentFrame.SubresourceSubset.MinSubresource();
        pD3D12InputStream->pTexture2D = pCurFrame->GetUnderlyingResource();
        pD3D12InputStream->Subresource = subresource;

        // Fill past frames into D3D12 input stream arguments
        UINT NumPastFrames = (UINT)pStreamInfo->ResourceSet[view].PastFrames.size();
        pD3D12InputStream->ReferenceSet.NumPastFrames = NumPastFrames;
        if (NumPastFrames)
        {
            pStreamInfo->ResourceSet[view].PastSubresources.resize(NumPastFrames);
            pStreamInfo->ResourceSet[view].D3D12ResourcePastFrames.resize(NumPastFrames);
            pD3D12InputStream->ReferenceSet.ppPastFrames = pStreamInfo->ResourceSet[view].D3D12ResourcePastFrames.data();
            pD3D12InputStream->ReferenceSet.pPastSubresources = pStreamInfo->ResourceSet[view].PastSubresources.data();
            for (DWORD pastFrame = 0; pastFrame < NumPastFrames; pastFrame++)
            {
                Resource *pPastFrame = pStreamInfo->ResourceSet[view].PastFrames[pastFrame].pResource;
                subresource = pStreamInfo->ResourceSet[view].PastFrames[pastFrame].SubresourceSubset.MinSubresource();
                pD3D12InputStream->ReferenceSet.ppPastFrames[pastFrame] = pPastFrame->GetUnderlyingResource();
                pD3D12InputStream->ReferenceSet.pPastSubresources[pastFrame] = subresource;
            }
        }

        UINT NumFutureFrames = (UINT)pStreamInfo->ResourceSet[view].FutureFrames.size();
        pD3D12InputStream->ReferenceSet.NumFutureFrames = NumFutureFrames;
        if (NumFutureFrames)
        {
            pStreamInfo->ResourceSet[view].FutureSubresources.resize(NumFutureFrames);
            pStreamInfo->ResourceSet[view].D3D12ResourceFutureFrames.resize(NumFutureFrames);
            pD3D12InputStream->ReferenceSet.ppFutureFrames = pStreamInfo->ResourceSet[view].D3D12ResourceFutureFrames.data();
            pD3D12InputStream->ReferenceSet.pFutureSubresources = pStreamInfo->ResourceSet[view].FutureSubresources.data();
            for (DWORD FutureFrame = 0; FutureFrame < NumFutureFrames; FutureFrame++)
            {
                Resource *pFutureFrame = pStreamInfo->ResourceSet[view].FutureFrames[FutureFrame].pResource;
                subresource = pStreamInfo->ResourceSet[view].FutureFrames[FutureFrame].SubresourceSubset.MinSubresource();
                pD3D12InputStream->ReferenceSet.ppFutureFrames[FutureFrame] = pFutureFrame->GetUnderlyingResource();
                pD3D12InputStream->ReferenceSet.pFutureSubresources[FutureFrame] = subresource;
            }
        }
        {
            const D3D12_RESOURCE_DESC &desc = D3D12InputStreamArguments[stream].InputStream[view].pTexture2D->GetDesc();
            D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC& inputDesc = D3D12InputStreamDesc[stream];

            inputDesc.Format = desc.Format;
            // set default colorspace if not set, based on DXGI format
            if (!StreamInfo[stream].ColorSpaceSet)
            {
                inputDesc.ColorSpace = CDXGIColorSpaceHelper::ConvertFromLegacyColorSpace(!CD3D11FormatHelper::YUV(desc.Format), CD3D11FormatHelper::GetBitsPerUnit(desc.Format), /* StudioRGB= */ false, /* P709= */ true, /* StudioYUV= */ true);
            }
        }
    }

    void VIDEO_PROCESS_INPUT_ARGUMENTS::PrepareStreamArguments(_In_ UINT stream)
    {
        PrepareTransform(stream);

        // Update Rate Info
        D3D12InputStreamArguments[stream].RateInfo.OutputIndex = StreamInfo[stream].OutputIndex;
        D3D12InputStreamArguments[stream].RateInfo.InputFrameOrField = StreamInfo[stream].InputFrameOrField;
    }

    void VIDEO_PROCESS_INPUT_ARGUMENTS::PrepareTransform(_In_ UINT stream)
    {
        // compute final orientation of the input stream based on our saved OrientationInfo
        D3D12_VIDEO_PROCESS_ORIENTATION orientation = FinalOrientation(StreamInfo[stream].OrientationInfo.Rotation, StreamInfo[stream].OrientationInfo.FlipHorizontal, StreamInfo[stream].OrientationInfo.FlipVertical);
        D3D12InputStreamArguments[stream].Transform.Orientation = orientation;
        D3D12InputStreamDesc[stream].EnableOrientation = (orientation == D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT) ? FALSE : TRUE;

        D3D12_RESOURCE_DESC desc = D3D12InputStreamArguments[stream].InputStream[0].pTexture2D->GetDesc();

        if (!StreamInfo[stream].EnableSourceRect)
        {
            // if source rectangle is not set, entire input surface according to DX11 spec
            D3D12InputStreamArguments[stream].Transform.SourceRectangle = CD3DX12_RECT(0, 0, (LONG)desc.Width, (LONG)desc.Height);

            D3D12_VIDEO_SIZE_RANGE& SourceSizeRange = D3D12InputStreamDesc[stream].SourceSizeRange;
            SourceSizeRange.MaxWidth = static_cast<UINT>(desc.Width);
            SourceSizeRange.MinWidth = static_cast<UINT>(desc.Width);
            SourceSizeRange.MaxHeight = desc.Height;
            SourceSizeRange.MinHeight = desc.Height;
        }

        if (!StreamInfo[stream].EnableDestinationRect)
        {
            // if dest rectangle is not set, no data is written from this stream according to DX11 spec
            D3D12InputStreamArguments[stream].Transform.DestinationRectangle = CD3DX12_RECT(0, 0, 0, 0);

            D3D12_VIDEO_SIZE_RANGE& DestinationSizeRange = D3D12InputStreamDesc[stream].DestinationSizeRange;
            DestinationSizeRange.MaxWidth = static_cast<UINT>(desc.Width);
            DestinationSizeRange.MinWidth = static_cast<UINT>(desc.Width);
            DestinationSizeRange.MaxHeight = desc.Height;;
            DestinationSizeRange.MinHeight = desc.Height;
        }
    }

    D3D12_VIDEO_PROCESS_ORIENTATION VIDEO_PROCESS_INPUT_ARGUMENTS::FinalOrientation(_In_ D3D12_VIDEO_PROCESS_ORIENTATION Rotation, _In_ BOOL FlipHorizontal, _In_ BOOL FlipVertical)
    {
        D3D12_VIDEO_PROCESS_ORIENTATION Operation = D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT;

        // 
        // D3D11 specifies rotation followed by flips
        //
        if (FlipHorizontal &&  FlipVertical)
        {
            Operation = D3D12_VIDEO_PROCESS_ORIENTATION_CLOCKWISE_180;
        }
        else if (FlipVertical)
        {
            Operation = D3D12_VIDEO_PROCESS_ORIENTATION_FLIP_VERTICAL;
        }
        else if (FlipHorizontal)
        {
            Operation = D3D12_VIDEO_PROCESS_ORIENTATION_FLIP_HORIZONTAL;
        }

        static_assert(D3D12_VIDEO_PROCESS_ORIENTATION_CLOCKWISE_270_FLIP_HORIZONTAL == 7);
        constexpr UINT NumOrientations = 8;
        return (D3D12_VIDEO_PROCESS_ORIENTATION)(((UINT)Rotation + (UINT)Operation) % NumOrientations);
    }

    //
    // Transitions the resources referenced by VIDEO_PROCESS_INPUT_ARGUMENTS to D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ.
    //
    void VIDEO_PROCESS_INPUT_ARGUMENTS::TransitionResources(_In_ ImmediateContext *pParent, _In_ UINT stream, _In_ UINT view)
    {
        VIDEO_PROCESS_STREAM_INFO *pStreamInfo = &StreamInfo[stream];

        Resource *pCurFrame = pStreamInfo->ResourceSet[view].CurrentFrame.pResource;
        pParent->GetResourceStateManager().TransitionSubresources(pCurFrame, pStreamInfo->ResourceSet[view].CurrentFrame.SubresourceSubset, D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ, COMMAND_LIST_TYPE::VIDEO_PROCESS);

        // Fill past frames into D3D12 input stream arguments
        UINT NumPastFrames = (UINT)pStreamInfo->ResourceSet[view].PastFrames.size();
        if (NumPastFrames)
        {
            for (DWORD pastFrame = 0; pastFrame < NumPastFrames; pastFrame++)
            {
                Resource *pPastFrame = pStreamInfo->ResourceSet[view].PastFrames[pastFrame].pResource;
                pParent->GetResourceStateManager().TransitionSubresources(pPastFrame, pStreamInfo->ResourceSet[view].PastFrames[pastFrame].SubresourceSubset, D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ, COMMAND_LIST_TYPE::VIDEO_PROCESS);
            }
        }

        UINT NumFutureFrames = (UINT)pStreamInfo->ResourceSet[view].FutureFrames.size();
        if (NumFutureFrames)
        {
            for (DWORD FutureFrame = 0; FutureFrame < NumFutureFrames; FutureFrame++)
            {
                Resource *pFutureFrame = pStreamInfo->ResourceSet[view].FutureFrames[FutureFrame].pResource;
                pParent->GetResourceStateManager().TransitionSubresources(pFutureFrame, pStreamInfo->ResourceSet[view].FutureFrames[FutureFrame].SubresourceSubset, D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ, COMMAND_LIST_TYPE::VIDEO_PROCESS);
            }
        }
    }

    //
    // Reads from the VIDEO_PROCESS_OUTPUT_ARGUMENTS' CurrentFrame data, building D3D12_VIDEO_PROCESS_OUTPUT_ARGUMENTS OutputStream data
    //
    void VIDEO_PROCESS_OUTPUT_ARGUMENTS::PrepareResources()
    {
        DWORD nViews = D3D12OutputStreamDesc.EnableStereo ? 2 : 1;
        for (DWORD view = 0; view < nViews; view++)
        {
            Resource *pCurrentFrame = CurrentFrame[view].pResource;
            D3D12OutputStreamArguments.OutputStream[view].pTexture2D = pCurrentFrame->GetUnderlyingResource();
            D3D12OutputStreamArguments.OutputStream[view].Subresource = CurrentFrame[view].SubresourceSubset.MinSubresource();
        }

        const D3D12_RESOURCE_DESC &desc = D3D12OutputStreamArguments.OutputStream[0].pTexture2D->GetDesc();
        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC& outputDesc = D3D12OutputStreamDesc;

        outputDesc.Format = desc.Format;
        if (!ColorSpaceSet)
        {
            outputDesc.ColorSpace = CDXGIColorSpaceHelper::ConvertFromLegacyColorSpace(!CD3D11FormatHelper::YUV(desc.Format), CD3D11FormatHelper::GetBitsPerUnit(desc.Format), /* StudioRGB= */ false, /* P709= */ true, /* StudioYUV= */ true);
        }

        bool isOutputYCbCr = !CDXGIColorSpaceHelper::IsRGBColorSpace(outputDesc.ColorSpace);
        if (BackgroundColorSet  &&  BackgroundColorYCbCr  != isOutputYCbCr)
        {
            // we will need to convert the background color to the dest colorspace, since they are not matching.
            FLOAT converted[4] = {};

            if (BackgroundColorYCbCr)
            {
                // Note that we do not have any info about the color space of the background color. Assume studio 709.
                ColorConvertNormalized(outputDesc.BackgroundColor, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709, converted, outputDesc.ColorSpace);
            }
            else
            {
                // Note that we do not have any info about the color space of the background color. Assume full 709.
                ColorConvertNormalized(outputDesc.BackgroundColor, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, converted, outputDesc.ColorSpace);
            }
            memcpy(outputDesc.BackgroundColor, converted, sizeof(outputDesc.BackgroundColor));
        }
        // we have now updated our D3D12 outputdesc background color if needed
        BackgroundColorSet = false;

    }

    void VIDEO_PROCESS_OUTPUT_ARGUMENTS::PrepareTransform()
    {
        D3D12_RESOURCE_DESC desc = D3D12OutputStreamArguments.OutputStream[0].pTexture2D->GetDesc();
        if (!EnableTargetRect)
        {
            // if target rectangle is not set, entire output surface according to DX11 spec
            D3D12OutputStreamArguments.TargetRectangle = CD3DX12_RECT(0, 0, (LONG)desc.Width, (LONG)desc.Height);
        }
    }

    //
    // Transitions the resources referenced by VIDEO_PROCESS_OUTPUT_ARGUMENTS to D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE.
    //
    void VIDEO_PROCESS_OUTPUT_ARGUMENTS::TransitionResources(_In_ ImmediateContext *pParent)
    {
        DWORD nViews = D3D12OutputStreamDesc.EnableStereo ? 2 : 1;
        for (DWORD view = 0; view < nViews; view++)
        {
            Resource *pCurrentFrame = CurrentFrame[view].pResource;

            // video process barrier for the output buffer
            pParent->GetResourceStateManager().TransitionSubresources(pCurrentFrame, CurrentFrame[view].SubresourceSubset, D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE, COMMAND_LIST_TYPE::VIDEO_PROCESS);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void DeinterlacePrepass::Process(_Inout_ VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, UINT NumInputStreams, _In_ VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments)
    {
        for (UINT stream = 0; stream < NumInputStreams; ++stream)
        {
            DWORD nViews = pInputArguments->D3D12InputStreamDesc[stream].StereoFormat == D3D12_VIDEO_FRAME_STEREO_FORMAT_SEPARATE ? 2 : 1;
            bool bIsInterlaced = false;
            bool bNeedToDeinterlace = false;
            bool bCanDoInterlace = false;
            if (pInputArguments->D3D12InputStreamArguments[stream].FieldType != D3D12_VIDEO_FIELD_TYPE_NONE)
            {
                // Content is interlaced
                bIsInterlaced = true;

                // Check if the underlying hardware can do the deinterlace.
                // TODO: If there's another translation layer op here, how to determine the right input parameters to pass?
                auto pInputResource = pInputArguments->StreamInfo[stream].ResourceSet[0].CurrentFrame.pResource;
                auto& InputResourceDesc = pInputResource->Parent()->m_desc12;
                D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT VPSupport =
                {
                    m_pParent->GetNodeIndex(),
                    { (UINT)InputResourceDesc.Width, InputResourceDesc.Height, 
                      { pInputArguments->D3D12InputStreamDesc[stream].Format,
                        pInputArguments->D3D12InputStreamDesc[stream].ColorSpace } },
                    pInputArguments->D3D12InputStreamArguments[stream].FieldType,
                    pInputArguments->D3D12InputStreamDesc[stream].StereoFormat,
                    pInputArguments->D3D12InputStreamDesc[stream].FrameRate,
                    { pOutputArguments->D3D12OutputStreamDesc.Format,
                      pOutputArguments->D3D12OutputStreamDesc.ColorSpace },
                    pOutputArguments->D3D12OutputStreamDesc.EnableStereo ?
                        D3D12_VIDEO_FRAME_STEREO_FORMAT_SEPARATE : D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE,
                    pOutputArguments->D3D12OutputStreamDesc.FrameRate,
                };
                CComQIPtr<ID3D12VideoDevice> spVideoDevice = m_pParent->m_pDevice12.get();
                bNeedToDeinterlace =
                    FAILED(spVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_PROCESS_SUPPORT, &VPSupport, sizeof(VPSupport))) ||
                    (VPSupport.DeinterlaceSupport & D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_BOB) == D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE;

                // If we can't do it, we'll still make the stream appear progressive
                bCanDoInterlace = 
                    InputResourceDesc.Format == DXGI_FORMAT_NV12 &&
                    (pInputArguments->D3D12InputStreamDesc[stream].FrameRate.Numerator / pInputArguments->D3D12InputStreamDesc[stream].FrameRate.Denominator ==
                     pOutputArguments->D3D12OutputStreamDesc.FrameRate.Numerator / pOutputArguments->D3D12OutputStreamDesc.FrameRate.Denominator);
            }
            
            for (DWORD view = 0; view < nViews; view++)
            {
                if (bNeedToDeinterlace && bCanDoInterlace)
                {
                    // Create an intermediate matching the input.
                    VideoProcessView& InputView = pInputArguments->StreamInfo[stream].ResourceSet[view].CurrentFrame;
                    Resource* pInputResource = InputView.pResource;
                    ResourceCreationArgs IntermediateArgs = *pInputResource->Parent();
                    CViewSubresourceSubset SrcSubresources = InputView.SubresourceSubset;

                    assert(InputView.SubresourceSubset.NumExtendedSubresources() == 2);
                    IntermediateArgs.m_desc12.DepthOrArraySize = 1;
                    IntermediateArgs.m_desc12.MipLevels = 1;
                    IntermediateArgs.m_appDesc.m_ArraySize = 1;
                    IntermediateArgs.m_appDesc.m_MipLevels = 1;
                    IntermediateArgs.m_appDesc.m_SubresourcesPerPlane = 1;
                    IntermediateArgs.m_appDesc.m_Subresources = 2;

                    if (m_spIntermediates.size() <= stream)
                    {
                        m_spIntermediates.resize(NumInputStreams); // throw( bad_alloc )
                    }

                    // If we already have an existing intermediate, check if it's compatible
                    auto& spIntermediate = m_spIntermediates[stream][view];
                    if (spIntermediate)
                    {
                        ResourceCreationArgs const& ExistingResourceArgs = *spIntermediate->Parent();
                        assert(ExistingResourceArgs.m_desc12.Format == DXGI_FORMAT_NV12);
                        if (ExistingResourceArgs.m_desc12.Width != IntermediateArgs.m_desc12.Width ||
                            ExistingResourceArgs.m_desc12.Height != IntermediateArgs.m_desc12.Height)
                        {
                            spIntermediate.reset();
                        }
                    }

                    if (!spIntermediate)
                    {
                        IntermediateArgs.m_desc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
                        spIntermediate = Resource::CreateResource(m_pParent, IntermediateArgs, ResourceAllocationContext::ImmediateContextThreadLongLived); // throw( bad_alloc, _com_error )
                    }

                    // Replace input stream resource with intermediate
                    InputView.pResource = spIntermediate.get();
                    InputView.SubresourceSubset = CViewSubresourceSubset(CSubresourceSubset(1, 1, 2), 1, 1, 2);

                    // Strip past and future frames
                    pInputArguments->StreamInfo[stream].ResourceSet[view].PastFrames.clear();
                    pInputArguments->StreamInfo[stream].ResourceSet[view].PastSubresources.clear();
                    pInputArguments->StreamInfo[stream].ResourceSet[view].FutureFrames.clear();
                    pInputArguments->StreamInfo[stream].ResourceSet[view].FutureSubresources.clear();
                    pInputArguments->StreamInfo[stream].ResourceSet[view].D3D12ResourcePastFrames.clear();
                    pInputArguments->StreamInfo[stream].ResourceSet[view].D3D12ResourceFutureFrames.clear();

                    pInputArguments->PrepareResources(stream, view);

                    CreatePipelines(DXGI_FORMAT_R8_UINT);
                    CreatePipelines(DXGI_FORMAT_R8G8_UINT);

                    // Issue draws
                    bool bTopFrame = 
                        ((pInputArguments->D3D12InputStreamArguments[stream].FieldType == D3D12_VIDEO_FIELD_TYPE_INTERLACED_TOP_FIELD_FIRST ? 0 : 1) +
                         pInputArguments->D3D12InputStreamArguments[stream].RateInfo.OutputIndex) % 2 == 0;
                    DoDeinterlace(pInputResource, SrcSubresources, spIntermediate.get(), bTopFrame);
                }
                else if (m_spIntermediates.size() > stream && m_spIntermediates[stream][view])
                {
                    m_spIntermediates[stream][view].reset();
                }
            }

            if (bNeedToDeinterlace)
            {
                // Make the stream appear not interlaced anymore
                pInputArguments->D3D12InputStreamArguments[stream].FieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
                pInputArguments->D3D12InputStreamDesc[stream].DeinterlaceMode = D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE;
                pInputArguments->D3D12InputStreamArguments[stream].RateInfo.OutputIndex /= 2;
            }
            else if (bIsInterlaced)
            {
                pInputArguments->D3D12InputStreamDesc[stream].DeinterlaceMode = m_DeinterlaceMode;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void DeinterlacePrepass::CreatePipelines(DXGI_FORMAT RTVFormat)
    {
        if (!m_spRootSig)
        {
            m_spRootSig.reset(new InternalRootSignature(m_pParent)); // throw( bad_alloc )
            m_spRootSig->Create(g_DeinterlacePS, sizeof(g_DeinterlacePS)); // throw( _com_error )
        }

        auto& spPSO = m_spDeinterlacePSOs[RTVFormat];
        if (!spPSO)
        {
            spPSO.reset(new VideoProcessPipelineState(m_pParent));

            struct VPPSOStream
            {
                CD3DX12_PIPELINE_STATE_STREAM_VS VS{CD3DX12_SHADER_BYTECODE(g_DeinterlaceVS, sizeof(g_DeinterlaceVS))};
                CD3DX12_PIPELINE_STATE_STREAM_PS PS{CD3DX12_SHADER_BYTECODE(g_DeinterlacePS, sizeof(g_DeinterlacePS))};
                CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopology{D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE};
                CD3DX12_PIPELINE_STATE_STREAM_NODE_MASK NodeMask;
                CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
                CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DSS;
                CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC Samples{DXGI_SAMPLE_DESC{1, 0}};
                CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK SampleMask{UINT_MAX};
            } PSODesc;
            PSODesc.NodeMask = m_pParent->GetNodeMask();
            PSODesc.RTVFormats = D3D12_RT_FORMAT_ARRAY{ {RTVFormat}, 1 };
            CD3DX12_DEPTH_STENCIL_DESC DSS(CD3DX12_DEFAULT{});
            DSS.DepthEnable = false;
            PSODesc.DSS = DSS;
            D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc = { sizeof(PSODesc), &PSODesc };
            ThrowFailure(m_pParent->m_pDevice12_2->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(spPSO->GetForCreate())));
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void DeinterlacePrepass::DoDeinterlace(Resource* pSrc, CViewSubresourceSubset SrcSubset, Resource* pDst, bool bTopFrame)
    {
        m_pParent->PreRender(COMMAND_LIST_TYPE::GRAPHICS);
        m_pParent->GetResourceStateManager().TransitionSubresources(pSrc, SrcSubset, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, COMMAND_LIST_TYPE::GRAPHICS);
        m_pParent->GetResourceStateManager().TransitionResource(pDst, D3D12_RESOURCE_STATE_RENDER_TARGET, COMMAND_LIST_TYPE::GRAPHICS);
        m_pParent->GetResourceStateManager().ApplyAllResourceTransitions();

        // According to documentation, VPBlt doesn't respect predication
        ImmediateContext::CDisablePredication DisablePredication(m_pParent);

        UINT SRVBaseSlot = m_pParent->ReserveSlots(m_pParent->m_ViewHeap, SrcSubset.NumExtendedSubresources());
        ID3D12GraphicsCommandList* pCommandList = m_pParent->GetGraphicsCommandList();
        pCommandList->SetGraphicsRootSignature(m_spRootSig->GetRootSignature());
        pCommandList->SetGraphicsRoot32BitConstant(0, bTopFrame ? 1 : 0, 0);
        
        pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        {
            // Unbind all VBs
            D3D12_VERTEX_BUFFER_VIEW VBVArray[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
            memset(VBVArray, 0, sizeof(VBVArray));
            pCommandList->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, VBVArray);
        }

        for (auto&& Range : SrcSubset)
        {
            for (UINT subresource = Range.first; subresource < Range.second; ++subresource)
            {
                UINT8 SrcPlane = 0, SrcMip = 0;
                UINT16 SrcArraySlice = 0;
                D3D12DecomposeSubresource(subresource, pSrc->AppDesc()->MipLevels(), pSrc->AppDesc()->ArraySize(), SrcMip, SrcArraySlice, SrcPlane);
                auto& SrcFootprint = pSrc->GetSubresourcePlacement(subresource).Footprint;

                DXGI_FORMAT ViewFormat = SrcFootprint.Format;
                switch (ViewFormat)
                {
                case DXGI_FORMAT_R8_TYPELESS:
                    ViewFormat = DXGI_FORMAT_R8_UINT;
                    break;
                case DXGI_FORMAT_R8G8_TYPELESS:
                    ViewFormat = DXGI_FORMAT_R8G8_UINT;
                    break;
                case DXGI_FORMAT_R16_TYPELESS:
                    ViewFormat = DXGI_FORMAT_R16_UINT;
                    break;
                case DXGI_FORMAT_R16G16_TYPELESS:
                    ViewFormat = DXGI_FORMAT_R16G16_UINT;
                    break;
                }

                D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                SRVDesc.Format = ViewFormat;
                SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                SRVDesc.Texture2DArray.MipLevels = 1;
                SRVDesc.Texture2DArray.MostDetailedMip = SrcMip;
                SRVDesc.Texture2DArray.PlaneSlice = SrcPlane;
                SRVDesc.Texture2DArray.ArraySize = 1;
                SRVDesc.Texture2DArray.FirstArraySlice = SrcArraySlice;
                SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
                SRV inputSRV(m_pParent, SRVDesc, *pSrc);

#if DBG
                UINT DstSubresource = ComposeSubresourceIdxExtended(0, 0, SrcPlane, 1, 1);
                auto& DstFootprint = pDst->GetSubresourcePlacement(DstSubresource).Footprint;
                assert(DstFootprint.Format == SrcFootprint.Format &&
                       DstFootprint.Width == SrcFootprint.Width &&
                       DstFootprint.Height == SrcFootprint.Height &&
                       DstSubresource == SrcPlane);
#endif

                D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
                RTVDesc.Format = ViewFormat;
                RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                RTVDesc.Texture2D.MipSlice = 0;
                RTVDesc.Texture2D.PlaneSlice = SrcPlane;
                RTV outputRTV(m_pParent, RTVDesc, *pDst);

                D3D12_CPU_DESCRIPTOR_HANDLE SRVBaseCPU = m_pParent->m_ViewHeap.CPUHandle(SRVBaseSlot);
                D3D12_GPU_DESCRIPTOR_HANDLE SRVBaseGPU = m_pParent->m_ViewHeap.GPUHandle(SRVBaseSlot);
                SRVBaseSlot++;

                m_pParent->m_pDevice12->CopyDescriptorsSimple(1, SRVBaseCPU, inputSRV.GetRefreshedDescriptorHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

                pCommandList->SetPipelineState(m_spDeinterlacePSOs[ViewFormat]->GetForUse(COMMAND_LIST_TYPE::GRAPHICS));

                CD3DX12_VIEWPORT Viewport(0.f, 0.f, (FLOAT)SrcFootprint.Width, (FLOAT)SrcFootprint.Height);
                CD3DX12_RECT Scissor(0, 0, SrcFootprint.Width, SrcFootprint.Height);
                pCommandList->RSSetViewports(1, &Viewport);
                pCommandList->RSSetScissorRects(1, &Scissor);

                pCommandList->OMSetRenderTargets(1, &outputRTV.GetRefreshedDescriptorHandle(), TRUE, nullptr);
                pCommandList->SetGraphicsRootDescriptorTable(1, SRVBaseGPU);

                pCommandList->DrawInstanced(4, 1, 0, 0);
            }
        }

        m_pParent->PostRender(COMMAND_LIST_TYPE::GRAPHICS, e_GraphicsStateDirty);
    }

};