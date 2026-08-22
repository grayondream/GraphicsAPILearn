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
    # VKRenderer 需要 imgui_impl_vulkan.h（imgui Vulkan 后端头，位于 3party/imgui/Vulkan）。
    # 该目录由 3party/imgui 子 CMake 的 include_directories 声明，但因 add_subdirectory 隔离
    # 不传播到 src 的 renderLearn target（imgui_impl_vulkan.cpp 靠同目录引号 include 编译），
    # 故这里对本 target 显式添加。
    target_include_directories(${PROJECT_NAME} PRIVATE "${RENDER_3PARTY_PATH}/imgui/Vulkan")
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
        file(READ "${src}" _vk_glsl_content)
        # 跳过含裸非不透明 uniform（如 uniform mat4 model;）的 shader：
        # Vulkan 要求非不透明 uniform 放 block 内，GL 老式裸 uniform 需 Task 9 包进 UniformBlock 后才能编译。
        if(_vk_glsl_content MATCHES "uniform[ \t]+(mat[234]|vec[234]|float|int|uint|bool)[ \t]+[A-Za-z0-9_]+")
            message(STATUS "Skip VK shader (bare uniform, needs UniformBlock): ${rel}")
            continue()
        endif()
        set(out "${VK_SHADER_OUTPUT}/${rel_dir}/${name}.spv")
        add_custom_command(OUTPUT "${out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VK_SHADER_OUTPUT}/${rel_dir}"
            COMMAND "${GLSLANG_VALIDATOR}" -V --auto-map-locations --target-env vulkan1.3 -o "${out}" "${src}"
            DEPENDS "${src}"
            COMMENT "Compiling Vulkan shader ${rel}" VERBATIM)
        list(APPEND VK_SPV_OUTPUTS "${out}")
    endforeach()
    if(VK_SPV_OUTPUTS)
        add_custom_target(vk_shaders ALL DEPENDS ${VK_SPV_OUTPUTS})
    endif()
    find_package(Vulkan REQUIRED)
    # 链接系统 libvulkan 而非 vcpkg 的 Vulkan loader：
    # vcpkg 构建的 loader（debug/lib/libvulkan.so.1.4.357）不含窗口系统 surface 扩展
    # （无 VK_KHR_xcb_surface/xlib_surface/wayland_surface），glfw 运行时 dlopen 复用该
    # loader 后 glfwGetRequiredInstanceExtensions 返回 NULL，导致 VK 初始化失败。
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_library(VULKAN_SYSTEM_LIB NAMES libvulkan.so.1 PATHS /usr/lib/x86_64-linux-gnu /usr/lib NO_DEFAULT_PATH)
        if(NOT VULKAN_SYSTEM_LIB)
            message(FATAL_ERROR "System libvulkan not found - install mesa-vulkan-drivers / libvulkan1")
        endif()
        target_link_libraries(${PROJECT_NAME} ${VULKAN_SYSTEM_LIB})
        target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIR})
    else()
        target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
    endif()
endif()

if(${ENABLE_DX11})
    message("Graphic API Enable D3D11")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DX11=1)
endif()

if(${ENABLE_DX12})
    message("Graphic API Enable D3D12")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DX12=1)
    # Windows SDK 自带 d3d12/dxgi/dxguid 导入库，直接链接；
    # Linux 侧仅定义宏不链接（无 Windows SDK 导入库），保证既有 Linux 构建/CI 不受影响
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} d3d12.lib dxgi.lib dxguid.lib)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME}
    user32.lib
    gdi32.lib
    kernel32.lib
    )
endif()

# 仅 DX11 需要旧式 d3d11/d3dx11/d3dcompiler 导入库（d3dx11 已从现代 Windows SDK 移除，
# 若 DX12 也进此分支会 LNK1104；DX12 的链接见上方 ENABLE_DX12 块）
if(ENABLE_DX11)
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

