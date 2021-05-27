# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.
cmake_minimum_required(VERSION 3.13)
project(dxbcparser)

add_library(dxbcparser STATIC
    src/BlobContainer.cpp
    src/DXBCUtils.cpp
    include/BlobContainer.h
    include/DXBCUtils.h
    include/pch.h)

target_include_directories(dxbcparser
    PUBLIC include
    PUBLIC ../external)