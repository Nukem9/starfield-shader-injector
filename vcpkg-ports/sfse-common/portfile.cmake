vcpkg_minimum_required(VERSION 2022-10-12)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ianpatt/sfse
    REF v0.2.19
    SHA512 136005554bdfd38558190e3e2d59ab3f9b0b33c1ae52ae2df07e2a7ea97a03768fa33b7d5efbcf18d9851153ed0d2863c5b721b5112dada7f38e8c79ed38b506
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