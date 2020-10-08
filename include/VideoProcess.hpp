// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once
#include <array>

namespace D3D12TranslationLayer
{
    const UINT MIN_SUPPORTED_INPUT_STREAMS_VIA_EMULATION = 3;

    struct VideoProcessView
    {
        Resource* pResource = nullptr;
        CViewSubresourceSubset SubresourceSubset;
    };

    struct VIDEO_PROCESS_ORIENTATION_INFO {
        D3D12_VIDEO_PROCESS_ORIENTATION Rotation = D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT;
        BOOL FlipHorizontal = FALSE;
        BOOL FlipVertical = FALSE;
    };

    struct VIDEO_PROCESS_STREAM_INFO {
        BOOL StereoFormatSwapViews = FALSE;
        VIDEO_PROCESS_ORIENTATION_INFO OrientationInfo = {};
        BOOL EnableSourceRect = FALSE;
        BOOL EnableDestinationRect = FALSE;
        BOOL ColorSpaceSet = FALSE;
        UINT OutputIndex = 0;
        UINT InputFrameOrField = 0;

        struct {
            VideoProcessView CurrentFrame;
            std::vector<VideoProcessView> PastFrames;
            std::vector<VideoProcessView> FutureFrames;

            std::vector<UINT> PastSubresources;
            std::vector<UINT> FutureSubresources;
            std::vector<ID3D12Resource *> D3D12ResourcePastFrames;
            std::vector<ID3D12Resource *> D3D12ResourceFutureFrames;
        } ResourceSet[D3D12_VIDEO_PROCESS_STEREO_VIEWS];
    };

    struct VIDEO_PROCESS_INPUT_ARGUMENTS
    {
        void ResetStreams(UINT NumStreams)
        {
            StreamInfo.resize(NumStreams);
            D3D12InputStreamArguments.resize(NumStreams);
            D3D12InputStreamDesc.resize(NumStreams);
            for (DWORD i = 0; i < NumStreams; i++)
            {
                // input stream arguments
                ZeroMemory(&D3D12InputStreamArguments[i].InputStream, sizeof(D3D12InputStreamArguments[i].InputStream));
                ZeroMemory(&D3D12InputStreamArguments[i].Transform, sizeof(D3D12InputStreamArguments[i].Transform));
                D3D12InputStreamArguments[i].Flags = D3D12_VIDEO_PROCESS_INPUT_STREAM_FLAG_NONE;
                ZeroMemory(&D3D12InputStreamArguments[i].RateInfo, sizeof(D3D12InputStreamArguments[i].RateInfo));
                ZeroMemory(&D3D12InputStreamArguments[i].FilterLevels, sizeof(D3D12InputStreamArguments[i].FilterLevels));
                ZeroMemory(&D3D12InputStreamArguments[i].AlphaBlending, sizeof(D3D12InputStreamArguments[i].AlphaBlending));

                // input stream descriptor
                D3D12InputStreamDesc[i].Format = DXGI_FORMAT_UNKNOWN;
                D3D12InputStreamDesc[i].ColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
                D3D12InputStreamDesc[i].SourceAspectRatio.Numerator = 1;
                D3D12InputStreamDesc[i].SourceAspectRatio.Denominator = 1;
                D3D12InputStreamDesc[i].DestinationAspectRatio.Numerator = 1;
                D3D12InputStreamDesc[i].DestinationAspectRatio.Denominator = 1;
                D3D12InputStreamDesc[i].FrameRate.Numerator = 30;
                D3D12InputStreamDesc[i].FrameRate.Denominator = 1;
                ZeroMemory(&D3D12InputStreamDesc[i].SourceSizeRange, sizeof(D3D12InputStreamDesc[i].SourceSizeRange));
                ZeroMemory(&D3D12InputStreamDesc[i].DestinationSizeRange, sizeof(D3D12InputStreamDesc[i].DestinationSizeRange));
                D3D12InputStreamDesc[i].EnableOrientation = FALSE;
                D3D12InputStreamDesc[i].FilterFlags = D3D12_VIDEO_PROCESS_FILTER_FLAG_NONE;
                D3D12InputStreamDesc[i].StereoFormat = D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE;
                D3D12InputStreamDesc[i].FieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
                D3D12InputStreamDesc[i].DeinterlaceMode = D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE;
                D3D12InputStreamDesc[i].EnableAlphaBlending = FALSE;
                ZeroMemory(&D3D12InputStreamDesc[i].LumaKey, sizeof(D3D12InputStreamDesc[i].LumaKey));
                D3D12InputStreamDesc[i].EnableAutoProcessing = FALSE;
                D3D12InputStreamDesc[i].NumPastFrames = 0;
                D3D12InputStreamDesc[i].NumFutureFrames = 0;

                // stream info
                for (DWORD view = 0; view < D3D12_VIDEO_PROCESS_STEREO_VIEWS; view++)
                {
                    StreamInfo[i].ResourceSet[view].PastFrames.clear();
                    StreamInfo[i].ResourceSet[view].FutureFrames.clear();
                }
            }
        }

