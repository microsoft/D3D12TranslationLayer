// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{
    struct VIDEO_DECODER_OUTPUT_VIEW_DESC_INTERNAL
    {
        DXGI_FORMAT Format;
        UINT ArraySlice;
    };

    struct VIDEO_PROCESSOR_INPUT_VIEW_DESC_INTERNAL
    {
        DXGI_FORMAT Format;      // TODO: verify FourCC usage when doing VP work
        UINT MipSlice;
        UINT ArraySlice;
    };

    struct VIDEO_PROCESSOR_OUTPUT_VIEW_DESC_INTERNAL
    {
        DXGI_FORMAT Format;
        UINT MipSlice;
        UINT FirstArraySlice;
        UINT ArraySize;
    };
};