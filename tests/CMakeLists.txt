# Include the FetchContent module to get Catch directly from the git repository
Include(FetchContent)

# Disable catch self-testing build
SET(CATCH_BUILD_TESTING OFF CACHE BOOL "CATCH_BUILD_TESTING")
SET(CATCH_INSTALL_DOCS OFF CACHE BOOL "CATCH_INSTALL_DOCS")

# Fetch Catch2
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v2.13.8)
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
        cachegrand-internal Catch2)
target_link_libraries(
        cachegrand-tests
        PRIVATE
        cachegrand-internal Catch2::Catch2 ${LIBCYAML_DYNAMIC_PATH_LIBRARY})
add_test(
        NAME
        cachegrand-tests
        COMMAND
        cachegrand-tests)
