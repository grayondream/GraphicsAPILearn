add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Res"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Res"
    COMMENT "Copying res directory to output path"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/Res"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../Res"
    COMMENT "Copying res directory to output path"
)