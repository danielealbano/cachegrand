# Setup catch2 lib, it's the testing framework
add_library(catch2 INTERFACE)
target_include_directories(catch2 INTERFACE ${CMAKE_BINARY_DIR}/3rdparty/catch2)
add_library(catch2::catch ALIAS catch2)

# Build tests
file(GLOB SRC_CACHEGRAND_TESTS "test-*.cpp" "*.c")
add_executable(cachegrand-tests main.cpp ${SRC_CACHEGRAND_TESTS})
add_dependencies(cachegrand-tests cachegrand-internal catch2)
set_target_properties(cachegrand-tests PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(cachegrand-tests PRIVATE cachegrand-internal t1ha catch2 pthread OpenSSL::SSL)
target_include_directories(cachegrand-tests PUBLIC ".")
target_include_directories(cachegrand-tests PUBLIC "../src")
target_include_directories(cachegrand-tests PUBLIC ${OPENSSL_INCLUDE_DIR})
target_compile_options(cachegrand-tests PRIVATE -mclflushopt -fpermissive)
add_test(NAME cachegrand-tests COMMAND cachegrand-tests)
