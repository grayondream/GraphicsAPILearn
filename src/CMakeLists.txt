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

if(${ENABLE_VULKAN} AND APPLE)
    # macOS 无系统 Vulkan，需 MoltenVK 且本项目在 macOS 以 Metal/OpenGL 为主，
    # 故 Vulkan 后端在 macOS 上整体跳过（不影响其他平台 REQUIRED 行为）。
    message(WARNING "Vulkan requested but skipped on macOS (Apple) - VK backend disabled; other platforms still require Vulkan SDK")
endif()

if(${ENABLE_VULKAN} AND NOT APPLE)
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
        find_program(_glslang_validator glslangValidator HINTS
            /usr/bin /usr/local/bin
            "$ENV{VCPKG_ROOT}/installed/x64-windows/tools/glslang"
            "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/tools/glslang")
        set(GLSLANG_VALIDATOR "${_glslang_validator}")
    endif()
    # SPIR-V 由 vk_shaders 目标生成于 build/res/Vulkan；双副本工作流下 Windows 侧可无
    # glslangValidator，直接复用 Linux 侧编译好的 .spv 产物树，仅 WARNING 不阻塞配置。
    set(_vk_shader_target_available OFF)
    if(GLSLANG_VALIDATOR AND EXISTS "${GLSLANG_VALIDATOR}")
        set(_vk_shader_target_available ON)
    else()
        message(WARNING "glslangValidator not found - skip vk_shaders target, expect prebuilt .spv under ${VK_SHADER_OUTPUT} (dual-copy workflow)")
    endif()
    if(_vk_shader_target_available)
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

if(${ENABLE_DX12})
    message("Graphic API Enable D3D12")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DX12=1)
    # DXRenderer 需要 imgui_impl_dx12.h（imgui DX12 后端头，位于 3party/imgui/DX12）。
    # 该目录由 3party/imgui 子 CMake 的 include_directories 声明，但因 add_subdirectory
    # 隔离不传播到 src 的 renderLearn target（imgui_impl_dx12.cpp 靠同目录引号 include
    # 编译），故这里对本 target 显式添加（同 Vulkan 块的做法）。
    target_include_directories(${PROJECT_NAME} PRIVATE "${RENDER_3PARTY_PATH}/imgui/DX12")
    # Windows SDK 自带 d3d12/dxgi/dxguid/d3dcompiler 导入库，直接链接；
    # Linux 侧仅定义宏不链接（无 Windows SDK 导入库），保证既有 Linux 构建/CI 不受影响
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} d3d12.lib dxgi.lib dxguid.lib d3dcompiler.lib)
    endif()
    # ---- dxc 构建目标（仿 vk_shaders；产物 build/res/DX12/<dir>/<name>.<stage>.cso，
    #      与 DXShader::LocateCso 的查找路径约定一致）----
    find_program(DXC_TOOL dxc HINTS "$ENV{VCPKG_ROOT}/installed/x64-windows/tools/directx-dxc")
    if(DXC_TOOL)
        set(DX_SHADER_SOURCE "${CMAKE_SOURCE_DIR}/res/DX12")
        set(DX_SHADER_OUTPUT "${CMAKE_BINARY_DIR}/res/DX12")
        file(GLOB_RECURSE DX_HLSL_SOURCES
            "${DX_SHADER_SOURCE}/*.vert.hlsl"
            "${DX_SHADER_SOURCE}/*.frag.hlsl"
            "${DX_SHADER_SOURCE}/*.geom.hlsl")
        foreach(src IN LISTS DX_HLSL_SOURCES)
            file(RELATIVE_PATH rel "${DX_SHADER_SOURCE}" "${src}")
            get_filename_component(rel_dir "${rel}" DIRECTORY)
            string(REGEX REPLACE "\\.(vert|frag|geom)\\.hlsl$" "" base "${rel}")
            if(rel MATCHES "\\.vert\\.hlsl$")
                set(profile vs_6_1)
                set(entry VSMain)
                set(stage vert)
            elseif(rel MATCHES "\\.frag\\.hlsl$")
                set(profile ps_6_1)
                set(entry PSMain)
                set(stage frag)
            else()
                set(profile gs_6_0)
                set(entry GSMain)
                set(stage geom)
            endif()
            set(out "${DX_SHADER_OUTPUT}/${base}.${stage}.cso")
            add_custom_command(OUTPUT "${out}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DX_SHADER_OUTPUT}/${rel_dir}"
                COMMAND "${DXC_TOOL}" -T ${profile} -E ${entry} -Fo "${out}" "${src}"
                DEPENDS "${src}" COMMENT "Compiling DX shader ${rel}" VERBATIM)
            list(APPEND DX_CSO_OUTPUTS "${out}")
        endforeach()
        if(DX_CSO_OUTPUTS)
            add_custom_target(dx_shaders ALL DEPENDS ${DX_CSO_OUTPUTS})
            add_dependencies(renderLearn dx_shaders)
        endif()
    else()
        message(WARNING "[DX12] dxc not found - dx_shaders skipped, HLSL will not compile to .cso")
    endif()
endif()

