file(GLOB APP_CPP_FILES "*.cpp")
file(GLOB APP_HPP_FILES "*.hpp")

set(ALL_CPP_FILES ${ALL_CPP_FILES} ${APP_CPP_FILES} CACHE STRING "all source files" FORCE)
set(ALL_HPP_FILES ${ALL_HPP_FILES} ${APP_HPP_FILES} CACHE STRING "all header files" FORCE)

file(GLOB_RECURSE APP_SAMPLE_CPP_FILES "sample/*.cpp")
file(GLOB_RECURSE APP_SAMPLE_HPP_FILES "sample/*.hpp")
set(ALL_CPP_FILES ${ALL_CPP_FILES} ${APP_SAMPLE_CPP_FILES} CACHE STRING "all source files" FORCE)
set(ALL_HPP_FILES ${ALL_HPP_FILES} ${APP_SAMPLE_HPP_FILES} CACHE STRING "all header files" FORCE)

if(ENABLE_OPENGL OR ENABLE_METAL)
    file(GLOB_RECURSE APP_OPENGL_CPP_FILES "Samples/*.cpp")
    file(GLOB_RECURSE APP_OPENGL_HPP_FILES "Samples/*.hpp")
    set(ALL_CPP_FILES ${ALL_CPP_FILES} ${APP_OPENGL_CPP_FILES} CACHE STRING "all source files" FORCE)
    set(ALL_HPP_FILES ${ALL_HPP_FILES} ${APP_OPENGL_HPP_FILES} CACHE STRING "all header files" FORCE)
endif()

# ImGuiMetalWindow.mm 依赖 imgui_impl_metal.h（仅 ENABLE_METAL 时加入 include 路径），
# 故 .mm 文件只能在 Metal 启用时编译，不能随 ENABLE_OPENGL 一起被 Samples/*.mm 捞入。
if(ENABLE_METAL)
    file(GLOB_RECURSE APP_METAL_MM_FILES "Samples/*.mm")
    set(ALL_CPP_FILES ${ALL_CPP_FILES} ${APP_METAL_MM_FILES} CACHE STRING "all source files" FORCE)
endif()


if(ENABLE_VULKAN AND NOT APPLE)
    file(GLOB_RECURSE APP_VULKAN_CPP_FILES "Vulkan/*.cpp")
    file(GLOB_RECURSE APP_VULKAN_HPP_FILES "Vulkan/*.hpp")
    set(ALL_CPP_FILES ${ALL_CPP_FILES} ${APP_VULKAN_CPP_FILES} CACHE STRING "all source files" FORCE)
    set(ALL_HPP_FILES ${ALL_HPP_FILES} ${APP_VULKAN_HPP_FILES} CACHE STRING "all header files" FORCE)
endif()




