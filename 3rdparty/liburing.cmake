include(ProcessorCount)
include(ExternalProject)

# As cmake will run the build from a different folder, the configure needs have a specific folder already existing for
# it to work as expected so here it gets pre-created
set(LIBURING_SRC_PATH "${CMAKE_BINARY_DIR}/_deps/src/liburing")
set(LIBURING_BUILD_PATH "${CMAKE_BINARY_DIR}/_deps/src/liburing-build")
set(LIBURING_INCLUDE_PATH "${LIBURING_SRC_PATH}/src/include")
file(MAKE_DIRECTORY "${LIBURING_BUILD_PATH}/src/include/liburing")

# Setup the buid
ProcessorCount(BUILD_CPU_CORES)
ExternalProject_Add(
        liburing
        GIT_REPOSITORY https://github.com/axboe/liburing.git
        GIT_TAG 80272cbeb42bcd0b39a75685a50b0009b77cd380 # tag liburing-2.8
        PREFIX ${CMAKE_BINARY_DIR}/_deps
        BUILD_BYPRODUCTS ${LIBURING_SRC_PATH}/src/liburing.a
        CONFIGURE_COMMAND
        cd ${LIBURING_SRC_PATH} && chmod +x ${LIBURING_SRC_PATH}/configure && ${LIBURING_SRC_PATH}/configure --prefix=${CMAKE_BINARY_DIR}/_deps/liburing/install
        BUILD_COMMAND
        make -C ${LIBURING_SRC_PATH} -j ${BUILD_CPU_CORES}
        INSTALL_COMMAND "")

set(LIBURING_LIBRARY_DIRS "${LIBURING_SRC_PATH}/src")
set(LIBURING_INCLUDE_DIRS "${LIBURING_INCLUDE_PATH}")
set(LIBURING_LIBRARIES_STATIC "liburing.a")

list(APPEND DEPS_LIST_LIBRARIES_PRIVATE "${LIBURING_LIBRARIES_STATIC}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBURING_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBURING_LIBRARY_DIRS}")
