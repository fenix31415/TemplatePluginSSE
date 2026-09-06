vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO libxse/commonlib-shared
    REF main
    SHA512 eff0a3a72e847808ea3931e6b6f5c3e523f89b1a426da39e50d2f55b832aedeac5777b703b386a53b8ffb329aae109d8808a17a050adbcf4d5eab0ffe5847efe
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCMAKE_INSTALL_PREFIX=${CURRENT_PACKAGES_DIR}
)

vcpkg_cmake_install()

# ------------------------------------------------------------------
# Copy the debug library from the build tree
# ------------------------------------------------------------------
set(DEBUG_LIB_PATH "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/commonlib-shared.lib")
if(EXISTS "${DEBUG_LIB_PATH}")
    message(STATUS "Copying debug library from ${DEBUG_LIB_PATH}")
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/lib")
    file(COPY "${DEBUG_LIB_PATH}" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/")
else()
    message(WARNING "Debug library not found at ${DEBUG_LIB_PATH}; copying release library as fallback")
    file(COPY "${CURRENT_PACKAGES_DIR}/lib/commonlib-shared.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/")
endif()

# ------------------------------------------------------------------
# Ensure CMake config files exist in debug/lib/cmake (fixup expects them)
# ------------------------------------------------------------------
if(NOT EXISTS "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/commonlib-shared")
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/commonlib-shared")
    file(COPY "${CURRENT_PACKAGES_DIR}/lib/cmake/commonlib-shared/"
         DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/commonlib-shared")
endif()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/commonlib-shared)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib/cmake")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/lib/cmake")