        void PrepareResources(_In_ UINT stream, _In_ UINT view);
        void PrepareStreamArguments(_In_ UINT stream);
        void PrepareTransform(_In_ UINT stream);
        D3D12_VIDEO_PROCESS_ORIENTATION FinalOrientation(_In_ D3D12_VIDEO_PROCESS_ORIENTATION Rotation, _In_ BOOL FlipHorizontal, _In_ BOOL FlipVertical);
        void TransitionResources(_In_ ImmediateContext *pParent, _In_ UINT stream, _In_ UINT view);

        std::vector<VIDEO_PROCESS_STREAM_INFO> StreamInfo;
        std::vector<D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS1> D3D12InputStreamArguments;
        std::vector<D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC> D3D12InputStreamDesc;
    };

    struct VIDEO_PROCESS_OUTPUT_ARGUMENTS
    {
        VIDEO_PROCESS_OUTPUT_ARGUMENTS()
        {
            // output stream arguments
            ZeroMemory(D3D12OutputStreamArguments.OutputStream, sizeof(D3D12OutputStreamArguments.OutputStream));
            ZeroMemory(&D3D12OutputStreamArguments.TargetRectangle, sizeof(D3D12OutputStreamArguments.TargetRectangle));

            // output stream desc
            D3D12OutputStreamDesc.Format = DXGI_FORMAT_UNKNOWN;
            D3D12OutputStreamDesc.ColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            D3D12OutputStreamDesc.AlphaFillMode = D3D12_VIDEO_PROCESS_ALPHA_FILL_MODE_OPAQUE;
            D3D12OutputStreamDesc.AlphaFillModeSourceStreamIndex = 0;
            ZeroMemory(D3D12OutputStreamDesc.BackgroundColor, sizeof(D3D12OutputStreamDesc.BackgroundColor));
            ZeroMemory(&D3D12OutputStreamDesc.FrameRate, sizeof(D3D12OutputStreamDesc.FrameRate));
            D3D12OutputStreamDesc.EnableStereo = FALSE;
            D3D12OutputStreamDesc.FrameRate.Numerator = 30;
            D3D12OutputStreamDesc.FrameRate.Denominator = 1;
        }

        void PrepareResources();
        void PrepareTransform();
        void TransitionResources(_In_ ImmediateContext *pParent);

        // helpers
        VideoProcessView CurrentFrame[D3D12_VIDEO_PROCESS_STEREO_VIEWS];
        bool EnableTargetRect = false;
        bool ColorSpaceSet = false;
        bool BackgroundColorYCbCr = false;
        bool BackgroundColorSet = false;
        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS D3D12OutputStreamArguments;
        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC D3D12OutputStreamDesc;
    };

    class DeinterlacePrepass
    {
    public:
        DeinterlacePrepass(ImmediateContext* pDevice, class VideoProcess* pVP, D3D12_VIDEO_PROCESS_DEINTERLACE_FLAGS DeinterlaceMode)
            : m_pParent(pDevice)
            , m_pVP(pVP)
            , m_DeinterlaceMode(DeinterlaceMode)
        {
        }
        void Process(_Inout_ VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, UINT NumInputStreams, _In_ VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments);

