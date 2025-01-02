cmake_minimum_required(VERSION 3.20)
project(renderLearn)
set(CMAKE_CXX_STANDARD 20)

set(RENDER_SOURCE_PATH "${CMAKE_SOURCE_DIR}/src" CACHE STRING "all source file's path")
set(RENDER_CMAKE_PATH "${CMAKE_SOURCE_DIR}/cmake" CACHE STRING "all cmake tool file's path")
set(RENDER_3PARTY_PATH "${CMAKE_SOURCE_DIR}/3party" CACHE STRING "all 3party tool file's path")


include_directories(${RENDER_SOURCE_PATH})
include_directories(${WINDOWS_SDK_ROOT})
include(${RENDER_3PARTY_PATH}/include.cmake)

option(ENABLE_DX11 "Enable Direct3D 11 support" ON)
option(ENABLE_DX12 "Enable Direct3D 12 support" ON)
option(ENABLE_OPENGL "Enable OpenGL" ON)

if(ENABLE_DX11 OR ENABLE_DX12)
    include_directories(${DX_SDK_ROOT}/Include)
    include_directories(${DX_SDK_ROOT}/Samples/C++/Effects11/Inc)
    link_directories(${DX_SDK_ROOT}/Lib/x64)
endif()


include(${RENDER_CMAKE_PATH}/init.cmake)

set(PROJECT_NAME renderLearn)

add_subdirectory(${RENDER_SOURCE_PATH})
