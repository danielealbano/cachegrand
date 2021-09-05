add_compile_options($<$<COMPILE_LANGUAGE:C>:-ggdb3>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-ggdb3>)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_VERBOSE_MAKEFILE ON)
    add_definitions(-DDEBUG=1)

    set(COMPILER_OPTIONS
            "-O0"; "-fno-inline"; "-fno-omit-frame-pointer";
            "-fsanitize=address"; "-fsanitize=pointer-compare"; "-fsanitize=pointer-subtract"; "-fsanitize=leak";
            "-fno-omit-frame-pointer"; "-fsanitize=undefined"; "-fsanitize=bounds-strict";
            "-fsanitize=float-divide-by-zero"; "-fsanitize=float-cast-overflow"
            )
    foreach(OPTION IN LISTS COMPILER_OPTIONS)
        add_compile_options($<$<COMPILE_LANGUAGE:C>:${OPTION}>)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${OPTION}>)
    endforeach()

    # gcov linking required by gcc
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        link_libraries(gcov)
    endif()

    message(STATUS "Debug build")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions(-DNDEBUG=1)

    add_compile_options($<$<COMPILE_LANGUAGE:C>:-O3>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-O3>)

    message(STATUS "Release build")
endif()
