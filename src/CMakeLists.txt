# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.
cmake_minimum_required(VERSION 3.13)
include(CheckCXXSourceCompiles)

set(SRC
	Allocator.cpp
	BatchedContext.cpp
	BlitHelper.cpp
	ColorConvertHelper.cpp
	CommandListManager.cpp
	DeviceChild.cpp
	Fence.cpp
	FormatDescImpl.cpp
	ImmediateContext.cpp
	Main.cpp
	MaxFrameLatencyHelper.cpp
	PipelineState.cpp
	Query.cpp
	Resource.cpp
	ResourceBinding.cpp
	ResourceCache.cpp
	ResourceState.cpp
	RootSignature.cpp
	Shader.cpp
	SubresourceHelpers.cpp
	SwapChainHelper.cpp
	SwapChainManager.cpp
	Util.cpp
	VideoDecode.cpp
	VideoDecodeStatistics.cpp
	VideoDevice.cpp
	VideoProcess.cpp
	VideoProcessEnum.cpp
	VideoReferenceDataManager.cpp
	View.cpp)

set (INC
	../include/Allocator.h
	../include/BatchedContext.hpp
	../include/BatchedQuery.hpp
	../include/BatchedResource.hpp
	../include/BlitHelper.hpp
	../include/BlitHelperShaders.h
	../include/BlockAllocators.h
	../include/CommandListManager.hpp
	../include/D3D12TranslationLayerDependencyIncludes.h
	../include/D3D12TranslationLayerIncludes.h
	../include/d3dx12residency.h
	../include/DeviceChild.hpp
	../include/DXGIColorSpaceHelper.h
	../include/Fence.hpp
	../include/FormatDesc.hpp
	../include/ImmediateContext.hpp
	../include/MaxFrameLatencyHelper.hpp
	../include/pch.h
	../include/PipelineState.hpp
	../include/PrecompiledShaders.h
	../include/Query.hpp
	../include/Resource.hpp
	../include/ResourceBinding.hpp
	../include/ResourceCache.hpp
	../include/ResourceState.hpp
	../include/RootSignature.hpp
	../include/Sampler.hpp
	../include/segmented_stack.h
	../include/Shader.hpp
	../include/SubresourceHelpers.hpp
	../include/SwapChainHelper.hpp
	../include/SwapChainManager.hpp
	../include/ThreadPool.hpp
	../include/Util.hpp
	../include/VideoDecode.hpp
	../include/VideoDecodeStatistics.hpp
	../include/VideoDevice.hpp
	../include/VideoProcess.hpp
	../include/VideoProcessEnum.hpp
	../include/VideoProcessShaders.h
	../include/VideoReferenceDataManager.hpp
	../include/VideoViewHelper.hpp
	../include/View.hpp)

file(GLOB INL ../include/*.inl *.inl)

file(GLOB EXTERNAL_INC ../external/*.h ../external/*.hpp)

add_library(d3d12translationlayer STATIC ${SRC} ${INC} ${INL} ${EXTERNAL_INC})
target_include_directories(d3d12translationlayer
    PUBLIC ../external
    PUBLIC ../include
    PRIVATE ./)

if (CMAKE_VERSION VERSION_GREATER 3.16)
	target_precompile_headers(d3d12translationlayer PRIVATE ../include/pch.h)
endif()
    
source_group(Inlines FILES ${INL})
source_group("Header Files\\External" FILES ${EXTERNAL_INC})

target_link_libraries(d3d12translationlayer Microsoft::DirectX-Headers d3d12 dxgi atls)

# Using a compile test instead of find_library so this doesn't have to be run from a VS command prompt
check_cxx_source_compiles("
#pragma comment(lib, \"dxcore.lib\")
#include <dxcore.h>
int main() {
	IDXCoreAdapterFactory *fac;
	return DXCoreCreateAdapterFactory(&fac);
}" HAVE_DXCORE_LIBRARY)
if (HAVE_DXCORE_LIBRARY)
	target_link_libraries(d3d12translationlayer dxcore)
	target_link_options(d3d12translationlayer INTERFACE "/DELAYLOAD:dxcore.dll")
else()
	target_compile_definitions(d3d12translationlayer PRIVATE DYNAMIC_LOAD_DXCORE=1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../packages.config ${CMAKE_CURRENT_BINARY_DIR}/packages.config COPYONLY)
add_library(WinPixEventRuntime INTERFACE IMPORTED GLOBAL)
set_target_properties(WinPixEventRuntime PROPERTIES INTERFACE_LINK_LIBRARIES "${CMAKE_BINARY_DIR}/packages/WinPixEventRuntime.1.0.190604001/build/WinPixEventRuntime.targets")
target_link_libraries(d3d12translationlayer WinPixEventRuntime)

target_compile_definitions(d3d12translationlayer PRIVATE $<$<CONFIG:DEBUG>:DBG>)
target_compile_definitions(d3d12translationlayer PUBLIC $<$<CONFIG:DEBUG>:TRANSLATION_LAYER_DBG=1>)

if (USE_PIX)
	target_compile_definitions(d3d12translationlayer PUBLIC USE_PIX)
endif()

if(MSVC)
  target_compile_options(d3d12translationlayer PUBLIC /W4 /WX /wd4238 /wd4324)
  target_link_options(d3d12translationlayer INTERFACE "/ignore:4286")
else()
  target_compile_options(d3d12translationlayer PUBLIC -Wall -Wextra -pedantic -Werror)
endif()

if (NOT IMPLEMENT_RECTS)
    target_compile_definitions(d3d12translationlayer PRIVATE NO_IMPLEMENT_RECT_FNS)
endif()

include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX(d3d12TokenizedProgramFormat.hpp HAS_WDK)

if (HAS_WDK)
    message(NOTICE "Adding WDK-dependent project.")
	target_compile_definitions(d3d12translationlayer PUBLIC SUPPORTS_DXBC_PARSE)

	add_library(d3d12translationlayer_wdk STATIC
		DxbcBuilder.cpp
		ShaderBinary.cpp
		ShaderParser.cpp
        SharedResourceHelpers.cpp
		../include/DxbcBuilder.hpp
		../include/ShaderBinary.h
        ../include/SharedResourceHelpers.hpp)
	target_link_libraries(d3d12translationlayer_wdk d3d12translationlayer)

else()
    message(WARNING "Only adding SDK-dependent projects.")
endif()
