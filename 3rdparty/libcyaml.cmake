include(ExternalProject)

set(LIBCYAML_BUILD_PATH_REL "build/release")
set(LIBCYAML_SRC_PATH "${CMAKE_BINARY_DIR}/_deps/src/cyaml")
set(LIBCYAML_BUILD_PATH "${LIBCYAML_SRC_PATH}/${LIBCYAML_BUILD_PATH_REL}")
set(LIBCYAML_INCLUDE_PATH "${LIBCYAML_SRC_PATH}/include")

# Notes:
# - do not use the latest tagged version (currently v1.1.0) as it contains some bugs and the errors in the yaml
#   documents are not being reported correctly.
# - because it's not a release version, enforce VERSION_DEVEL to 0
# - build the dynamic version of the library for the tests (check tests/CMakeLists.txt for more details)
ExternalProject_Add(
        cyaml
        GIT_REPOSITORY    https://github.com/tlsa/libcyaml.git
        GIT_TAG           227bbe04581541d8e97b7306a81c05fa2894841b # tag v1.2.0
        PREFIX ${CMAKE_BINARY_DIR}/_deps
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
        sed -e "s/VERSION_DEVEL = 1/VERSION_DEVEL = 0/" -i ${LIBCYAML_SRC_PATH}/Makefile &&
        CFLAGS="-fPIC" make -C ${LIBCYAML_SRC_PATH} ${LIBCYAML_BUILD_PATH_REL}/libcyaml.a ${LIBCYAML_BUILD_PATH_REL}/libcyaml.so.1
        INSTALL_COMMAND "")

find_library(LIBCYAML_LIBRARY_DIRS NAMES yaml NAMES_PER_DIR)
find_path(LIBCYAML_INCLUDE_DIRS yaml.h)

set(LIBCYAML_LIBRARY_DIRS "${LIBCYAML_BUILD_PATH}")
set(LIBCYAML_INCLUDE_DIRS "${LIBCYAML_INCLUDE_PATH}")
set(LIBCYAML_LIBRARIES_STATIC "libcyaml.a")
set(LIBCYAML_LIBRARIES_DYNAMIC "libcyaml.so.1")

list(APPEND DEPS_LIST_LIBRARIES_PRIVATE "${LIBCYAML_LIBRARIES_STATIC}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBCYAML_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBCYAML_LIBRARY_DIRS}")