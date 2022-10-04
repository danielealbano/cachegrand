include(FindPkgConfig)

pkg_check_modules(LIBPCRE2 REQUIRED libpcre2-8)
pkg_check_modules(LIBJSONC REQUIRED json-c)

##################################################

file(GLOB_RECURSE CACHEGRAND_TESTS "../../../tests/unit_tests/modules/redis/command/*.cpp")
foreach(t ${CACHEGRAND_TESTS})
    set(ALL_TESTS "${ALL_TESTS}   \"${t}\",\n")
endforeach()

configure_file(all_tests.h.in all_tests.h)

##################################################
# Build the executable
##################################################
file(GLOB_RECURSE SRC_FILES_ANALYZER "*.c")

add_executable(
        cachegrand-benchmark-generator
        analyzer.c
        ${SRC_FILES_ANALYZER})

set_target_properties(
        cachegrand-benchmark-generator
        PROPERTIES
        LINKER_LANGUAGE C)

target_link_libraries(
        cachegrand-benchmark-generator
        PUBLIC
        ${LIBPCRE2_LIBRARIES} ${LIBJSONC_LIBRARIES})

target_link_directories(
        cachegrand-benchmark-generator
        PUBLIC
        ${LIBPCRE2_LIBRARY_DIRS} ${LIBJSONC_LIBRARY_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(
        cachegrand-benchmark-generator
        PUBLIC
        ${LIBPCRE2_INCLUDE_DIRS} ${LIBJSONC_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
