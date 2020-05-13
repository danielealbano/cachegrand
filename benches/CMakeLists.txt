find_package(benchmark REQUIRED)

# Build benches
file(GLOB SRC_CACHEGRAND_BENCHES "bench-*.cpp")
add_executable(cachegrand-benches main.cpp ${SRC_CACHEGRAND_BENCHES})
add_dependencies(cachegrand-benches cachegrand-internal)
set_target_properties(cachegrand-benches PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(cachegrand-benches benchmark::benchmark cachegrand-internal t1ha2 pthread)
target_include_directories(cachegrand-benches PUBLIC ".")
target_include_directories(cachegrand-benches PUBLIC "../src")
target_include_directories(cachegrand-benches PUBLIC "${download_dir}")
add_test(NAME cachegrand-benches COMMAND cachegrand-benches)
