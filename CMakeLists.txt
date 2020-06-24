cmake_minimum_required(VERSION 3.13)
project(cachegrand LANGUAGES C CXX)

set(CMAKE_VERBOSE_MAKEFILE ON)

include(tools/cmake/config.cmake)

add_subdirectory(3rdparty)
add_subdirectory(src)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (BUILD_INTERNAL_BENCHES)
    add_subdirectory(benches)
endif()
