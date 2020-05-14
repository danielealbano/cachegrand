# Setup catch2 lib, it's the testing framework
add_library(catch2 INTERFACE)
target_include_directories(catch2 INTERFACE ${CMAKE_BINARY_DIR}/3rdparty/catch2)
add_library(catch2::catch ALIAS catch2)

# Build tests
file(GLOB SRC_CACHEGRAND_TESTS "test-*.cpp")
add_executable(cachegrand-tests main.cpp ${SRC_CACHEGRAND_TESTS})
add_dependencies(cachegrand-tests cachegrand-internal catch2)
set_target_properties(cachegrand-tests PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(cachegrand-tests catch2)
target_link_libraries(cachegrand-tests cachegrand-internal t1ha2)
target_include_directories(cachegrand-tests PUBLIC ".")
target_include_directories(cachegrand-tests PUBLIC "../src")
target_compile_options(cachegrand-tests PRIVATE -fpermissive)
add_test(NAME cachegrand-tests COMMAND cachegrand-tests)
