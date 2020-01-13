// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include "SharedResourceHelpers.hpp"

namespace D3D12TranslationLayer
{
    SharedResourceLocalHandle TRANSLATION_API SharedResourceHelpers::CreateKMTHandle(_In_ HANDLE resourceHandle)
    {
        CComPtr<IUnknown> spResource;
        ThrowFailure(m_ImmCtx.m_pDevice12->OpenSharedHandle(resourceHandle, IID_PPV_ARGS(&spResource)));
        UINT handle = GetHandleForResource(spResource);

        // No more exceptions
        spResource.Detach();
        return handle;
    }

    SharedResourceLocalHandle TRANSLATION_API SharedResourceHelpers::CreateKMTHandle(_In_ IUnknown* pResource)
    {
        return GetHandleForResource(pResource);
    }

    IUnknown* TRANSLATION_API SharedResourceHelpers::DetachKMTHandle(SharedResourceLocalHandle handle)
    {
        std::lock_guard lock(m_OpenResourceMapLock);
        // Called by C11On12 after a successful resource open
        assert(m_OpenResourceMap[handle] != nullptr);
        IUnknown* pUnknown = m_OpenResourceMap[handle];
        m_OpenResourceMap[handle] = nullptr;

        return pUnknown;
    }

    void TRANSLATION_API SharedResourceHelpers::DestroyKMTHandle(SharedResourceLocalHandle handle)
    {
        std::lock_guard lock(m_OpenResourceMapLock);
        // This method is called by the core layer in cases of failure during OpenSharedResource
        // The KM handle for a shared resource is simply an index into this
        // vector, and "destroying" the KM handle should release the underlying resource.
        if (m_OpenResourceMap[handle])
        {
            m_OpenResourceMap[handle]->Release();
            m_OpenResourceMap[handle] = nullptr;
        }
    }

    SharedResourceLocalHandle SharedResourceHelpers::GetHandleForResource(_In_ IUnknown* pResource)
    {
        std::lock_guard lock(m_OpenResourceMapLock);
        // The KM handle for a shared resource is simply an index into this
        // vector, to allow the core layer to clean up an 11on12 resource the same way as an 11 resource
        UINT i = 0;
        for (; i < m_OpenResourceMap.size(); ++i)
        {
            if (m_OpenResourceMap[i] == nullptr)
            {
                m_OpenResourceMap[i] = pResource;
                // It's now safe to detach the smart pointer
                return i;
            }
        }
        m_OpenResourceMap.push_back(pResource); // throw( bad_alloc )
                                                // It's now safe to detach the smart pointer
        return i;
    }

    SharedResourceHelpers::SharedResourceHelpers(ImmediateContext& ImmCtx, CreationFlags const& Flags) noexcept
        : m_ImmCtx(ImmCtx)
        , m_CreationFlags(Flags)
    {
    }

    void TRANSLATION_API SharedResourceHelpers::InitializePrivateDriverData(
        DeferredDestructionType destructionType, _Out_writes_bytes_(dataSize) void* pResourcePrivateDriverData, _In_ UINT dataSize)
    {
        if (dataSize != cPrivateResourceDriverDataSize)
        {
            ThrowFailure(E_INVALIDARG);
        }

        __analysis_assume(dataSize == cPrivateResourceDriverDataSize);
        SOpenResourcePrivateData* pPrivateData = (SOpenResourcePrivateData*)pResourcePrivateDriverData;
        *pPrivateData = SOpenResourcePrivateData(destructionType);
    }

    IUnknown* TRANSLATION_API SharedResourceHelpers::QueryResourceFromKMTHandle(SharedResourceLocalHandle handle)
    {
        std::lock_guard lock(m_OpenResourceMapLock);
        assert(m_OpenResourceMap[handle] != nullptr);

        return m_OpenResourceMap[handle];
    }


