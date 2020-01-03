cmake_minimum_required(VERSION 3.14)
project(cachegrand)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(3rdparty)
add_subdirectory(src)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
