if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG=1)
    add_compile_options(-g -O0 -fno-inline)

    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        link_libraries(gcov)
    endif()

    message(STATUS "Release build")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions(-DNDEBUG=1)
    add_compile_options(-g -O3)

    message(STATUS "Release build")
elseif (CMAKE_BUILD_TYPE MATCHES ReleaseBench)
    add_definitions(-DNDEBUG=1)
    add_compile_options(-g -O3)

    message(STATUS "Release for Benches build (using CRC32 algorithm)")
endif()
