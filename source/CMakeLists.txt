#
# Set up the source files and output library
#
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(
	GLOB HEADER_FILES
	LIST_DIRECTORIES FALSE
	CONFIGURE_DEPENDS
	"${SOURCE_DIR}/Hooking/*.h"
	"${SOURCE_DIR}/RE/*.h"
	"${SOURCE_DIR}/*.h"
)

file(
	GLOB SOURCE_FILES
	LIST_DIRECTORIES FALSE
	CONFIGURE_DEPENDS
	"${SOURCE_DIR}/Hooking/*.cpp"
	"${SOURCE_DIR}/RE/*.cpp"
	"${SOURCE_DIR}/*.cpp"
)

source_group(
	TREE "${SOURCE_DIR}/.."
	FILES
		${HEADER_FILES}
		${SOURCE_FILES}
)

add_library(
	"${PROJECT_NAME}"
	SHARED
		${HEADER_FILES}
		${SOURCE_FILES}
)

target_precompile_headers(
	"${PROJECT_NAME}"
    PRIVATE
        pch.h
)

target_include_directories(
	"${PROJECT_NAME}"
	PRIVATE
		"${SOURCE_DIR}"
)

set_target_properties(
	"${PROJECT_NAME}"
	PROPERTIES
		OUTPUT_NAME "SFShaderInjector"
		MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase"
)

if(BUILD_FOR_ASILOADER)
	set_target_properties(
		"${PROJECT_NAME}"
		PROPERTIES
			SUFFIX ".asi"
	)
endif()

#
# Compiler-specific options
#
target_compile_features(
	"${PROJECT_NAME}"
	PRIVATE
		cxx_std_23
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(
		"${PROJECT_NAME}"
		PRIVATE
			"/utf-8"
			"/sdl"
			"/permissive-"
			"/Zc:preprocessor"
			"/Zc:inline"
			"/EHsc"

			"/W4"
			"/wd4100"	# '': unreferenced formal parameter
			"/wd4200"	# nonstandard extension used: zero-sized array in struct/union
			"/wd4201"	# nonstandard extension used: nameless struct/union
			"/wd4324"	# '': structure was padded due to alignment specifier
	)

	target_link_options(
		"${PROJECT_NAME}"
		PRIVATE
			# Force generate release PDBs: https://learn.microsoft.com/en-us/cpp/build/reference/debug-generate-debug-info
			"$<$<CONFIG:RELEASE>:/DEBUG:FULL;/OPT:REF;/OPT:ICF>"
			# Anonymize RSDS PDB paths
			"$<$<CONFIG:RELEASE>:/PDBALTPATH:$<TARGET_FILE_PREFIX:${PROJECT_NAME}>$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.pdb>"
	)
endif()

target_compile_definitions(
	"${PROJECT_NAME}"
	PRIVATE
		NOMINMAX
		VC_EXTRALEAN
		WIN32_LEAN_AND_MEAN

		BUILD_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
		BUILD_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
		BUILD_FOR_ASILOADER=$<BOOL:${BUILD_FOR_ASILOADER}>
		BUILD_FOR_SFSE=$<BOOL:${BUILD_FOR_SFSE}>
)

#
# Dependencies
#
# tomlplusplus
find_package(tomlplusplus CONFIG REQUIRED)
target_link_libraries("${PROJECT_NAME}" PRIVATE tomlplusplus::tomlplusplus)

# Spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries("${PROJECT_NAME}" PRIVATE spdlog::spdlog)

# Detours
find_path(DETOURS_INCLUDE_DIRS "detours/detours.h")
find_library(DETOURS_LIBRARY detours REQUIRED)
target_include_directories("${PROJECT_NAME}" PRIVATE "${DETOURS_INCLUDE_DIRS}")
target_link_libraries("${PROJECT_NAME}" PRIVATE "${DETOURS_LIBRARY}")

# Xbyak
find_package(xbyak CONFIG REQUIRED)
target_link_libraries("${PROJECT_NAME}" PRIVATE xbyak::xbyak)

# SFSE
if(BUILD_FOR_SFSE)
	target_link_libraries("${PROJECT_NAME}" PRIVATE sfse::sfse_common)
endif()

#
# Commands
#
if(NOT BUILD_COPY_TO_OUTPUT_PATH STREQUAL "")
	add_custom_command(
		TARGET "${PROJECT_NAME}"
		POST_BUILD
		COMMAND
			"${CMAKE_COMMAND}" -E copy_if_different
			"$<TARGET_FILE:${PROJECT_NAME}>"
			"${BUILD_COPY_TO_OUTPUT_PATH}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
	)
endif()
