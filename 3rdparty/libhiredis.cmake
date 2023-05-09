include(ProcessorCount)
include(ExternalProject)

set(LIBHIREDIS_BUILD_PATH_REL "build/release")
set(LIBHIREDIS_SRC_PATH "${CMAKE_BINARY_DIR}/_deps/src/hiredis")
set(LIBHIREDIS_BUILD_PATH "${LIBHIREDIS_SRC_PATH}")
set(LIBHIREDIS_INCLUDE_PATH "${LIBHIREDIS_SRC_PATH}")

# Notes:
# - do not use the latest tagged version (currently v1.1.0) as it contains some bugs and the errors in the yaml
#   documents are not being reported correctly.
# - because it's not a release version, enforce VERSION_DEVEL to 0
# - build the dynamic version of the library for the tests (check tests/CMakeLists.txt for more details)
ProcessorCount(BUILD_CPU_CORES)
ExternalProject_Add(
        hiredis
        GIT_REPOSITORY    https://github.com/redis/hiredis.git
        GIT_TAG           c14775b4e48334e0262c9f168887578f4a368b5d # tag v1.1.0
        PREFIX ${CMAKE_BINARY_DIR}/_deps
        CONFIGURE_COMMAND ""
        BUILD_BYPRODUCTS ${LIBHIREDIS_BUILD_PATH}/libhiredis.a
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
        cd ${LIBHIREDIS_BUILD_PATH} && make -j ${BUILD_CPU_CORES} libhiredis.a
        INSTALL_COMMAND "")

find_library(LIBHIREDIS_LIBRARY_DIRS NAMES hiredis NAMES_PER_DIR)
find_path(LIBHIREDIS_INCLUDE_DIRS hiredis.h)

set(LIBHIREDIS_LIBRARY_DIRS "${LIBHIREDIS_BUILD_PATH}")
set(LIBHIREDIS_INCLUDE_DIRS "${LIBHIREDIS_INCLUDE_PATH}")
set(LIBHIREDIS_LIBRARIES_STATIC "libhiredis.a")

list(APPEND DEPS_LIST_LIBRARIES_PRIVATE "${LIBHIREDIS_LIBRARIES_STATIC}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBHIREDIS_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBHIREDIS_LIBRARY_DIRS}")
