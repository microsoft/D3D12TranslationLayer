// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    struct ResourceCacheEntry
    {
        unique_comptr<Resource> m_Resource;
        std::unique_ptr<RTV> m_RTV;
        std::unique_ptr<SRV> m_SRV;
    };

    class ResourceCache
    {
    public:
        ResourceCache(ImmediateContext &device);

        ResourceCacheEntry const& GetResource(DXGI_FORMAT format, UINT width, UINT height, DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN);
        void TakeCacheEntryOwnership(DXGI_FORMAT format, ResourceCacheEntry& entryOut);

        typedef std::map<DXGI_FORMAT, ResourceCacheEntry> DXGIMap;
    private:
        DXGIMap m_Cache;
        ImmediateContext &m_device;
    };
}