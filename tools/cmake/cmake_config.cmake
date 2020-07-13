set(CACHEGRAND_CMAKE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CACHEGRAND_CMAKE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
string(LENGTH "${CMAKE_SOURCE_DIR}/" CMAKE_SOURCE_DIR_LENGTH)

# Generate the cmake_config.c
include(cmake_config.buildstep)
add_custom_target(__internal_refresh_cmake_config
    COMMAND
        cmake
            -DCACHEGRAND_ARCH_TARGET=${CACHEGRAND_ARCH_TARGET}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCACHEGRAND_CMAKE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            -DCACHEGRAND_CMAKE_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/cmake_config.buildstep.cmake" >/dev/null
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

# Generate the cmake_config.h
message(STATUS "Updating cmake_config.h")
configure_file(
        "${CACHEGRAND_CMAKE_SOURCE_DIR}/src/cmake_config.h.in"
        "${CACHEGRAND_CMAKE_BINARY_DIR}/cmake_config/cmake_config.h"
        @ONLY)
message(STATUS "Updating cmake_config.h -- done")

# Automatically include cmake_config.c and cmake_config.h in the build
set(CACHEGRAND_CMAKE_CONFIG_C_SRC ${CMAKE_CURRENT_BINARY_DIR}/cmake_config/cmake_config.c)

# Add to the list of include directories for the dependencies the one containing cmake_config.h
list(APPEND DEPS_LIST_INCLUDE_DIRS "${CACHEGRAND_CMAKE_BINARY_DIR}/cmake_config/")
