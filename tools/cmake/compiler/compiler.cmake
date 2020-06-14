set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG=1)
    add_compile_options(-g -O0 -fno-inline)

    message(STATUS "Debug build, defining DEBUG=1 and setting compile options to '-g -O0 -fno-inline'")
else()
    add_definitions(-DNDEBUG=1)
    add_compile_options(-O3 -march=x86-64)

    message(STATUS "Release build")
endif()
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling: yes")
else()
    message(STATUS "Cross-compiling: no")
endif()

string(LENGTH "${CMAKE_SOURCE_DIR}/" CACHEGRAND_CMAKE_CONFIG_SOURCE_PATH_SIZE)
add_definitions("-DCACHEGRAND_CMAKE_CONFIG_SOURCE_PATH_SIZE=${CACHEGRAND_CMAKE_CONFIG_SOURCE_PATH_SIZE}")

include(compiler-ccache)
