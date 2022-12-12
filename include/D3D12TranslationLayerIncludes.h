// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#ifdef _WIN64
#define USE_PIX_ON_ALL_ARCHITECTURES
#endif

#ifdef USE_PIX
#include <pix3.h>
#endif

//Library Headers
#define TRANSLATION_API
#include "VideoViewHelper.hpp"
#include "SubresourceHelpers.hpp"
#include "Util.hpp"
#include "DeviceChild.hpp"

#include <BlockAllocators.h>
#include "Allocator.h"
#include "XPlatHelpers.h"

#include <ThreadPool.hpp>
#include <segmented_stack.h>
#include <formatdesc.hpp>
#include <dxgiColorSpaceHelper.h>

#include "MaxFrameLatencyHelper.hpp"
#include "Shader.hpp"
#include "Sampler.hpp"
#include "View.hpp"
#include "PipelineState.hpp"
#include "SwapChainManager.hpp"
#include "ResourceBinding.hpp"
#include "Fence.hpp"
#include "Residency.h"
#include "ResourceState.hpp"
#include "RootSignature.hpp"
#include "Resource.hpp"
#include "Query.hpp"
#include "ResourceCache.hpp"
#include "BlitHelper.hpp"
#include "ImmediateContext.hpp"
#include "BatchedContext.hpp"
#include "BatchedResource.hpp"
#include "BatchedQuery.hpp"
#include "CommandListManager.hpp"
#include "VideoDecodeStatistics.hpp"
#include "VideoReferenceDataManager.hpp"
#include "VideoDecode.hpp"
#include "VideoDevice.hpp"
#include "VideoProcess.hpp"
#include "VideoProcessEnum.hpp"

#include "View.inl"
#include "Sampler.inl"
#include "Shader.inl"
#include "ImmediateContext.inl"
#include "CommandListManager.inl"
#include <BlockAllocators.inl>

#ifndef MICROSOFT_TELEMETRY_ASSERT
#define MICROSOFT_TELEMETRY_ASSERT(x) assert(x)
#endif

namespace D3D12TranslationLayer
{
extern TraceLoggingHProvider g_hTracelogging;
void SetTraceloggingProvider(TraceLoggingHProvider hTracelogging);
}
