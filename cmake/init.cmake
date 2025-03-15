function(include_cmake_files directory)
    file(GLOB CMAKE_FILES "${directory}/*.cmake")
    # 移除 init.cmake 文件
    list(REMOVE_ITEM CMAKE_FILES "${directory}/Init.cmake")

    # 包含所有收集到的 CMake 文件
    foreach(CMAKE_FILE ${CMAKE_FILES})
        include(${CMAKE_FILE})
    endforeach()
endfunction()

message(STATUS "${RENDER_CMAKE_PATH}")
include_cmake_files("${RENDER_CMAKE_PATH}")