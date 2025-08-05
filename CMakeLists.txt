cmake_minimum_required(VERSION 3.20)
project(renderLearn)
set(CMAKE_CXX_STANDARD 20)

option(ENABLE_DX11 "Enable Direct3D 11 support" ON)
option(ENABLE_DX12 "Enable Direct3D 12 support" ON)
option(ENABLE_OPENGL "Enable OpenGL" ON)

set(RENDER_SOURCE_PATH "${CMAKE_SOURCE_DIR}/Src" CACHE STRING "all source file's path")
set(RENDER_CMAKE_PATH "${CMAKE_SOURCE_DIR}/Cmake" CACHE STRING "all cmake tool file's path")
set(RENDER_3PARTY_PATH "${CMAKE_SOURCE_DIR}/3party" CACHE STRING "all 3party tool file's path")

include_directories(${RENDER_SOURCE_PATH})
include(${RENDER_3PARTY_PATH}/include.cmake)
include(${RENDER_CMAKE_PATH}/Preinit.cmake)

add_subdirectory(${RENDER_SOURCE_PATH})
