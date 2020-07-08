// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    class BatchedQuery : public BatchedDeviceChild
    {
        unique_batched_ptr<Async> m_spImmediateAsync;
        Async* m_pImmediateAsyncWeak;

    public:
        BatchedQuery(BatchedContext& Context, Async* pAsync, bool ownsAsync)
            : BatchedDeviceChild(Context)
            , m_spImmediateAsync(ownsAsync ? pAsync : nullptr, { Context })
            , m_pImmediateAsyncWeak(pAsync)
        {
        }
        Async* GetImmediateNoFlush() { return m_pImmediateAsyncWeak; }
        Async* FlushBatchAndGetImmediate() { ProcessBatch(); return m_pImmediateAsyncWeak; }

        Async::AsyncState m_CurrentState = Async::AsyncState::Ended;
        uint64_t m_BatchReferenceID = 0;
    };
}