vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO crosire/reshade
    REF v5.9.2
    SHA512 548be6b0bde6aadf988c332814b0ea72a3efcff6f47503fba2e49de94756639eafd1e76b6ca47373968f0beb1b31601422e428644ab88181785ba9ef1eac910e
    HEAD_REF master
)

file(INSTALL ${SOURCE_PATH}/include DESTINATION ${CURRENT_PACKAGES_DIR}/include RENAME reshade-api)

# Handle copyright
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.md")