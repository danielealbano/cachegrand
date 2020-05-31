cmake_minimum_required(VERSION 3.13)
project(cachegrand LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DNDEBUG)
else()
    add_definitions(-DDEBUG)
endif()

message("-- Check if ccache is available")
find_program(CCACHE_FOUND_PATH ccache)
if(CCACHE_FOUND_PATH)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_FOUND_PATH})
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_FOUND_PATH})
    message("   Found, cache dir set to ($ENV{CCACHE_DIR})")
endif(CCACHE_FOUND_PATH)

add_subdirectory(3rdparty)
add_subdirectory(src)

if (CACHEGRAND_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (CACHEGRAND_BUILD_BENCHES)
    add_subdirectory(benches)
endif()
