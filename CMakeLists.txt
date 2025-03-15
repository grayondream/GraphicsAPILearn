cmake_minimum_required(VERSION 3.20)
project(renderLearn)
set(CMAKE_CXX_STANDARD 20)

message(STATUS ${DX_SDK_ROOT} "DirectX SDK ROOT: ${DX_SDK_ROOT}")
if(NOT DX_SDK_ROOT)
    message(FATAL_ERROR "The DirectX's sdk directory is empty, please set it with the variable DX_SDK_ROOT")
endif()

message(STATUS "DirectX SDK ROOT: ${DX_SDK_ROOT}")

if(NOT WINDOWS_SDK_ROOT)
    message(FATAL_ERROR "The Windows's sdk directory is empty, please set it with the variable WINDOWS_SDK_ROOT")
endif()

message(STATUS "Windows SDK ROOT: ${WINDOWS_SDK_ROOT}")

set(RENDER_SOURCE_PATH "${CMAKE_SOURCE_DIR}/Src" CACHE STRING "all source file's path")
set(RENDER_CMAKE_PATH "${CMAKE_SOURCE_DIR}/Cmake" CACHE STRING "all cmake tool file's path")
set(RENDER_3PARTY_PATH "${CMAKE_SOURCE_DIR}/3party" CACHE STRING "all 3party tool file's path")


include_directories(${RENDER_SOURCE_PATH})
include_directories(${WINDOWS_SDK_ROOT})
include(${RENDER_3PARTY_PATH}/include.cmake)

option(ENABLE_DX11 "Enable Direct3D 11 support" ON)
option(ENABLE_DX12 "Enable Direct3D 12 support" ON)
option(ENABLE_OPENGL "Enable OpenGL" ON)

if(ENABLE_DX11 OR ENABLE_DX12)
    include_directories(${DX_SDK_ROOT}/Include)
    link_directories(${DX_SDK_ROOT}/Lib/x64)
endif()


include(${RENDER_CMAKE_PATH}/Init.cmake)

set(PROJECT_NAME renderLearn)

add_subdirectory(${RENDER_SOURCE_PATH})
