set(CACHEGRAND_CMAKE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CACHEGRAND_CMAKE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(CACHEGRAND_CMAKE_CONFIG_C_SRC ${CMAKE_CURRENT_BINARY_DIR}/cmake_config.c)

include(cmake_config.c.buildstep)

add_custom_target(__internal_refresh_cmake_config_c
    COMMAND
        cmake
            -DCACHEGRAND_ARCH_TARGET=${CACHEGRAND_ARCH_TARGET}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCACHEGRAND_CMAKE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            -DCACHEGRAND_CMAKE_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/cmake_config.c.buildstep.cmake" >/dev/null
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
