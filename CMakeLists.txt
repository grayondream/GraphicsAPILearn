cmake_minimum_required(VERSION 3.20)
project(dx11learn)
set(CMAKE_CXX_STANDARD 20)

set(DX_SOURCE_PATH "${CMAKE_SOURCE_DIR}/src" CACHE STRING "all source file's path")
include_directories(${DX_SDK_ROOT}/Include)
include_directories(${DX_SOURCE_PATH})
include_directories(${WINDOWS_SDK_ROOT})
link_directories(${DX_SDK_ROOT}/Lib/x64)


set(PROJECT_NAME dxlearn)

add_subdirectory(${DX_SOURCE_PATH})
