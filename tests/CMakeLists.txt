# Setup catch2, it's the testing framework
include(ExternalProject)
find_package(Wget REQUIRED)

ExternalProject_Add(
        catch2-external-project
        PREFIX ${CMAKE_BINARY_DIR}/catch2
        DOWNLOAD_DIR catch2
        DOWNLOAD_COMMAND ${WGET_EXECUTABLE} https://github.com/catchorg/Catch2/releases/download/v2.1.2/catch.hpp
        TIMEOUT 10
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
)

ExternalProject_Get_Property(catch2-external-project download_dir)

add_library(catch2 INTERFACE)
target_include_directories(catch2 INTERFACE ${download_dir}/..)
add_library(catch2::catch ALIAS catch2)

# Build tests
file(GLOB SRC_CACHEGRAND_TESTS "test-*.cpp")
add_executable(cachegrand-tests main.cpp ${SRC_CACHEGRAND_TESTS})
add_dependencies(cachegrand-tests cachegrand-internal catch2-external-project catch2)
set_target_properties(cachegrand-tests PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(cachegrand-tests catch2)
target_link_libraries(cachegrand-tests cachegrand-internal t1ha2)
target_include_directories(cachegrand-tests PUBLIC ".")
target_include_directories(cachegrand-tests PUBLIC "../src")
target_include_directories(cachegrand-tests PUBLIC "${download_dir}")
add_test(NAME cachegrand-tests COMMAND cachegrand-tests)

# Build benches
file(GLOB SRC_CACHEGRAND_BENCHES "bench-*.cpp")
add_executable(cachegrand-benches main.cpp ${SRC_CACHEGRAND_BENCHES})
add_dependencies(cachegrand-benches cachegrand-internal catch2-external-project catch2)
set_target_properties(cachegrand-benches PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(cachegrand-benches catch2)
target_link_libraries(cachegrand-benches cachegrand-internal t1ha2)
target_include_directories(cachegrand-benches PUBLIC ".")
target_include_directories(cachegrand-benches PUBLIC "../src")
target_include_directories(cachegrand-benches PUBLIC "${download_dir}")
add_test(NAME cachegrand-benches COMMAND cachegrand-benches)
