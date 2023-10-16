include(ProcessorCount)
include(ExternalProject)

set(LIBUNWIND_SRC_PATH "${CMAKE_BINARY_DIR}/_deps/src/unwind")
set(LIBUNWIND_BUILD_PATH "${CMAKE_BINARY_DIR}/_deps/src/unwind-build")
set(LIBUNWIND_INCLUDE_PATH "${LIBUNWIND_SRC_PATH}/include")

ProcessorCount(BUILD_CPU_CORES)
ExternalProject_Add(
        unwind
        GIT_REPOSITORY https://github.com/libunwind/libunwind.git
        GIT_TAG v1.7.2
        PREFIX ${CMAKE_BINARY_DIR}/_deps
        BUILD_BYPRODUCTS ${LIBUNWIND_SRC_PATH}/.libs/libunwind.a
        CONFIGURE_COMMAND
        cd ${LIBUNWIND_SRC_PATH} && autoreconf -i && chmod +x ${LIBUNWIND_SRC_PATH}/configure && ${LIBUNWIND_SRC_PATH}/configure --prefix=${CMAKE_BINARY_DIR}/_deps/libunwind/install
        BUILD_COMMAND
        make -C ${LIBUNWIND_SRC_PATH} -j ${BUILD_CPU_CORES}
        INSTALL_COMMAND "")

set(LIBUNWIND_LIBRARY_DIRS "${LIBUNWIND_SRC_PATH}/.libs/")
set(LIBUNWIND_INCLUDE_DIRS "${LIBUNWIND_INCLUDE_PATH}")
set(LIBUNWIND_LIBRARIES_STATIC "libunwind.a")

list(APPEND DEPS_LIST_LIBRARIES_PRIVATE "${LIBUNWIND_LIBRARIES_STATIC}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBUNWIND_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBUNWIND_LIBRARY_DIRS}")
