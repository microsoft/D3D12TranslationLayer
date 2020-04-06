// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{
    ResourceCache::ResourceCache(ImmediateContext &device) :
        m_device(device)
    {
    };

    ResourceCacheEntry const& ResourceCache::GetResource(DXGI_FORMAT format, UINT width, UINT height, DXGI_FORMAT viewFormat)
    {
        ResourceCacheEntry& CacheEntry = m_Cache[format];

        if (CacheEntry.m_Resource)
        {
            // If the resource is bigger than the clear resource we have cached, reallocate a bigger one. 
            // Note that bigger is better in this case because we can also use large resources for copying clear
            // color into smaller resources as well
            auto pAppDesc = CacheEntry.m_Resource->AppDesc();
            if (pAppDesc->Width() < width || pAppDesc->Height() < height)
            {
                width = max(width, pAppDesc->Width());
                height = max(height, pAppDesc->Height());
                CacheEntry = ResourceCacheEntry{};
            }
        }

        // The D3D12 runtime also defaults DXGI_FORMAT_UNKNOWN to the resource format of the texture when creating a view.
        viewFormat = viewFormat == DXGI_FORMAT_UNKNOWN ? format : viewFormat;

        if (!CacheEntry.m_Resource)
        {
            ResourceCreationArgs createArg = {};
            createArg.m_appDesc = AppResourceDesc(1, // SubresourcesPerPlane
                                                  (UINT8)CD3D11FormatHelper::NonOpaquePlaneCount(format), //PlaneCount
                                                  CD3D11FormatHelper::NonOpaquePlaneCount(format), //SubresourceCount
                                                  1, // Mips
                                                  1, // ArraySize
                                                  1, // Depth
                                                  width,
                                                  height,
                                                  format,
                                                  1, 0, // SampleDesc
                                                  RESOURCE_USAGE_DEFAULT,
                                                  (RESOURCE_CPU_ACCESS)0, // CPUAccess
                                                  (RESOURCE_BIND_FLAGS)(RESOURCE_BIND_RENDER_TARGET | RESOURCE_BIND_SHADER_RESOURCE),
                                                  D3D12_RESOURCE_DIMENSION_TEXTURE2D);
            createArg.m_desc12 = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            createArg.m_heapDesc = CD3DX12_HEAP_DESC(0, D3D12_HEAP_TYPE_DEFAULT);
            createArg.m_flags11.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            createArg.m_bManageResidency = true;

            // TODO: Pass down the clear color to D3D12's create resource
            CacheEntry.m_Resource = Resource::CreateResource(&m_device, createArg, ResourceAllocationContext::ImmediateContextThreadLongLived);
        }

        if (   !CacheEntry.m_RTV
            || !(CacheEntry.m_RTV->GetDesc12().Format == viewFormat))
        {
            D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
            RTVDesc.Format = viewFormat;
            RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            RTVDesc.Texture2D.MipSlice = 0;
            RTVDesc.Texture2D.PlaneSlice = 0;
            CacheEntry.m_RTV.reset(new RTV(&m_device, RTVDesc, *CacheEntry.m_Resource));
        }

        if (   !CacheEntry.m_SRV
            || !(CacheEntry.m_SRV->GetDesc12().Format == viewFormat))
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = viewFormat;
            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            SRVDesc.Texture2D.MipLevels = 1;
            SRVDesc.Texture2D.MostDetailedMip = 0;
            SRVDesc.Texture2D.PlaneSlice = 0;
            SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
            CacheEntry.m_SRV.reset(new SRV(&m_device, SRVDesc, *CacheEntry.m_Resource));
        }

        return CacheEntry;
    }

    void ResourceCache::TakeCacheEntryOwnership(DXGI_FORMAT format, ResourceCacheEntry& entryOut)
    {
        ResourceCacheEntry& CacheEntry = m_Cache[format];
        entryOut = std::move(CacheEntry);
    }

}