// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    typedef enum 
    {
        VIDEO_DECODE_PROFILE_TYPE_NONE,
        VIDEO_DECODE_PROFILE_TYPE_VC1,
        VIDEO_DECODE_PROFILE_TYPE_MPEG2,
        VIDEO_DECODE_PROFILE_TYPE_MPEG4PT2,
        VIDEO_DECODE_PROFILE_TYPE_H264,
        VIDEO_DECODE_PROFILE_TYPE_HEVC,
        VIDEO_DECODE_PROFILE_TYPE_VP9,
        VIDEO_DECODE_PROFILE_TYPE_VP8,
        VIDEO_DECODE_PROFILE_TYPE_H264_MVC,
        VIDEO_DECODE_PROFILE_TYPE_MAX_VALID // Keep at the end to inform static asserts
    } VIDEO_DECODE_PROFILE_TYPE;

    typedef struct _DXVA_PicEntry
    {
        union 
        {
            struct
            {
                UCHAR Index7Bits : 7;
                UCHAR AssociatedFlag : 1;
            };
            UCHAR bPicEntry;
        };
    } DXVA_PicEntry; 

    class VideoDecodeStatistics : public DeviceChild
    {
    public:
        VideoDecodeStatistics(ImmediateContext* pDevice);
        ~VideoDecodeStatistics();
        
        void EndQuery(UINT StatusReportFeedbackNumber, const DXVA_PicEntry& CurrPic, UCHAR field_pic_flag = 0) noexcept;
        void ReadAvailableData(ID3D12VideoDecoder* pVideoDecoder, VIDEO_DECODE_PROFILE_TYPE profileType, BYTE* pData, SIZE_T DataSize);

    protected:

        typedef struct _StatisticsInfo
        {
            UINT64 CompletedFenceId = UINT64_MAX;
            UINT StatusReportFeedbackNumber = 0;
            DXVA_PicEntry CurrPic = {};
            UCHAR field_pic_flag = 0;
        } StatisticsInfo;

        static SIZE_T GetResultOffsetForIndex(UINT Index);
        static SIZE_T GetStatStructSize(VIDEO_DECODE_PROFILE_TYPE profileType);

        std::vector<StatisticsInfo> m_StatisticsInfo;
        unique_comptr<ID3D12QueryHeap> m_spQueryHeap;
        D3D12ResourceSuballocation m_ResultBuffer;
        UINT16 m_SubmissionIndex = 0;
        UINT16 m_ResultCount;
    };

}; // namespace D3D12TranslationLayer