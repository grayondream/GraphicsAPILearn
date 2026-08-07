if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    return()
endif()

message(STATUS "Initialize ${CMAKE_SYSTEM_NAME}")
message(STATUS ${DX_SDK_ROOT} "DirectX SDK ROOT: ${DX_SDK_ROOT}")
if(NOT DX_SDK_ROOT)
    message(FATAL_ERROR "The DirectX's sdk directory is empty, please set it with the variable DX_SDK_ROOT")
endif()

message(STATUS "DirectX SDK ROOT: ${DX_SDK_ROOT}")
if(NOT WINDOWS_SDK_ROOT)
    message(FATAL_ERROR "The Windows's sdk directory is empty, please set it with the variable WINDOWS_SDK_ROOT")
endif()

include_directories(${WINDOWS_SDK_ROOT})
include_directories(${WINDOWS_SDK_ROOT}/winrt)

if(ENABLE_DX11 OR ENABLE_DX12)
    include_directories(${DX_SDK_ROOT}/Include)
    link_directories(${DX_SDK_ROOT}/Lib/x64)
endif()