// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{

class BatchedResource : public BatchedDeviceChild
{
public:
    BatchedResource(BatchedContext& Context, Resource* underlyingResource, bool ownsResource)
        : BatchedDeviceChild(Context)
        , m_spResource(ownsResource ? underlyingResource : nullptr, { Context })
        , m_pResource(underlyingResource)
        // Resources constructed without ownership don't support Map(NO_OVERWRITE) until they've been discarded once.
        , m_LastRenamedResource(ownsResource ? m_pResource->GetIdentity()->m_suballocation : decltype(m_LastRenamedResource){})
    {
        // For simplicity we'll let the immediate resource be constructed before the batched, but it should've been constructed properly.
        assert(m_pResource->m_isValid);
    }

    struct BatchedDeleter
    {
        BatchedContext& Context;
        void operator()(Resource* p) { Context.ReleaseResource(p); }
    };
    std::unique_ptr<Resource, BatchedDeleter> const m_spResource;
    Resource* const m_pResource;
    // Used to implement Map(NO_OVERWRITE)
    D3D12ResourceSuballocation m_LastRenamedResource;

    // Currently, the batched context only supports D3D11 dynamic resources, which are single-subresource and write-only.
    Resource::DynamicTexturePlaneData m_DynamicTexturePlaneData;
    SafeRenameResourceCookie m_PendingRenameViaCopyCookie;
};

}