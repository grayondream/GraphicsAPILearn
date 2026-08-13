set(RENDER_APP_PATH "${RENDER_SOURCE_PATH}/app" CACHE STRING "app file's path")
set(RENDER_BASE_PATH "${RENDER_SOURCE_PATH}/base" CACHE STRING "base file's path")
set(RENDER_NATIVE_PATH "${RENDER_SOURCE_PATH}/native" CACHE STRING "native file's path")
set(RENDER_UTILS_PATH "${RENDER_SOURCE_PATH}/utils" CACHE STRING "utils file's path")
set(RENDER_MODEL_PATH "${RENDER_SOURCE_PATH}/model" CACHE STRING "model file's path")
set(RENDER_GEOMETRY_PATH "${RENDER_SOURCE_PATH}/geometry" CACHE STRING "geometry file's path")
set(PROJECT_NAME renderLearn)

set(DX_SRC_FILES "" CACHE STRING "all source files")

add_source_group(${RENDER_SOURCE_PATH} "")

add_subdirectory(${RENDER_APP_PATH})
add_subdirectory(${RENDER_BASE_PATH})
add_subdirectory(${RENDER_NATIVE_PATH})
add_subdirectory(${RENDER_UTILS_PATH})
add_subdirectory(${RENDER_MODEL_PATH})
add_subdirectory(${RENDER_GEOMETRY_PATH})
add_subdirectory(${RENDER_SOURCE_PATH}/rhi)

add_executable(${PROJECT_NAME}
    ${ALL_CPP_FILES}
    ${ALL_HPP_FILES}
    ${IMGUI_SRC_FILES}
    ${GLAD_SRC_FILES}
)

include_directories(${RENDER_NATIVE_PATH})

if(${ENABLE_OPENGL})
    message("Graphic API Enable OpenGL")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_OPENGL=1)
endif()

if(${ENABLE_VULKAN})
    message("Graphic API Enable Vulkan")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_VULKAN=1)
    set(VK_SHADER_SOURCE "${CMAKE_SOURCE_DIR}/res/Vulkan")
    set(VK_SHADER_OUTPUT "${CMAKE_BINARY_DIR}/res/Vulkan")
    set(GLSLANG_VALIDATOR "" CACHE FILEPATH "Path to glslangValidator")
    if(NOT GLSLANG_VALIDATOR)
        find_program(_glslang_validator glslangValidator HINTS /usr/bin /usr/local/bin)
        set(GLSLANG_VALIDATOR "${_glslang_validator}")
    endif()
    if(NOT GLSLANG_VALIDATOR OR NOT EXISTS "${GLSLANG_VALIDATOR}")
        message(FATAL_ERROR "glslangValidator not found - install glslang-tools")
    endif()
    file(GLOB_RECURSE VK_GLSL_SOURCES "${VK_SHADER_SOURCE}/*.vert" "${VK_SHADER_SOURCE}/*.frag" "${VK_SHADER_SOURCE}/*.geom")
    foreach(src IN LISTS VK_GLSL_SOURCES)
        file(RELATIVE_PATH rel "${VK_SHADER_SOURCE}" "${src}")
        get_filename_component(rel_dir "${rel}" DIRECTORY)
        get_filename_component(name "${rel}" NAME)
        string(REGEX REPLACE "\\.[^.]+$" "" nospv "${name}")
        set(out "${VK_SHADER_OUTPUT}/${rel_dir}/${nospv}.spv")
        add_custom_command(OUTPUT "${out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VK_SHADER_OUTPUT}/${rel_dir}"
            COMMAND "${GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 -o "${out}" "${src}"
            DEPENDS "${src}"
            COMMENT "Compiling Vulkan shader ${rel}" VERBATIM)
        list(APPEND VK_SPV_OUTPUTS "${out}")
    endforeach()
    if(VK_SPV_OUTPUTS)
        add_custom_target(vk_shaders ALL DEPENDS ${VK_SPV_OUTPUTS})
    endif()
    find_package(Vulkan REQUIRED)
    target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
endif()

if(${ENABLE_DX11})
    message("Graphic API Enable D3D11")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DX11=1)
endif()

if(${ENABLE_DX12})
    message("Graphic API Enable D3D12")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DX12=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME}
    user32.lib
    gdi32.lib
    kernel32.lib
    )
endif()

if(ENABLE_DX11 OR ENABLE_DX12)
    target_link_libraries(${PROJECT_NAME}
        user32.lib
        gdi32.lib
        kernel32.lib
        d3d11.lib
        d3dx11.lib
        d3dcompiler.lib
        dxguid.lib
        )
endif()

if(ENABLE_OPENGL)
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${OPENGL_INCLUDE_DIR})
endif()

find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} glm::glm)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} imgui::imgui)

target_compile_definitions(${PROJECT_NAME} PRIVATE GAME_RES_ROOT="${GAME_RES_ROOT}")

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} spdlog::spdlog_header_only)
target_include_directories(${PROJECT_NAME} PRIVATE ${SPDLOG_INCLUDE_DIR})

find_package(assimp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} assimp::assimp)
target_include_directories(${PROJECT_NAME} PRIVATE ${ASSIMP_INCLUDE_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE RESOURCE_DIR="${CMAKE_SOURCE_DIR}/res")
include(${RENDER_SOURCE_PATH}/BuildPost.cmake)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} glfw)
target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE glfw3::glfw3)

find_package(cxxopts CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} cxxopts::cxxopts)

