cmake_minimum_required(VERSION 3.26)

project(
    sf_shaderinjector
    VERSION 1.8
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
# Set up the actual library
#
add_subdirectory("${PROJECT_SOURCE_PATH}")

#
# And finally produce build artifacts
#
if(BUILD_FOR_SFSE)
    set(PLUGIN_LOADER_OUTPUT_DIR "Data/SFSE/Plugins/")
    set(CPACK_PACKAGE_FILE_NAME "SSI-${CMAKE_PROJECT_VERSION_MAJOR}_${CMAKE_PROJECT_VERSION_MINOR} (SFSE-X_X_XX)")
elseif(BUILD_FOR_ASILOADER)
    set(PLUGIN_LOADER_OUTPUT_DIR "/")
    set(CPACK_PACKAGE_FILE_NAME "SSI-${CMAKE_PROJECT_VERSION_MAJOR}_${CMAKE_PROJECT_VERSION_MINOR} (GamePassASI-X_X_XX)")
endif()

install(
    FILES
        "${PROJECT_RESOURCES_PATH}/SFShaderInjector.ini"
    DESTINATION ${PLUGIN_LOADER_OUTPUT_DIR}
)

install(
    TARGETS
        sfse_output_dll
    RUNTIME
        DESTINATION ${PLUGIN_LOADER_OUTPUT_DIR}
)

set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
