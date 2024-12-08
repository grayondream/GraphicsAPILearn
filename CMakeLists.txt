cmake_minimum_required(VERSION 3.20)
project(dx11learn)
set(CMAKE_CXX_STANDARD 20)

include_directories(${DX_SDK_ROOT}/Include)
include_directories(${DX_SOURCE_PATH})
link_directories(${DX_SDK_ROOT}/Lib/x64)
set(PROJECT_NAME dxlearn)

set(DX_SOURCE_PATH "${CMAKE_SOURCE_DIR}/src" CACHE STRING "all source file's path")
add_subdirectory(${DX_SOURCE_PATH})
