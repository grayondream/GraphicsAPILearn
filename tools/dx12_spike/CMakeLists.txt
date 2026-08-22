cmake_minimum_required(VERSION 3.24)
project(dx12_spike CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(glfw3 CONFIG REQUIRED)

# DirectX-Headers vendor（含 wsl/winadapter.h 与 directx/*.h）
set(DX_HEADERS "${CMAKE_CURRENT_LIST_DIR}/../../3party/DirectX-Headers/include")
add_executable(spike spike.cpp)
target_include_directories(spike PRIVATE
    ${DX_HEADERS}
    ${DX_HEADERS}/wsl/stubs   # winadapter.h 内部 #include <unknwn.h> 等所需
)
# GUID 定义（__uuidof 展开的符号）与格式表
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../3party/DirectX-Headers ${CMAKE_BINARY_DIR}/dxh)
target_link_libraries(spike PRIVATE glfw Microsoft::DirectX-Guids "/usr/lib/wsl/lib/libd3d12.so" "/usr/lib/wsl/lib/libdxcore.so" ${CMAKE_DL_LIBS})
