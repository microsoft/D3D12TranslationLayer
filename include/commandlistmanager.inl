// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once
namespace D3D12TranslationLayer
{
    //----------------------------------------------------------------------------------------------------------------------------------
    // This allows one to atomically read 64 bit values
    inline LONGLONG InterlockedRead64(volatile LONGLONG* p)
    {
        return InterlockedCompareExchange64(p, 0, 0);
    }
};