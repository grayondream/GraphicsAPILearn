set(IMGUI_SRC_PATH ${RENDER_3PARTY_PATH}/imgui CACHE STRING "imgui source path" FORCE)

set(IMGUI_SRC_FILES)

include_directories(${IMGUI_SRC_PATH})
aux_source_directory(${IMGUI_SRC_PATH} IMGUI_SRC_FILES)

if(ENABLE_DX11)
    include_directories(${IMGUI_SRC_PATH}/DX11)
    aux_source_directory(${IMGUI_SRC_PATH}/DX11 IMGUI_DX11_SRC_FILES)
    message(STATUS "IMGUI_DX11_SRC_FILES: ${IMGUI_DX11_SRC_FILES}")
    list(APPEND IMGUI_SRC_FILES ${IMGUI_DX11_SRC_FILES})
endif()

if(ENABLE_DX12)
    include_directories(${IMGUI_SRC_PATH}/DX12)
    aux_source_directory(${IMGUI_SRC_PATH}/DX12 IMGUI_DX12_SRC_FILES)
    message(STATUS "IMGUI_DX12_SRC_FILES: ${IMGUI_DX12_SRC_FILES}")

    list(APPEND IMGUI_SRC_FILES ${IMGUI_DX12_SRC_FILES})
endif()

if(ENABLE_OPENGL)
    include_directories(${IMGUI_SRC_PATH}/OpenGL)
    aux_source_directory(${IMGUI_SRC_PATH}/OpenGL IMGUI_OPENGL_SRC_FILES)
    message(STATUS "IMGUI_OPENGL_SRC_FILES: ${IMGUI_OPENGL_SRC_FILES}")

    list(APPEND IMGUI_SRC_FILES ${IMGUI_OPENGL_SRC_FILES})
endif()

if(ENABLE_VULKAN)
    include_directories(${IMGUI_SRC_PATH}/Vulkan)
    aux_source_directory(${IMGUI_SRC_PATH}/Vulkan IMGUI_VULKAN_SRC_FILES)
    message(STATUS "IMGUI_VULKAN_SRC_FILES: ${IMGUI_VULKAN_SRC_FILES}")

    list(APPEND IMGUI_SRC_FILES ${IMGUI_VULKAN_SRC_FILES})
endif()

if(ENABLE_METAL)
    include_directories(${IMGUI_SRC_PATH}/Metal)
    aux_source_directory(${IMGUI_SRC_PATH}/Metal IMGUI_METAL_SRC_FILES)
    message(STATUS "IMGUI_METAL_SRC_FILES: ${IMGUI_METAL_SRC_FILES}")

    list(APPEND IMGUI_SRC_FILES ${IMGUI_METAL_SRC_FILES})
endif()

set(IMGUI_SRC_FILES ${IMGUI_SRC_FILES} CACHE STRING "imgui source files" FORCE)





