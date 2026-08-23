if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    return()
endif()

message(STATUS "Initialize ${CMAKE_SYSTEM_NAME}")
message(STATUS ${DX_SDK_ROOT} "DirectX SDK ROOT: ${DX_SDK_ROOT}")
if(NOT DX_SDK_ROOT)
    # 旧式手动指定 DirectX/Windows SDK 布局的路径；现代 VS 生成器经 Windows SDK 自动解析，
    # 未设置时直接跳过本文件（d3d11/d3d12/dxgi 导入库由工具链自带）
    message(STATUS "DX_SDK_ROOT not set, skip legacy Windows.cmake")
    return()
endif()

message(STATUS "DirectX SDK ROOT: ${DX_SDK_ROOT}")
if(NOT WINDOWS_SDK_ROOT)
    message(FATAL_ERROR "The Windows's sdk directory is empty, please set it with the variable WINDOWS_SDK_ROOT")
endif()

include_directories(${WINDOWS_SDK_ROOT})
include_directories(${WINDOWS_SDK_ROOT}/winrt)

if(ENABLE_DX12)
    include_directories(${DX_SDK_ROOT}/Include)
    link_directories(${DX_SDK_ROOT}/Lib/x64)
endif()