// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    class ImmediateContext;

    class DeviceChild
    {
    public:
        DeviceChild(ImmediateContext* pParent) noexcept
            : m_pParent(pParent)
        {
        }

        ImmediateContext* m_pParent = nullptr;
        UINT64 m_LastUsedCommandListID[(UINT)COMMAND_LIST_TYPE::MAX_VALID] = {};
        bool m_bWaitForCompletionRequired = true;

        // Warning: this method is hidden in some derived child types, and is not virtual
        // Always ensure that this method is called on the most derived type.
        void UsedInCommandList(_In_range_(0, COMMAND_LIST_TYPE::MAX_VALID - 1) COMMAND_LIST_TYPE CommandListType, UINT64 CommandListID) noexcept
        {
            assert(CommandListType < COMMAND_LIST_TYPE::MAX_VALID);
            assert(CommandListID >= m_LastUsedCommandListID[(UINT)CommandListType]);
            m_LastUsedCommandListID[(UINT)CommandListType] = CommandListID;
        }

        void MarkUsedInCommandListIfNewer(COMMAND_LIST_TYPE CommandListType, UINT64 CommandListID) noexcept
        {
            if (CommandListID >= m_LastUsedCommandListID[(UINT)CommandListType])
            {
                UsedInCommandList(CommandListType, CommandListID);
            }
        }

        void ResetLastUsedInCommandList()
        {
            ZeroMemory(m_LastUsedCommandListID, sizeof(m_LastUsedCommandListID));
        }

    protected:
        template <typename TIface>
        void AddToDeferredDeletionQueue(unique_comptr<TIface>& spObject)
        {
            if (spObject)
            {
                AddToDeferredDeletionQueue(spObject.get());
                spObject.reset();
            }
        }

        template <typename TIface>
        void AddToDeferredDeletionQueue(unique_comptr<TIface>& spObject, COMMAND_LIST_TYPE CommandListType, UINT64 CommandListID)
        {
            // Convert from our existing array of command list IDs into a single command list ID
            // for the specified command list type, by resetting everything and marking just the one.
            ResetLastUsedInCommandList();
            MarkUsedInCommandListIfNewer(CommandListType, CommandListID);
            AddToDeferredDeletionQueue(spObject);
        }

        template <typename TIface>
        void AddToDeferredDeletionQueue(unique_comptr<TIface>& spObject, COMMAND_LIST_TYPE CommandListType)
        {
            AddToDeferredDeletionQueue(spObject, CommandListType, m_pParent->GetCommandListID(CommandListType));
        }

        void SwapIdentities(DeviceChild& Other)
        {
            for (DWORD i = 0; i < (UINT)COMMAND_LIST_TYPE::MAX_VALID; i++)
            {
                std::swap(m_LastUsedCommandListID[i], Other.m_LastUsedCommandListID[i]);
            }
            assert(m_pParent == Other.m_pParent);
            assert(m_bWaitForCompletionRequired == Other.m_bWaitForCompletionRequired);
        }

        void AddToDeferredDeletionQueue(ID3D12Object* pObject);
    };

    template <typename TIface>
    class DeviceChildImpl : public DeviceChild
    {
    public:
        DeviceChildImpl(ImmediateContext* pParent) noexcept
            : DeviceChild(pParent)
        {
        }
        void Destroy() { AddToDeferredDeletionQueue(m_spIface); }
        ~DeviceChildImpl() { Destroy(); }

        bool Created() { return m_spIface.get() != nullptr; }
        TIface** GetForCreate() { Destroy(); return &m_spIface; }
        TIface* GetForUse(COMMAND_LIST_TYPE CommandListType, UINT64 CommandListID)
        {
            MarkUsedInCommandListIfNewer(CommandListType, CommandListID);
            return m_spIface.get();
        }
        TIface* GetForUse(COMMAND_LIST_TYPE CommandListType)
        {
            return GetForUse(CommandListType, m_pParent->GetCommandListID(CommandListType));
        }
        TIface* GetForImmediateUse() { return m_spIface.get(); }

    private:
        unique_comptr<TIface> m_spIface;
    };
};