    //----------------------------------------------------------------------------------------------------------------------------------
    UINT ConvertPossibleBindFlags(D3D12_RESOURCE_DESC& Desc, D3D12_HEAP_FLAGS HeapFlags, const D3D12_FEATURE_DATA_FORMAT_SUPPORT& formatSupport, bool SupportDisplayableTextures)
    {
        UINT BindFlags = 0;
        if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            BindFlags |= D3D11_BIND_CONSTANT_BUFFER |
                D3D11_BIND_INDEX_BUFFER |
                D3D11_BIND_VERTEX_BUFFER |
                D3D11_BIND_STREAM_OUTPUT;
        }
        else if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D
            && SupportDisplayableTextures
            && (HeapFlags & D3D12_HEAP_FLAG_SHARED)
            && (HeapFlags & D3D12_HEAP_FLAG_ALLOW_DISPLAY)
            )
        {
            BindFlags |= D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER;
        }
        else if ((HeapFlags & D3D12_HEAP_FLAG_ALLOW_DISPLAY) == 0)
        {
            BindFlags |= (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_DECODER_OUTPUT) ? D3D11_BIND_DECODER : 0u;
            BindFlags |= (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_VIDEO_ENCODER) ? D3D11_BIND_VIDEO_ENCODER : 0u;
        }
        if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
            BindFlags |= D3D11_BIND_RENDER_TARGET;
        if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
            BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        if ((Desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0)
            BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        return BindFlags;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT ConvertHeapMiscFlags(D3D12_HEAP_FLAGS HeapFlags, bool bNtHandle, bool bKeyedMutex)
    {
        UINT MiscFlags = 0;
        if (HeapFlags & D3D12_HEAP_FLAG_SHARED)
            MiscFlags |= (bKeyedMutex ? D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX : D3D11_RESOURCE_MISC_SHARED) | (bNtHandle ? D3D11_RESOURCE_MISC_SHARED_NTHANDLE : 0);
        return MiscFlags;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    UINT ConvertPossibleCPUAccessFlags(D3D12_HEAP_PROPERTIES HeapProps, ID3D12Device* pDevice)
    {
        UINT CPUAccessFlags = 0;
        if (HeapProps.Type != D3D12_HEAP_TYPE_CUSTOM)
        {
            HeapProps = pDevice->GetCustomHeapProperties(HeapProps.CreationNodeMask, HeapProps.Type);
        }
        switch (HeapProps.CPUPageProperty)
        {
        case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK: CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ; break;
        case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE: CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; break;
        }
        return CPUAccessFlags;
    }

    void TRANSLATION_API SharedResourceHelpers::QueryResourceInfoFromKMTHandle(UINT handle, _In_opt_ const D3D11_RESOURCE_FLAGS* pOverrideFlags, _Out_ ResourceInfo* pResourceInfo)
    {
        if (pResourceInfo == nullptr)
        {
            ThrowFailure(E_INVALIDARG);
        }

        ZeroMemory(pResourceInfo, sizeof(*pResourceInfo));
        CComPtr<IUnknown> spUnknown = QueryResourceFromKMTHandle(handle);
        bool bShared = false;
        ID3D12Object* pObject = nullptr;

        // First, check for tile pools
        CComPtr<ID3D12Heap> spHeap;
        if (SUCCEEDED(spUnknown->QueryInterface(&spHeap)))
        {
            // Shared tile pools not supported on tier 1
            if (m_ImmCtx.GetCaps().TiledResourcesTier == D3D12_TILED_RESOURCES_TIER_1)
            {
                // TODO: Debug spew
                ThrowFailure(E_INVALIDARG);
            }

            pResourceInfo->TiledPool.m_HeapDesc = spHeap->GetDesc();
            pResourceInfo->m_Type = TiledPoolType;

            bShared = (pResourceInfo->TiledPool.m_HeapDesc.Flags & D3D12_HEAP_FLAG_SHARED) != 0;
            pObject = spHeap;
        }

        // Not a tile pool, must be a resource
        CComPtr<ID3D12Resource1> spResource;
        if (SUCCEEDED(spUnknown->QueryInterface(&spResource)))
        {
            D3D12_HEAP_FLAGS MiscFlags;
            pResourceInfo->m_Type = ResourceType;
            bShared = SUCCEEDED(spResource->GetHeapProperties(nullptr, &MiscFlags)) &&
                (MiscFlags & D3D12_HEAP_FLAG_SHARED) != 0;
            pObject = spResource;
        }

        if (!spResource && !spHeap)
        {
            // TODO: Debug spew
            ThrowFailure(E_INVALIDARG);
        }

        if (!bShared)
        {
            pResourceInfo->m_bAllocatedBy9on12 = false;
            pResourceInfo->m_bNTHandle = false;
            pResourceInfo->m_bSynchronized = false;
            pResourceInfo->m_GDIHandle = 0;
        }
        else
        {
            D3D12_COMPATIBILITY_SHARED_FLAGS CompatFlags;
            ThrowFailure(m_ImmCtx.m_pCompatDevice->ReflectSharedProperties(
                pObject, D3D12_REFELCT_SHARED_PROPERTY_COMPATIBILITY_SHARED_FLAGS, &CompatFlags, sizeof(CompatFlags)));

            pResourceInfo->m_bNTHandle = (CompatFlags & D3D12_COMPATIBILITY_SHARED_FLAG_NON_NT_HANDLE) == 0;
            pResourceInfo->m_bSynchronized = (CompatFlags & D3D12_COMPATIBILITY_SHARED_FLAG_KEYED_MUTEX) != 0;
            pResourceInfo->m_bAllocatedBy9on12 = (CompatFlags & D3D12_COMPATIBILITY_SHARED_FLAG_9_ON_12) != 0;

            if (pResourceInfo->m_bNTHandle)
            {
                pResourceInfo->m_GDIHandle = 0;
            }
            else
            {
                ThrowFailure(m_ImmCtx.m_pCompatDevice->ReflectSharedProperties(
                    pObject, D3D12_REFLECT_SHARED_PROPERTY_NON_NT_SHARED_HANDLE, &pResourceInfo->m_GDIHandle, sizeof(pResourceInfo->m_GDIHandle)));
            }
        }

        if (spResource)
        {
            D3D12_RESOURCE_DESC Desc = spResource->GetDesc();
            D3D11_RESOURCE_FLAGS Flags = { };
            D3D12_HEAP_FLAGS HeapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_HEAP_PROPERTIES HeapProps = { };

            if (Desc.Width > UINT_MAX)
            {
                // TODO: Debug spew
                ThrowFailure(E_INVALIDARG);
            }

            if (m_CreationFlags.SupportDisplayableTextures
                && Desc.Format == DXGI_FORMAT_420_OPAQUE
                )
            {
                // 420_OPAQUE doesn't exist in D3D12.
                Desc.Format = DXGI_FORMAT_NV12;
            }

            D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = { Desc.Format };
            (void)m_ImmCtx.m_pDevice12->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));

            if (pOverrideFlags || FAILED(m_ImmCtx.m_pCompatDevice->ReflectSharedProperties(
                spResource, D3D12_REFLECT_SHARED_PROPERTY_D3D11_RESOURCE_FLAGS, &Flags, sizeof(Flags))))
            {
                // First, determine the full set of capabilities from the resource
                Flags.MiscFlags = 0;
                Flags.CPUAccessFlags = 0;
                Flags.StructureByteStride = 0;

                if (SUCCEEDED(spResource->GetHeapProperties(&HeapProps, &HeapFlags)))
                {
                    // Committed or placed
                    Flags.MiscFlags |= ConvertHeapMiscFlags(HeapFlags, pResourceInfo->m_bNTHandle, pResourceInfo->m_bSynchronized);
                    if (m_CreationFlags.SupportDisplayableTextures
                        && (HeapFlags & D3D12_HEAP_FLAG_SHARED)
                        && Desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER
                        )
                    {
                        if (HeapFlags & D3D12_HEAP_FLAG_ALLOW_DISPLAY)
                        {
                            Flags.MiscFlags |= 0x100000 /*D3D11_RESOURCE_MISC_SHARED_DISPLAYABLE*/;
                        }
                        if (!(Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS))
                        {
                            Flags.MiscFlags |= 0x200000 /*D3D11_RESOURCE_MISC_SHARED_EXCLUSIVE_WRITER*/;
                        }
                    }
                    Flags.CPUAccessFlags = ConvertPossibleCPUAccessFlags(HeapProps, m_ImmCtx.m_pDevice12.get());
                }
                else
                {
                    // Reserved (tiled)
                    Flags.MiscFlags |= D3D11_RESOURCE_MISC_TILED;
                }

                Flags.BindFlags = ConvertPossibleBindFlags(Desc, HeapFlags, formatSupport, m_CreationFlags.SupportDisplayableTextures);

                {
                    CComPtr<ID3D12ProtectedResourceSession> spProtectedResourceSession;
                    if (SUCCEEDED(spResource->GetProtectedResourceSession(IID_PPV_ARGS(&spProtectedResourceSession))))
                    {
                        Flags.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;
                    }
                }

                UINT AssumableBindFlags = (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE |
                    D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS);
                UINT AssumableMiscFlags = (D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE |
                    D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                    D3D11_RESOURCE_MISC_TILED | D3D11_RESOURCE_MISC_TILE_POOL | D3D11_RESOURCE_MISC_HW_PROTECTED);

                if (m_CreationFlags.SupportDisplayableTextures)
                {
                    AssumableBindFlags |= D3D11_BIND_VIDEO_ENCODER | D3D11_BIND_DECODER;

                    // D3D11_RESOURCE_MISC_SHARED_DISPLAYABLE | D3D11_RESOURCE_MISC_SHARED_EXCLUSIVE_WRITER
                    AssumableMiscFlags |= 0x300000;
                }
                static const UINT AssumableCPUFlags = 0;

                if (pOverrideFlags)
                {
                    // Most misc flags will all be validated by the open shared resource process in the D3D11 core,
                    // since validity of misc flags depend on the final bind/CPU flags, not the possible ones
                    // The only ones we validate here are the ones that are based on heap properties (or lack thereof)
                    if ((pOverrideFlags->MiscFlags & AssumableMiscFlags) & ~Flags.MiscFlags ||
                        pOverrideFlags->BindFlags & ~Flags.BindFlags ||
                        pOverrideFlags->CPUAccessFlags & ~Flags.CPUAccessFlags)
                    {
                        // TODO: Debug spew
                        ThrowFailure(E_INVALIDARG);
                    }
                    if (pOverrideFlags->StructureByteStride && Desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
                    {
                        // TODO: Debug spew
                        ThrowFailure(E_INVALIDARG);
                    }

                    Flags.BindFlags = pOverrideFlags->BindFlags;
                    Flags.CPUAccessFlags = pOverrideFlags->CPUAccessFlags;
                    Flags.StructureByteStride = pOverrideFlags->StructureByteStride;
                }
                else
                {
                    Flags.BindFlags &= AssumableBindFlags;
                    Flags.CPUAccessFlags &= AssumableCPUFlags;
                    assert((Flags.MiscFlags & ~AssumableMiscFlags) == 0);
                }
            }
#if DBG
            else
            {
                // Ensure the 11 flags are a subset of capabilities implied by the 12 resource desc
                assert(SUCCEEDED(spResource->GetHeapProperties(&HeapProps, &HeapFlags)));
                assert((Flags.CPUAccessFlags & ~ConvertPossibleCPUAccessFlags(HeapProps, m_ImmCtx.m_pDevice12.get())) == 0);
                assert((Flags.BindFlags & ~ConvertPossibleBindFlags(Desc, HeapFlags, formatSupport, m_CreationFlags.SupportDisplayableTextures)) == 0);
                assert((~Flags.MiscFlags & ConvertHeapMiscFlags(HeapFlags, pResourceInfo->m_bNTHandle, pResourceInfo->m_bSynchronized)) == 0);
            }
#endif

            pResourceInfo->Resource.m_ResourceDesc = Desc;
            pResourceInfo->Resource.m_HeapFlags = HeapFlags;
            pResourceInfo->Resource.m_HeapProps = HeapProps;
            pResourceInfo->Resource.m_Flags11 = Flags;
        }
    }

    unique_comptr<Resource> TRANSLATION_API SharedResourceHelpers::OpenResourceFromKmtHandle(
        ResourceCreationArgs& createArgs,
        _In_ SharedResourceLocalHandle kmtHandle,
        _In_reads_bytes_(resourcePrivateDriverDataSize) void* pResourcePrivateDriverData,
        _In_ UINT resourcePrivateDriverDataSize,
        _In_ D3D12_RESOURCE_STATES currentState)
    {
        UNREFERENCED_PARAMETER(resourcePrivateDriverDataSize);

        IUnknown* pRefToRelease = QueryResourceFromKMTHandle(kmtHandle);

        assert(resourcePrivateDriverDataSize == sizeof(SOpenResourcePrivateData));
        SOpenResourcePrivateData* pPrivateData = reinterpret_cast<SOpenResourcePrivateData*>(pResourcePrivateDriverData);

        auto spResource = Resource::OpenResource(
            &m_ImmCtx,
            createArgs,
            pRefToRelease,
            pPrivateData->GetDeferredDestructionType(),
            currentState);

        if (pRefToRelease)
        {
            pRefToRelease->Release();
        }
        DetachKMTHandle(kmtHandle);
        return spResource;
    }
}