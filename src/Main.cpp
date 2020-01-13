// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

namespace D3D12TranslationLayer
{

TraceLoggingHProvider g_hTracelogging = nullptr;
void SetTraceloggingProvider(TraceLoggingHProvider hTracelogging)
{
    g_hTracelogging = hTracelogging;
}

}