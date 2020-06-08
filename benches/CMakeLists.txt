# Setup the google-benchmark lib, it's the bench framework
include(ExternalProject)
set (CMAKE_CXX_STANDARD 17)

ExternalProject_Add(benchmark-install
        PREFIX benchmark-sources
        INSTALL_DIR ${CMAKE_BINARY_DIR}/benchmark-install
        URL https://github.com/google/benchmark/archive/v1.5.0.tar.gz
        CMAKE_CACHE_ARGS
            -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE:STRING=RELEASE
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
            -DCMAKE_CXX_FLAGS:STRING=${BENCHMARK_LIBCXX_COMPILE_FLAGS}
            -DBENCHMARK_ENABLE_TESTING:BOOL=OFF
            -DBENCHMARK_DOWNLOAD_DEPENDENCIES:BOOL=ON)

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

# Build benches
file(GLOB SRC_CACHEGRAND_BENCHES "bench-*.cpp")
add_executable(cachegrand-benches main.cpp bench-support.c ${SRC_CACHEGRAND_BENCHES})
add_dependencies(cachegrand-benches cachegrand-internal __internal_refresh_cmake_config_c)
set_target_properties(cachegrand-benches PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(cachegrand-benches cachegrand-internal t1ha pthread benchmark::benchmark)
target_include_directories(cachegrand-benches PUBLIC ".")
target_include_directories(cachegrand-benches PUBLIC "../src")
target_include_directories(cachegrand-benches PUBLIC "${CMAKE_BINARY_DIR}/benchmark-install/include")
