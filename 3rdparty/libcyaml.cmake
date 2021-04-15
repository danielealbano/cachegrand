include(ExternalProject)

set(LIBCYAML_BUILD_PATH_REL "build/release")
set(LIBCYAML_SRC_PATH "${CMAKE_BINARY_DIR}/_deps/src/cyaml")
set(LIBCYAML_BUILD_PATH "${LIBCYAML_SRC_PATH}/${LIBCYAML_BUILD_PATH_REL}")
set(LIBCYAML_INCLUDE_PATH "${LIBCYAML_SRC_PATH}/include")

# Need to set VERSION_DEVEL to something different from zero to force a debug build
ExternalProject_Add(
        cyaml
        GIT_REPOSITORY    https://github.com/tlsa/libcyaml.git
        GIT_TAG           v1.1.0
        PREFIX ${CMAKE_BINARY_DIR}/_deps
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make -C ${LIBCYAML_SRC_PATH} ${LIBCYAML_BUILD_PATH_REL}/libcyaml.a
        INSTALL_COMMAND "")

find_library(LIBCYAML_LIBRARY_DIRS NAMES yaml NAMES_PER_DIR)
find_path(LIBCYAML_INCLUDE_DIRS yaml.h)

set(LIBCYAML_LIBRARY_DIRS "${LIBCYAML_BUILD_PATH}")
set(LIBCYAML_INCLUDE_DIRS "${LIBCYAML_INCLUDE_PATH}")
set(LIBCYAML_LIBRARIES "libcyaml.a")

list(APPEND DEPS_LIST_LIBRARIES "${LIBCYAML_LIBRARIES}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBCYAML_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBCYAML_LIBRARY_DIRS}")
