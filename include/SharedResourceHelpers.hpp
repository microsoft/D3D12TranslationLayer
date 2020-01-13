// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    typedef UINT SharedResourceLocalHandle;

    class SOpenResourcePrivateData
    {
    public:
        SOpenResourcePrivateData(DeferredDestructionType deferredDestructionType) :
            m_deferredDestructionType(deferredDestructionType) {}

        DeferredDestructionType GetDeferredDestructionType() { return m_deferredDestructionType; }
    private:
        DeferredDestructionType m_deferredDestructionType;
    };

    class SharedResourceHelpers
    {
    public:
        struct CreationFlags
        {
            bool SupportDisplayableTextures;
        };
        SharedResourceHelpers(ImmediateContext& ImmCtx, CreationFlags const&) noexcept;

        static const UINT cPrivateResourceDriverDataSize = sizeof(SOpenResourcePrivateData);

        void TRANSLATION_API InitializePrivateDriverData(DeferredDestructionType destructionType, _Out_writes_bytes_(dataSize) void* pResourcePrivateDriverData, _In_ UINT dataSize);

        SharedResourceLocalHandle TRANSLATION_API CreateKMTHandle(_In_ HANDLE resourceHandle);
        SharedResourceLocalHandle TRANSLATION_API CreateKMTHandle(_In_ IUnknown* pResource);

        IUnknown* TRANSLATION_API QueryResourceFromKMTHandle(SharedResourceLocalHandle handle);
        void TRANSLATION_API QueryResourceInfoFromKMTHandle(SharedResourceLocalHandle handle, _In_opt_ const D3D11_RESOURCE_FLAGS* pOverrideFlags, _Out_ ResourceInfo* pResourceInfo);
        void TRANSLATION_API DestroyKMTHandle(SharedResourceLocalHandle handle);
        IUnknown* TRANSLATION_API DetachKMTHandle(SharedResourceLocalHandle handle);

        unique_comptr<Resource> TRANSLATION_API OpenResourceFromKmtHandle(
            ResourceCreationArgs& createArgs,
            _In_ SharedResourceLocalHandle kmtHandle,
            _In_reads_bytes_(dataSize) void* pResourcePrivateDriverData,
            _In_ UINT dataSize,
            _In_ D3D12_RESOURCE_STATES currentState);

    private:
        ImmediateContext& m_ImmCtx;
        const CreationFlags m_CreationFlags;

        SharedResourceLocalHandle GetHandleForResource(_In_ IUnknown* pResource) noexcept(false);
        IUnknown* GetResourceFromHandle(_In_ SharedResourceLocalHandle handle) noexcept(false);

        std::vector<IUnknown*> m_OpenResourceMap;
        std::mutex m_OpenResourceMapLock;
    };
}