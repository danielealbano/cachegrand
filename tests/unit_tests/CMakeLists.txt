# Copyright (C) 2018-2023 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

# Include the FetchContent module to get Catch directly from the git repository
Include(FetchContent)

# Disable catch self-testing build
SET(CATCH_BUILD_TESTING OFF CACHE BOOL "CATCH_BUILD_TESTING")
SET(CATCH_INSTALL_DOCS OFF CACHE BOOL "CATCH_INSTALL_DOCS")

# Fetch Catch2
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.3.2)
FetchContent_MakeAvailable(Catch2)

# Build tests
file(GLOB_RECURSE SRC_CACHEGRAND_TESTS "test-*.cpp" "*.c")
add_executable(cachegrand-tests main.cpp ${SRC_CACHEGRAND_TESTS})

target_compile_options(
        cachegrand-tests
        PRIVATE
        -fpermissive)
if(HOST_HAS_CLFLUSHOPT)
    target_compile_options(
            cachegrand-tests
            PRIVATE
            -mclflushopt)
endif()

if (HOST_HAS_AVX512F)
    target_compile_options(
            cachegrand-tests
            PRIVATE
            -mavx512f)
endif()

if (HOST_HAS_AVX2)
    target_compile_options(
            cachegrand-tests
            PRIVATE
            -mno-avx256-split-unaligned-load -mavx2 -mfma)
endif()

if (HOST_HAS_AVX)
    target_compile_options(
            cachegrand-tests
            PRIVATE
            -mavx -mbmi -mbmi2)
endif()

add_custom_command(
        TARGET cachegrand-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

add_custom_command(
        TARGET cachegrand-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/tests/unit_tests/fixtures/*
        ${CMAKE_CURRENT_BINARY_DIR}/fixtures/)

# The libcyaml library can't be statically linked for the tests, the C++ compiler will fail to link the libyaml library
# as it is compiled for C and the C++ is not properly configured.
# Linking it dynamically will avoid the issue, it has a number of side effects but for the tests it's fine (at least
# for now)
set(LIBCYAML_DYNAMIC_PATH_LIBRARY "${CMAKE_BINARY_DIR}/_deps/src/cyaml/build/release/libcyaml.so.1")

set_target_properties(
        cachegrand-tests
        PROPERTIES
        LINKER_LANGUAGE CXX)

add_dependencies(
        cachegrand-tests
        cachegrand-internal Catch2 hiredis)

target_link_libraries(
        cachegrand-tests
        PRIVATE
        cachegrand-internal Catch2::Catch2 ${LIBCYAML_DYNAMIC_PATH_LIBRARY} ${TESTSDEPS_LIST_LIBRARIES})

target_link_directories(
        cachegrand-tests
        PRIVATE
        ${TESTSDEPS_LIST_LIBRARY_DIRS})

target_include_directories(
        cachegrand-tests
        PRIVATE
        ${TESTSDEPS_LIST_LIBRARIES})

add_custom_command(
        TARGET cachegrand-tests
        PRE_LINK
        COMMAND
        nm --format=posix "$<TARGET_FILE:cachegrand-internal>" | grep _[cd]tors_fp | cut -d\  -f1 | xargs -I{} echo -Wl,-u{} > "${CMAKE_CURRENT_BINARY_DIR}/linker_options.txt"
        VERBATIM)

set_target_properties(
        cachegrand-tests
        PROPERTIES
        LINK_FLAGS
        "@${CMAKE_CURRENT_BINARY_DIR}/linker_options.txt")

add_test(
        NAME
        cachegrand-tests
        COMMAND
        cachegrand-tests)
