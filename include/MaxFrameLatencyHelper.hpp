#pragma once

namespace D3D12TranslationLayer
{
	class MaxFrameLatencyHelper
	{
    public:
        void Init(ImmediateContext* pImmCtx);
        void SetMaximumFrameLatency(UINT MaxFrameLatency);
        UINT GetMaximumFrameLatency();
        bool IsMaximumFrameLatencyReached();
        void WaitForMaximumFrameLatency();
        void RecordPresentFenceValue(UINT64 fenceValue);

	private:
        // Maximum frame latency can be modified or polled from application threads,
        // while presents are enqueued from a driver worker thread.
        // We only need to be as good as kernel, so the app thread only needs to reflect
        // work done by the driver thread, not work that's queued against it.
        std::recursive_mutex m_FrameLatencyLock;
        UINT m_MaximumFrameLatency = 3;
        CircularArray<UINT64, 17> m_PresentFenceValues = {};
        // The fence value to wait on
        decltype(m_PresentFenceValues)::iterator m_PresentFenceValuesBegin = m_PresentFenceValues.begin();
        // The fence value to write to
        decltype(m_PresentFenceValues)::iterator m_PresentFenceValuesEnd = m_PresentFenceValuesBegin;
        ImmediateContext* m_pImmediateContext;
	};
}