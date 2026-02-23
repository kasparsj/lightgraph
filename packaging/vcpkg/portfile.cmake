vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DLIGHTPATH_CORE_BUILD_TESTS=OFF
        -DLIGHTPATH_CORE_BUILD_EXAMPLES=OFF
        -DLIGHTPATH_CORE_BUILD_BENCHMARKS=OFF
        -DLIGHTPATH_CORE_BUILD_DOCS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/lightpath" PACKAGE_NAME "lightpath")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
