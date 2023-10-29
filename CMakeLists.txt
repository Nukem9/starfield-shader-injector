cmake_minimum_required(VERSION 3.26)

project(
    sf_shaderinjector
    VERSION 1.0
    LANGUAGES CXX)

option(BUILD_FOR_SFSE "Build is meant for the Starfield Script Extender" OFF)
option(BUILD_FOR_ASILOADER "Build is meant for the Microsoft Store ASI loader" OFF)

if(BUILD_FOR_SFSE AND BUILD_FOR_ASILOADER)
	message(FATAL_ERROR "BUILD_FOR_ASILOADER and BUILD_FOR_SFSE cannot be enabled at the same time.")
endif()

set(PROJECT_DEPENDENCIES_PATH "${CMAKE_CURRENT_LIST_DIR}/dependencies")
set(PROJECT_RESOURCES_PATH "${CMAKE_CURRENT_LIST_DIR}/resources")
set(PROJECT_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/source")

#
# Store the current git commit hash for later use
#
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE BUILD_GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

#
# Add SFSE as necessary
#
if(BUILD_FOR_SFSE)
    set(SFSE_COMMON_DEP "${PROJECT_DEPENDENCIES_PATH}/sfse")
    add_subdirectory("${SFSE_COMMON_DEP}" EXCLUDE_FROM_ALL)

    target_compile_options(
	    sfse
	    PRIVATE
            "/EHsc"
    )
endif()

#
# Set up the actual library
#
add_subdirectory("${PROJECT_SOURCE_PATH}")

#
# And finally produce build artifacts
#
install(
    FILES
        "${PROJECT_RESOURCES_PATH}/SFShaderInjector.ini"
    DESTINATION "/"
)