if(${ENABLE_DX11})
    message("Graphic API Enable D3D11")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DX11=1)
    # Windows SDK 自带 d3d11/dxgi/d3dcompiler 导入库，直接链接；Linux 侧仅定义宏
    # 不链接不编译源（源文件组由 rhi/CMakeLists 按 WIN32 门控），保证 Linux 构建/CI 不受影响
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} d3d11.lib dxgi.lib d3dcompiler.lib)
    endif()
    # DX11Renderer 需要 imgui_impl_dx11.h（imgui DX11 后端头，位于 3party/imgui/DX11）。
    # 该目录由 3party/imgui 子 CMake 的 include_directories 声明，但因 add_subdirectory
    # 隔离不传播到 src 的 renderLearn target（imgui_impl_dx11.cpp 靠同目录引号 include
    # 编译），故这里对本 target 显式添加（同 Vulkan/DX12 块的做法）。
    target_include_directories(${PROJECT_NAME} PRIVATE "${RENDER_3PARTY_PATH}/imgui/DX11")
    # ---- fxc 构建目标（仿 dx_shaders；产物 build/res/DX11/<dir>/<name>.<stage>.fxc，
    #      与 DX11Shader::LocateFxc 的查找路径约定一致。SM5.0 目标只能用 SDK 自带
    #      fxc.exe 编译，dxc 不支持 5_0 profile）----
    if(WIN32)
        # fxc 发现：优先显式缓存变量，再查 WindowsSdkDir 环境变量与各盘常见
        # Windows Kits 安装根（SDK 可装在非 C 盘），取版本号最大者
        set(FXC_TOOL "" CACHE FILEPATH "Path to fxc.exe (Windows SDK HLSL compiler)")
        if(NOT FXC_TOOL)
            set(_FXC_KIT_ROOTS "")
            if(EXISTS "$ENV{WindowsSdkDir}")
                list(APPEND _FXC_KIT_ROOTS "$ENV{WindowsSdkDir}")
            endif()
            foreach(_drive C D E F G)
                list(APPEND _FXC_KIT_ROOTS "${_drive}:/Program Files (x86)/Windows Kits/10")
                list(APPEND _FXC_KIT_ROOTS "${_drive}:/Windows Kits/10")
            endforeach()
            set(FXC_CANDIDATES "")
            foreach(_root IN LISTS _FXC_KIT_ROOTS)
                if(EXISTS "${_root}/bin")
                    file(GLOB _fxc_found
                        "${_root}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64/fxc.exe"
                        "${_root}/bin/*/x64/fxc.exe")
                    list(APPEND FXC_CANDIDATES ${_fxc_found})
                endif()
            endforeach()
            if(FXC_CANDIDATES)
                # 版本号零填充目录按字典序排列：取最大者即最新 SDK
                list(SORT FXC_CANDIDATES)
                list(REVERSE FXC_CANDIDATES)
                list(GET FXC_CANDIDATES 0 _FXC_BEST)
                set(FXC_TOOL "${_FXC_BEST}" CACHE FILEPATH "Path to fxc.exe (Windows SDK HLSL compiler)" FORCE)
            endif()
        endif()
        if(FXC_TOOL)
            message(STATUS "[DX11] fxc found: ${FXC_TOOL}")
            set(DX11_SHADER_SOURCE "${CMAKE_SOURCE_DIR}/res/DX11")
            set(DX11_SHADER_OUTPUT "${CMAKE_BINARY_DIR}/res/DX11")
            file(GLOB_RECURSE DX11_HLSL_SOURCES
                "${DX11_SHADER_SOURCE}/*.vert.hlsl"
                "${DX11_SHADER_SOURCE}/*.frag.hlsl"
                "${DX11_SHADER_SOURCE}/*.geom.hlsl")
            foreach(src IN LISTS DX11_HLSL_SOURCES)
                file(RELATIVE_PATH rel "${DX11_SHADER_SOURCE}" "${src}")
                get_filename_component(rel_dir "${rel}" DIRECTORY)
                string(REGEX REPLACE "\\.(vert|frag|geom)\\.hlsl$" "" base "${rel}")
                if(rel MATCHES "\\.vert\\.hlsl$")
                    set(profile vs_5_0)
                    set(entry VSMain)
                    set(stage vert)
                elseif(rel MATCHES "\\.frag\\.hlsl$")
                    set(profile ps_5_0)
                    set(entry PSMain)
                    set(stage frag)
                else()
                    set(profile gs_5_0)
                    set(entry GSMain)
                    set(stage geom)
                endif()
                set(out "${DX11_SHADER_OUTPUT}/${base}.${stage}.fxc")
                add_custom_command(OUTPUT "${out}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${DX11_SHADER_OUTPUT}/${rel_dir}"
                    COMMAND "${FXC_TOOL}" /T ${profile} /E ${entry} /Fo "${out}" "${src}"
                    DEPENDS "${src}" COMMENT "Compiling DX11 shader ${rel}" VERBATIM)
                list(APPEND DX11_FXC_OUTPUTS "${out}")
            endforeach()
            if(DX11_FXC_OUTPUTS)
                add_custom_target(dx11_shaders ALL DEPENDS ${DX11_FXC_OUTPUTS})
                add_dependencies(renderLearn dx11_shaders)
            endif()
        else()
            message(WARNING "[DX11] fxc not found - dx11_shaders skipped, HLSL will not compile to .fxc")
        endif()
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME}
    user32.lib
    gdi32.lib
    kernel32.lib
    )
endif()

if(${ENABLE_METAL})
    message("Graphic API Enable Metal")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_METAL=1)
    target_include_directories(${PROJECT_NAME} PRIVATE "${RENDER_3PARTY_PATH}/imgui/Metal")
    if(APPLE)
        find_library(METAL_FRAMEWORK Metal REQUIRED)
        find_library(QUARTZCORE QuartzCore REQUIRED)
        find_library(COREGRAPHICS CoreGraphics REQUIRED)
        target_link_libraries(${PROJECT_NAME} ${METAL_FRAMEWORK} ${QUARTZCORE} ${COREGRAPHICS})
    endif()
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

