set(DX_APP_PATH "${DX_SOURCE_PATH}/app" CACHE STRING "app file's path")
set(DX_EH_PATH "${DX_SOURCE_PATH}/EH" CACHE STRING "error handle file's path")

set(DX_SRC_FILES "" CACHE STRING "all source files")

add_subdirectory(${DX_APP_PATH})
add_subdirectory(${DX_EH_PATH})

add_executable(${PROJECT_NAME} WIN32 
${DX_APP_SRC_FILES} 
${DX_EH_SRC_FILES}
)

target_link_libraries(${PROJECT_NAME}
user32.lib
gdi32.lib
kernel32.lib
d3d11.lib
d3dcompiler.lib
dxguid.lib
)

