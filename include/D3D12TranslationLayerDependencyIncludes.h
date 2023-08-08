// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

// The Windows build uses DBG for debug builds, but Visual Studio defaults to NDEBUG for retail
// We'll pick TRANSLATION_LAYER_DBG for CMake (VS) builds, and we'll convert DBG to that here
// for Windows builds
#if DBG
#define TRANSLATION_LAYER_DBG 1
#endif

//SDK Headers
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _ATL_NO_WIN_SUPPORT
#include <windows.h>
#include <atlbase.h>
#include <comdef.h>
#include <strsafe.h>

// This defines NTSTATUS and other types that are needed for kernel headers
#include <wincrypt.h>

#define INITGUID
#include <guiddef.h>
#include <directx/d3d12video.h>
#include <directx/dxcore.h>
#undef INITGUID
#include <directx/d3d12compatibility.h>
#include <directx/d3dx12.h>
#include <d3d11_3.h>
#include <dxgi1_5.h>
#include <d3dkmthk.h>

//STL
#include <utility>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <unordered_map>
#include <memory>
#include <cstddef>
#include <functional>
#include <new>
#include <bitset>
#include <algorithm>
#include <unordered_set>
#include <atomic>
#include <optional>
#include <mutex>
#include <bitset>
#include <type_traits>
#include <climits>
using std::min;
using std::max;

#ifndef assert
#include <assert.h>
#endif

#include <evntrace.h>
#include <traceloggingprovider.h>
#include <MicrosoftTelemetry.h>

#define INITGUID
#include <guiddef.h>
