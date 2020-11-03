set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<COMPILE_LANGUAGE:C>:-fstack-protector-strong>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstack-protector-strong>)

if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling: yes")
else()
    message(STATUS "Cross-compiling: no")
endif()

include(compiler-ccache)
