set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG=1)
else()
    add_definitions(-DNDEBUG=1)
endif()

if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling: yes")
else()
    message(STATUS "Cross-compiling: no")
endif()

include(compiler-ccache)
