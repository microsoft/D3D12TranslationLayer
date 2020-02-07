// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include <dxva.h>
namespace D3D12TranslationLayer
{
    const UINT16 DXVA_MIN_STATUS_REPORTS = 512;           // The DXVA specs require a minimum of 512 entries in the driver for status reports

    const UCHAR VideoDecodeStatusMap[] = 
    {
        0,          // D3D12_VIDEO_DECODE_STATUS_OK 
        1,          // D3D12_VIDEO_DECODE_STATUS_CONTINUE
        2,          // D3D12_VIDEO_DECODE_STATUS_CONTINUE_SKIP_DISPLAY
        3,          // D3D12_VIDEO_DECODE_STATUS_RESTART
        3,          // D3D12_VIDEO_DECODE_STATUS_RESTART
    };

    //----------------------------------------------------------------------------------------------------------------------------------
    VideoDecodeStatistics::VideoDecodeStatistics(ImmediateContext* pDevice)
        : DeviceChild(pDevice)
        , m_ResultCount(DXVA_MIN_STATUS_REPORTS)
    {
        D3D12_QUERY_HEAP_DESC QueryHeapDesc = { D3D12_QUERY_HEAP_TYPE_VIDEO_DECODE_STATISTICS, 1, m_pParent->GetNodeMask() };
        ThrowFailure(m_pParent->m_pDevice12->CreateQueryHeap(
                &QueryHeapDesc,
                IID_PPV_ARGS(&m_spQueryHeap)
                )); // throw( _com_error )

        // Query data goes into a readback heap for CPU readback in GetData
        SIZE_T BufferSize = GetResultOffsetForIndex(m_ResultCount);
        m_ResultBuffer = m_pParent->AcquireSuballocatedHeap(
            AllocatorHeapType::Readback, 
            BufferSize,
            ResourceAllocationContext::FreeThread); // throw( _com_error )
    
        m_StatisticsInfo.resize(m_ResultCount); // throw(bad_alloc )

        void* pMappedData;
        CD3DX12_RANGE ReadRange(0, 0);
        ThrowFailure(m_ResultBuffer.Map(0, &ReadRange, &pMappedData));

        ZeroMemory(pMappedData, BufferSize);

        CD3DX12_RANGE WrittenRange(0, BufferSize);
        m_ResultBuffer.Unmap(0, &WrittenRange);
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    VideoDecodeStatistics::~VideoDecodeStatistics()
    {
        AddToDeferredDeletionQueue(m_spQueryHeap);

        if (m_ResultBuffer.IsInitialized())
        {
            m_pParent->ReleaseSuballocatedHeap(AllocatorHeapType::Readback, m_ResultBuffer, m_LastUsedCommandListID[(UINT)COMMAND_LIST_TYPE::VIDEO_DECODE], COMMAND_LIST_TYPE::VIDEO_DECODE);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void VideoDecodeStatistics::EndQuery(UINT StatusReportFeedbackNumber, const DXVA_PicEntry& CurrPic, UCHAR field_pic_flag) noexcept
    {
        auto pCommandList = m_pParent->GetVideoDecodeCommandList();

        pCommandList->EndQuery(
            m_spQueryHeap.get(),
            D3D12_QUERY_TYPE_VIDEO_DECODE_STATISTICS,
            0);

        SIZE_T offset = GetResultOffsetForIndex(m_SubmissionIndex);
        
        pCommandList->ResolveQueryData(
                m_spQueryHeap.get(),
                D3D12_QUERY_TYPE_VIDEO_DECODE_STATISTICS,
                0,
                1,
                m_ResultBuffer.GetResource(),
                offset + m_ResultBuffer.GetOffset()
                );

        StatisticsInfo& statisticsInfo = m_StatisticsInfo[m_SubmissionIndex];
        statisticsInfo.CompletedFenceId = m_pParent->GetCommandListID(COMMAND_LIST_TYPE::VIDEO_DECODE);
        statisticsInfo.StatusReportFeedbackNumber = StatusReportFeedbackNumber;
        statisticsInfo.CurrPic = CurrPic;
        statisticsInfo.field_pic_flag = field_pic_flag;
        
        UsedInCommandList(COMMAND_LIST_TYPE::VIDEO_DECODE, statisticsInfo.CompletedFenceId);

        m_SubmissionIndex++;
        if (m_SubmissionIndex == m_ResultCount)
        {
            m_SubmissionIndex = 0;
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    SIZE_T VideoDecodeStatistics::GetStatStructSize(VIDEO_DECODE_PROFILE_TYPE profileType)
    {
        switch (profileType)
        {
            case VIDEO_DECODE_PROFILE_TYPE_H264:
            case VIDEO_DECODE_PROFILE_TYPE_H264_MVC:
                return sizeof(DXVA_Status_H264);

            case VIDEO_DECODE_PROFILE_TYPE_VP9:
            case VIDEO_DECODE_PROFILE_TYPE_VP8:
                return sizeof(DXVA_Status_VPx);

            case VIDEO_DECODE_PROFILE_TYPE_HEVC:
                return sizeof(DXVA_Status_HEVC);

            case VIDEO_DECODE_PROFILE_TYPE_VC1:
                return sizeof(DXVA_Status_VC1);

            case VIDEO_DECODE_PROFILE_TYPE_MPEG4PT2:
                return sizeof(DXVA_Status_VC1);         // TODO: Srinath to confirm if MPEG4PT2 spec is really right mentioning to use this one

            case VIDEO_DECODE_PROFILE_TYPE_MPEG2:       // TODO: no info on what to use for this one.
                return 0;

            default:
            {
                ThrowFailure(E_UNEXPECTED);
            }
        }

        // Unreachable
        return 0;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    void VideoDecodeStatistics::ReadAvailableData(ID3D12VideoDecoder* pVideoDecoder, VIDEO_DECODE_PROFILE_TYPE profileType, BYTE* pData, SIZE_T DataSize)
    {
        // From the H.264 DXVA spec. HEVC and VPx specs have similar language.
        //
        // When the accelerator receives the Execute call for status reporting, it should not stall operation to wait 
        // for any prior operations to complete. Instead, it should immediately provide the available status information 
        // for all operations that have completed since the previous request for a status report, up to the maximum amount 
        // requested. Immediately after the Execute call returns, the host decoder can read the status report information 
        // from the buffer.
        //
        // The spec does not specify what to do when caller asks for status and there are no status reports available, or
        // when there are less status reports available than are requested.  A modified output size is not returned to apps by
        // runtimes and there doesn't appear to be an expected error code.  Documentation for StatusRepportFeedbackNumber says 
        // that "The value should not equal 0, and should be different in each call to Execute", so we zero the status reports
        // here including their StatusReportFeedbackNumber to indicate they do not have valid data.  This is consistent with some
        // drivers.
        //
        ZeroMemory(pData, DataSize);

        // Map the resolved query buffer.
        void* pMappedData = nullptr;
        CD3DX12_RANGE ReadRange(0, GetResultOffsetForIndex(m_ResultCount));
        ThrowFailure(m_ResultBuffer.Map(0, &ReadRange, &pMappedData));

        auto Unmap = MakeScopeExit([&]() { m_ResultBuffer.Unmap(0, &CD3DX12_RANGE(0,0)); });

        // Determine the fence ID of the last completed 
        UINT64 lastCompletedFenceID = m_pParent->GetCompletedFenceValue(COMMAND_LIST_TYPE::VIDEO_DECODE);

        SIZE_T codecStructSize = GetStatStructSize(profileType);
        UINT16 statisticsInfoIndex = (m_SubmissionIndex == 0 ? m_ResultCount : m_SubmissionIndex) - 1;

        for(UINT16 i(0); i < m_ResultCount && DataSize >= codecStructSize; ++i, --statisticsInfoIndex)
        {
            StatisticsInfo& statisticsInfo = m_StatisticsInfo[statisticsInfoIndex];
            const D3D12_QUERY_DATA_VIDEO_DECODE_STATISTICS* pD3d12VideoStats = static_cast<const D3D12_QUERY_DATA_VIDEO_DECODE_STATISTICS*>(pMappedData) + statisticsInfoIndex;

            if (statisticsInfo.CompletedFenceId <= lastCompletedFenceID)
            {
                switch (profileType)
                {
                    case VIDEO_DECODE_PROFILE_TYPE_H264:
                    case VIDEO_DECODE_PROFILE_TYPE_H264_MVC:
                    {
                        assert(codecStructSize == sizeof(DXVA_Status_H264));
                        DXVA_Status_H264 *pStatus = reinterpret_cast<DXVA_Status_H264 *>(pData);

                        pStatus->StatusReportFeedbackNumber = statisticsInfo.StatusReportFeedbackNumber;
                        pStatus->CurrPic.Index7Bits = statisticsInfo.CurrPic.Index7Bits;
                        pStatus->CurrPic.AssociatedFlag = statisticsInfo.CurrPic.AssociatedFlag;
                        pStatus->CurrPic.bPicEntry = statisticsInfo.CurrPic.bPicEntry;
                        pStatus->field_pic_flag = statisticsInfo.field_pic_flag;
                        pStatus->bStatus = VideoDecodeStatusMap[pD3d12VideoStats->Status];
                        pStatus->wNumMbsAffected = (USHORT)pD3d12VideoStats->NumMacroblocksAffected;

                        if (g_hTracelogging)
                        {
                            TraceLoggingWrite(g_hTracelogging,
                                "GetStatus - StatusReportFeedbackNumber",
                                TraceLoggingPointer(pVideoDecoder, "pID3D12Decoder"),
                                TraceLoggingValue(pStatus->StatusReportFeedbackNumber, "statusReportFeedbackNumber"));
                        }
                    } break;

                    case VIDEO_DECODE_PROFILE_TYPE_HEVC:
                    {
                        assert(codecStructSize == sizeof(DXVA_Status_HEVC));
                        DXVA_Status_HEVC *pStatus = reinterpret_cast<DXVA_Status_HEVC *>(pData);

                        pStatus->StatusReportFeedbackNumber = static_cast<UINT16>(statisticsInfo.StatusReportFeedbackNumber);
                        pStatus->CurrPic.Index7Bits = statisticsInfo.CurrPic.Index7Bits;
                        pStatus->CurrPic.AssociatedFlag = statisticsInfo.CurrPic.AssociatedFlag;
                        pStatus->CurrPic.bPicEntry = statisticsInfo.CurrPic.bPicEntry;
                        pStatus->bStatus = VideoDecodeStatusMap[pD3d12VideoStats->Status];
                        pStatus->wNumMbsAffected = (USHORT)pD3d12VideoStats->NumMacroblocksAffected;

                        if (g_hTracelogging)
                        {
                            TraceLoggingWrite(g_hTracelogging,
                                "GetStatus - StatusReportFeedbackNumber",
                                TraceLoggingPointer(pVideoDecoder, "pID3D12Decoder"),
                                TraceLoggingValue(pStatus->StatusReportFeedbackNumber, "statusReportFeedbackNumber"));
                        }
                    } break;

                    case VIDEO_DECODE_PROFILE_TYPE_VP9:
                    case VIDEO_DECODE_PROFILE_TYPE_VP8:
                    {
                        assert(codecStructSize == sizeof(DXVA_Status_VPx));
                        DXVA_Status_VPx *pStatus = reinterpret_cast<DXVA_Status_VPx *>(pData);

                        pStatus->StatusReportFeedbackNumber = statisticsInfo.StatusReportFeedbackNumber;
                        pStatus->CurrPic.Index7Bits = statisticsInfo.CurrPic.Index7Bits;
                        pStatus->CurrPic.AssociatedFlag = statisticsInfo.CurrPic.AssociatedFlag;
                        pStatus->CurrPic.bPicEntry = statisticsInfo.CurrPic.bPicEntry;
                        pStatus->bStatus = VideoDecodeStatusMap[pD3d12VideoStats->Status];
                        pStatus->wNumMbsAffected = (USHORT)pD3d12VideoStats->NumMacroblocksAffected;

                        if (g_hTracelogging)
                        {
                            TraceLoggingWrite(g_hTracelogging,
                                "GetStatus - StatusReportFeedbackNumber",
                                TraceLoggingPointer(pVideoDecoder, "pID3D12Decoder"),
                                TraceLoggingValue(pStatus->StatusReportFeedbackNumber, "statusReportFeedbackNumber"));
                        }
                    } break;

                    case VIDEO_DECODE_PROFILE_TYPE_VC1:
                    case VIDEO_DECODE_PROFILE_TYPE_MPEG4PT2:
                    {
                        assert(codecStructSize == sizeof(DXVA_Status_VC1));
                        DXVA_Status_VC1 *pStatus = reinterpret_cast<DXVA_Status_VC1 *>(pData);

                        pStatus->StatusReportFeedbackNumber = static_cast<UINT16>(statisticsInfo.StatusReportFeedbackNumber);
                        pStatus->wDecodedPictureIndex = statisticsInfo.CurrPic.Index7Bits;
                        pStatus->bStatus = VideoDecodeStatusMap[pD3d12VideoStats->Status];
                        pStatus->wNumMbsAffected = (USHORT)pD3d12VideoStats->NumMacroblocksAffected;

                        if (g_hTracelogging)
                        {
                            TraceLoggingWrite(g_hTracelogging,
                                "GetStatus - StatusReportFeedbackNumber",
                                TraceLoggingPointer(pVideoDecoder, "pID3D12Decoder"),
                                TraceLoggingValue(pStatus->StatusReportFeedbackNumber, "statusReportFeedbackNumber"));
                        }
                    } break;

                    case VIDEO_DECODE_PROFILE_TYPE_MPEG2:           // TODO: can't find info about this one, Srinath is checking.
                    {
                        assert(codecStructSize == sizeof(DXVA_Status_H264));
                        break;
                    }

                    default:
                    {
                        ThrowFailure(E_INVALIDARG);
                        break;
                    }
                }

                statisticsInfo.CompletedFenceId = UINT64_MAX;
                
                DataSize -= codecStructSize;
                pData += codecStructSize;
            }
            
            if (statisticsInfoIndex == 0)
            {
                statisticsInfoIndex = m_ResultCount;
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    SIZE_T VideoDecodeStatistics::GetResultOffsetForIndex(UINT Index)
    {
        return sizeof(D3D12_QUERY_DATA_VIDEO_DECODE_STATISTICS) * Index;
    }

}; // namespace D3D12TranslationLayer