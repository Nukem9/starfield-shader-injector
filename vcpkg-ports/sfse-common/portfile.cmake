vcpkg_minimum_required(VERSION 2022-10-12)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ianpatt/sfse
    REF v0.2.0
    SHA512 0a7287f7a09b48e08cb5da892b80b5c7613d6320354236de9ab45319ca2660bec10797fc0e130354a5013d51e992b9601ae459ff42c230c3399c4390b84944d2
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/sfse_common"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    CONFIG_PATH "lib/cmake/sfse_common"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# Vcpkg doesn't allow underscores. find_package() has to match.
file(RENAME "${CURRENT_PACKAGES_DIR}/share/${PORT}/sfse_common-config.cmake" "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}-config.cmake")

# Manually copy PluginAPI.h over since it should be part of "sfse_common". Alternatively sfse_version.h could be moved
# to the "sfse" project.
file(INSTALL "${SOURCE_PATH}/sfse/PluginAPI.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include/sfse_common")

# Handle copyright
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# Handle usage
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")