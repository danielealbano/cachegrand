# Setup catch2 lib, it's the testing framework
add_library(catch2 INTERFACE)
target_include_directories(catch2 INTERFACE ${CMAKE_BINARY_DIR}/3rdparty/catch2)
add_library(catch2::catch ALIAS catch2)

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

set_target_properties(
        cachegrand-tests
        PROPERTIES
        LINKER_LANGUAGE CXX)

add_dependencies(
        cachegrand-tests
        cachegrand-internal catch2)

target_link_libraries(
        cachegrand-tests
        PUBLIC
        cachegrand-internal)

add_test(
        NAME
        cachegrand-tests
        COMMAND
        cachegrand-tests)
