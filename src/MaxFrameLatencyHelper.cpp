#include "pch.h"

namespace D3D12TranslationLayer
{
	void MaxFrameLatencyHelper::Init(ImmediateContext* pImmCtx)
	{
		assert(pImmCtx != nullptr);
		m_pImmediateContext = pImmCtx;
	}

	void MaxFrameLatencyHelper::SetMaximumFrameLatency(UINT MaxFrameLatency)
	{
		assert(m_pImmediateContext != nullptr);
		std::lock_guard Lock(m_FrameLatencyLock);
		m_MaximumFrameLatency = MaxFrameLatency;
	}

	UINT MaxFrameLatencyHelper::GetMaximumFrameLatency()
	{
		std::lock_guard Lock(m_FrameLatencyLock);
		return m_MaximumFrameLatency;
	}

	bool MaxFrameLatencyHelper::IsMaximumFrameLatencyReached()
	{
		assert(m_pImmediateContext != nullptr);
		std::lock_guard Lock(m_FrameLatencyLock);
		UINT64 CompletedFenceValue = m_pImmediateContext->GetCompletedFenceValue(COMMAND_LIST_TYPE::GRAPHICS);
		while (m_PresentFenceValuesBegin != m_PresentFenceValuesEnd &&
			*m_PresentFenceValuesBegin <= CompletedFenceValue)
		{
			++m_PresentFenceValuesBegin;
		}
		return std::distance(m_PresentFenceValuesBegin, m_PresentFenceValuesEnd) >= (ptrdiff_t)m_MaximumFrameLatency;
	}

	void MaxFrameLatencyHelper::WaitForMaximumFrameLatency()
	{
		assert(m_pImmediateContext != nullptr);
		std::lock_guard Lock(m_FrameLatencyLock);
		// Looping, because max frame latency can be dropped, and we may
		// need to wait for multiple presents to complete here.
		while (IsMaximumFrameLatencyReached())
		{
			m_pImmediateContext->WaitForFenceValue(COMMAND_LIST_TYPE::GRAPHICS, *m_PresentFenceValuesBegin);
		}
	}
	
	void MaxFrameLatencyHelper::RecordPresentFenceValue(UINT64 fenceValue)
	{
		std::lock_guard Lock(m_FrameLatencyLock);
		*m_PresentFenceValuesEnd = fenceValue;
		++m_PresentFenceValuesEnd;
	}
}