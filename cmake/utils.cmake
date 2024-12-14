message(STATUS "Cmake Utils file")

function(add_source_group directory group_name)
    file(GLOB hpp_files "${directory}/*.hpp")
    file(GLOB cpp_files "${directory}/*.cpp")

    # 处理 .hpp 文件
    if(hpp_files)
        message(STATUS "Add Header File into ${group_name}/include")
        message(STATUS "${hpp_files}")
        source_group("${group_name}/include" FILES ${hpp_files})
    endif()

    # 处理 .cpp 文件
    if(cpp_files)
        message(STATUS "Add Header File into ${group_name}/src")
        message(STATUS "${cpp_files}")
        source_group("${group_name}/src" FILES ${cpp_files})
    endif()

    file(GLOB subdirs "${directory}/*")
    foreach(subdir ${subdirs})
        if(IS_DIRECTORY ${subdir})
            get_filename_component(sname ${subdir} NAME)
            if(group_name)
                message(STATUS "add source group sub directory -- ${subdir} to ${group_name}/${sname}")
            else()
                message(STATUS "add source group sub directory -- ${subdir} to ${sname}")
            endif()            
            add_source_group("${subdir}" "${group_name}/${sname}")
        endif()
    endforeach()
endfunction()