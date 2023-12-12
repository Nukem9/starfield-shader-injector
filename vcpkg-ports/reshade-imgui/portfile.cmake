vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ocornut/imgui
    REF v1.89.7-docking
    SHA512 d5f4433da365961916267e80a82234e439549a997578b684bbcf8970cdae7ab1f284da22ef1469f419e091c30578daadbd437bfa82a031fc6b2cee2e7a048418
    HEAD_REF docking
)

file(INSTALL ${SOURCE_PATH}/imgui.h DESTINATION ${CURRENT_PACKAGES_DIR}/include/${PORT})
file(INSTALL ${SOURCE_PATH}/imconfig.h DESTINATION ${CURRENT_PACKAGES_DIR}/include/${PORT})

# Handle copyright
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")