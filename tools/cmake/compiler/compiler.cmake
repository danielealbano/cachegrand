# Copyright (C) 2018-2022 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options($<$<COMPILE_LANGUAGE:C>:-fstack-protector-strong>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstack-protector-strong>)

if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling: yes")
else()
    message(STATUS "Cross-compiling: no")
endif()

include(compiler-ccache)
