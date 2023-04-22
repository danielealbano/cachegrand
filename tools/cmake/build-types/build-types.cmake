# Copyright (C) 2018-2023 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

include(CheckPIESupported)
check_pie_supported()

add_compile_options($<$<COMPILE_LANGUAGE:C>:-ggdb3>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-ggdb3>)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_VERBOSE_MAKEFILE ON)
    add_definitions(-DDEBUG=1)
    add_definitions(-DFFMA_TRACK_ALLOCS_FREES=1)

    add_compile_options($<$<COMPILE_LANGUAGE:C>:-O0>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-O0>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-fno-inline>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-inline>)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-fno-omit-frame-pointer>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-omit-frame-pointer>)

    # gcov linking required by gcc
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        link_libraries(gcov)
    endif()

    message(STATUS "Debug build")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions(-DNDEBUG=1)

    add_compile_options($<$<COMPILE_LANGUAGE:C>:-O2>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-O2>)

    message(STATUS "Release build")
endif()
