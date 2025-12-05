
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    return()
endif()

message(STATUS "Initalize ${CMAKE_SYSTEM_NAME}")
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.1" CACHE STRING "Minimum macOS deployment target")
message(STATUS "macOS Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")s