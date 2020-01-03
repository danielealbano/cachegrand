# Build the tests
file(GLOB SRC_CACHEGRAND_TESTS "*.cpp")

add_executable(tests ${SRC_CACHEGRAND_TESTS})
add_dependencies(tests cachegrand-internal)
set_target_properties(tests PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(tests cachegrand-internal t1ha)
target_include_directories(tests PUBLIC ".")
target_include_directories(tests PUBLIC "../src")
#add_test(NAME "tests" COMMAND "tests")
