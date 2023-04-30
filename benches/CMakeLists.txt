# Copyright (C) 2018-2022 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

# Setup the google-benchmark lib, it's the bench framework
include(ExternalProject)
set (CMAKE_CXX_STANDARD 17)

if (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER_EQUAL 3.24)
    ExternalProject_Add(benchmark-install
            PREFIX benchmark-sources
            DOWNLOAD_EXTRACT_TIMESTAMP true
            INSTALL_DIR ${CMAKE_BINARY_DIR}/benchmark-install
            URL https://github.com/google/benchmark/archive/v1.6.1.tar.gz
            BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/benchmark-install/lib/libbenchmark.a
            CMAKE_CACHE_ARGS
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE:STRING=RELEASE
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
            -DCMAKE_CXX_FLAGS:STRING=${BENCHMARK_LIBCXX_COMPILE_FLAGS}
            -DBENCHMARK_ENABLE_TESTING:BOOL=OFF
            -DBENCHMARK_DOWNLOAD_DEPENDENCIES:BOOL=ON)
else()
    ExternalProject_Add(benchmark-install
            PREFIX benchmark-sources
            INSTALL_DIR ${CMAKE_BINARY_DIR}/benchmark-install
            URL https://github.com/google/benchmark/archive/v1.6.1.tar.gz
            BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/benchmark-install/lib/libbenchmark.a
            CMAKE_CACHE_ARGS
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE:STRING=RELEASE
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
            -DCMAKE_CXX_FLAGS:STRING=${BENCHMARK_LIBCXX_COMPILE_FLAGS}
            -DBENCHMARK_ENABLE_TESTING:BOOL=OFF
            -DBENCHMARK_DOWNLOAD_DEPENDENCIES:BOOL=ON)
endif()

add_library(benchmark::benchmark STATIC IMPORTED)
add_dependencies(benchmark::benchmark benchmark-install)
set_target_properties(benchmark::benchmark PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/benchmark-install/lib/libbenchmark.a"
        )
IF(WIN32)
    set_target_properties(benchmark::benchmark PROPERTIES
            INTERFACE_LINK_LIBRARIES "Shlwapi"
            )
ENDIF(WIN32)

set(LIBCYAML_DYNAMIC_PATH_LIBRARY "${CMAKE_BINARY_DIR}/_deps/src/cyaml/build/release/libcyaml.so.1")

# Build benches
file(GLOB SRC_CACHEGRAND_BENCHES "bench-*.cpp")

add_custom_target(bench-all)

foreach(SRC_CACHEGRAND_BENCH ${SRC_CACHEGRAND_BENCHES})
    get_filename_component(CACHEGRAND_BENCH_EXEC_NAME ${SRC_CACHEGRAND_BENCH} NAME_WE)

    add_executable(
            ${CACHEGRAND_BENCH_EXEC_NAME}
            ${SRC_CACHEGRAND_BENCH} ../tests/unit_tests/support.c)

    add_dependencies(
            ${CACHEGRAND_BENCH_EXEC_NAME}
            cachegrand-internal)

    target_compile_options(
            ${CACHEGRAND_BENCH_EXEC_NAME}
            PRIVATE
            -march=native -O3)

    set_target_properties(
            ${CACHEGRAND_BENCH_EXEC_NAME}
            PROPERTIES
            LINKER_LANGUAGE CXX)

    target_link_libraries(
            ${CACHEGRAND_BENCH_EXEC_NAME}
            PUBLIC
            cachegrand-internal benchmark::benchmark ${LIBCYAML_DYNAMIC_PATH_LIBRARY})

    target_include_directories(
            ${CACHEGRAND_BENCH_EXEC_NAME}
            PRIVATE
            ../tests ${CMAKE_BINARY_DIR}/benchmark-install/include)

    add_custom_command(
            TARGET ${CACHEGRAND_BENCH_EXEC_NAME}
            PRE_LINK
            COMMAND
            nm --format=just-symbols "$<TARGET_FILE:cachegrand-internal>" | grep _[cd]tors_fp | grep -v gcov | xargs -I{} echo -Wl,-u{} > "${CMAKE_CURRENT_BINARY_DIR}/linker_options.txt"
            VERBATIM)

    set_target_properties(
            ${CACHEGRAND_BENCH_EXEC_NAME}
            PROPERTIES
            LINK_FLAGS
            "@${CMAKE_CURRENT_BINARY_DIR}/linker_options.txt")

    add_dependencies(
            bench-all
            ${CACHEGRAND_BENCH_EXEC_NAME})
endforeach()
