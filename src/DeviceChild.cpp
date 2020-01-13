// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{
    void DeviceChild::AddToDeferredDeletionQueue(ID3D12Object* pObject)
    {
        m_pParent->AddObjectToDeferredDeletionQueue(pObject, m_LastUsedCommandListID, m_bWaitForCompletionRequired);
    }

    void BatchedDeviceChild::ProcessBatch()
    {
        m_Parent.ProcessBatch();
    }
};