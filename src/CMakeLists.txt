set(RAPP_PATH "${RENDER_SOURCE_PATH}/App" CACHE STRING "app file's path")
set(RENDER_EH_PATH "${RENDER_SOURCE_PATH}/EH" CACHE STRING "error handle file's path")
set(RENDER_TRACY_PATH "${RENDER_SOURCE_PATH}/Tracy" CACHE STRING "tracy file's path")
set(RENDER_BASE_PATH "${RENDER_SOURCE_PATH}/Base" CACHE STRING "base file's path")
set(RENDER_CONFIG_PATH "${RENDER_SOURCE_PATH}/Config" CACHE STRING "config file's path")

set(DX_SRC_FILES "" CACHE STRING "all source files")

add_subdirectory(${RAPP_PATH})
add_subdirectory(${RENDER_EH_PATH})
add_subdirectory(${RENDER_TRACY_PATH})
add_subdirectory(${RENDER_BASE_PATH})
add_subdirectory(${RENDER_CONFIG_PATH})

add_source_group(${RENDER_SOURCE_PATH} "")

file(GLOB_RECURSE ALL_HPP_FILES "${RENDER_SOURCE_PATH}/*.hpp")
file(GLOB_RECURSE ALL_CPP_FILES "${RENDER_SOURCE_PATH}/*.cpp")

if(NOT ENABLE_OPENGL)
    remove_excluded_sources(ALL_CPP_FILES ${RAPP_PATH}/GL/)
    remove_excluded_sources(ALL_HPP_FILES ${RAPP_PATH}/GL/)
endif()

if(NOT ENABLE_DX11)
    remove_excluded_sources(ALL_CPP_FILES ${RAPP_PATH}/DX11/)
    remove_excluded_sources(ALL_HPP_FILES ${RAPP_PATH}/DX11/)
endif()

if(NOT ENABLE_DX12)
    remove_excluded_sources(ALL_CPP_FILES ${RAPP_PATH}/DX12/)
    remove_excluded_sources(ALL_HPP_FILES ${RAPP_PATH}/DX12/)
endif()

message(STATUS "All source files: ${ALL_CPP_FILES}")

add_executable(${PROJECT_NAME} WIN32 
${ALL_CPP_FILES} 
${ALL_HPP_FILES}
${GLAD_SRC_FILES}
${IMGUI_SRC_FILES}
)

target_link_libraries(${PROJECT_NAME}
user32.lib
gdi32.lib
kernel32.lib
)

if(ENABLE_DX11 OR ENABLE_DX12)
    target_link_libraries(${PROJECT_NAME}
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

target_link_libraries(${PROJECT_NAME}
user32.lib
gdi32.lib
kernel32.lib
d3d11.lib
d3dx11.lib
d3dcompiler.lib
dxguid.lib
)

find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} glm::glm)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} imgui::imgui)

target_compile_definitions(${PROJECT_NAME} PRIVATE GAME_RES_ROOT="${GAME_RES_ROOT}")

if(${ENABLE_OPENGL})
    message("Graphic API Enabel OPenGL")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_OPENGL=1)
endif()

if(${ENABLE_DX11})
    message("Graphic API Enabel D3D11")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DX11=1)
endif()

if(${ENABLE_DX12})
    message("Graphic API Enabel D3D12")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DX12=1)
endif()

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} spdlog::spdlog_header_only)
target_include_directories(${PROJECT_NAME} PRIVATE ${SPDLOG_INCLUDE_DIR})

find_package(assimp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} assimp::assimp)
target_include_directories(${PROJECT_NAME} PRIVATE ${ASSIMP_INCLUDE_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE RESOURCE_DIR="${CMAKE_SOURCE_DIR}/Res")
include(${RENDER_SOURCE_PATH}/BuildPost.cmake)