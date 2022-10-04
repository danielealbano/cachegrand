# Copyright (C) 2018-2022 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

cmake_minimum_required(VERSION 3.16)
project(cachegrand LANGUAGES C CXX ASM)

include(tools/cmake/main.cmake)

add_subdirectory(3rdparty)
add_subdirectory(tools)
add_subdirectory(src)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (BUILD_INTERNAL_BENCHES)
    add_subdirectory(benches)
endif()