    private:
        void CreatePipelines(DXGI_FORMAT RTVFormat);
        void DoDeinterlace(Resource* pSrc, CViewSubresourceSubset SrcSubset, Resource* pDst, bool bTopFrame);

        using VideoProcessPipelineState = DeviceChildImpl<ID3D12PipelineState>;

        ImmediateContext* const m_pParent;
        VideoProcess* const m_pVP;
        D3D12_VIDEO_PROCESS_DEINTERLACE_FLAGS m_DeinterlaceMode;
        std::vector<std::array<unique_comptr<Resource>, 2>> m_spIntermediates;
        std::map<DXGI_FORMAT, std::unique_ptr<VideoProcessPipelineState>> m_spDeinterlacePSOs;
        std::unique_ptr<InternalRootSignature> m_spRootSig;
    };

    class VideoProcessor : public DeviceChildImpl<ID3D12VideoProcessor>
    {
    public:
        VideoProcessor(ImmediateContext *pContext, ID3D12VideoDevice* pVideoDeviceNoRef, const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC* pOutputStreamDesc, UINT NumInputStreamDescs, const D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC *pInputStreamDescs);
    };

    class VideoProcess : public DeviceChild
    {
    public:
        VideoProcess(_In_ ImmediateContext *pDevice, D3D12_VIDEO_PROCESS_DEINTERLACE_FLAGS DeinterlaceMode)
            : DeviceChild(pDevice)
            , m_Deinterlace(pDevice, this, DeinterlaceMode)
        {
            Initialize();
        }
        virtual ~VideoProcess() noexcept;

        void ProcessFrames(_Inout_updates_(NumInputStreams) VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, _In_ UINT NumInputStreams, _In_ VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments);

    private:
        void Initialize();

    protected:
        unique_comptr<ID3D12VideoDevice>         m_spVideoDevice;
        std::unique_ptr<VideoProcessor>          m_spVideoProcessor;
        std::vector<D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC> m_creationInputDesc;
        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC   m_creationOutputDesc;
        UINT                                     m_driverSupportedMaxInputStreams = 0;

        DeinterlacePrepass m_Deinterlace;

        void InitializeProcessor(_Inout_updates_(NumInputStreams) VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, UINT NumInputStreams, _Inout_ VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments);
        void UpdateNeededMaxPastFutureFrames(_Inout_ D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC &inputDesc, _In_ const D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC &outputDesc);
        void UpdateInputDescriptor(_Inout_updates_(NumInputStreams) VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, UINT NumInputStreams, _In_ VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments, _Out_ bool &updated);
        void UpdateOutputDescriptor(_Inout_ VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments, _Out_ bool &updated);
        void EmulateVPBlit(_Inout_updates_(NumInputStreams) VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, _In_ UINT NumInputStreams, _In_ VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments, _In_ UINT StartStream);
    };

    class BatchedVideoProcess
    {
    public:
        virtual ~BatchedVideoProcess() {}

        virtual void ProcessFrames(_Inout_updates_(NumInputStreams) VIDEO_PROCESS_INPUT_ARGUMENTS* pInputArguments, _In_ UINT NumInputStreams, _In_ VIDEO_PROCESS_OUTPUT_ARGUMENTS* pOutputArguments) = 0;

    };

    class BatchedVideoProcessImpl : public BatchedDeviceChildImpl<VideoProcess>, public BatchedVideoProcess
    {
    public:
        BatchedVideoProcessImpl(BatchedContext& Context, D3D12_VIDEO_PROCESS_DEINTERLACE_FLAGS DeinterlaceMode)
            : BatchedDeviceChildImpl(Context, DeinterlaceMode)
        {
        }

        void ProcessFrames(_Inout_updates_(NumInputStreams) VIDEO_PROCESS_INPUT_ARGUMENTS *pInputArguments, _In_ UINT NumInputStreams, _In_ VIDEO_PROCESS_OUTPUT_ARGUMENTS *pOutputArguments) override
        {
           FlushBatchAndGetImmediate().ProcessFrames(pInputArguments, NumInputStreams, pOutputArguments);
        }
    };
};
