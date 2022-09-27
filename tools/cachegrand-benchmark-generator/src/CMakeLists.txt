file(GLOB_RECURSE SRC_FILES_ANALYZER "*.c")

#################################################################
include(FindPkgConfig)

pkg_check_modules(LIBPCRE2 REQUIRED libpcre2-8)
pkg_check_modules(LIBJSONC REQUIRED json-c) # apt install libjson-c-dev

#################################################################

##################################################
# Build the executable
##################################################
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
        ${LIBPCRE2_LIBRARY_DIRS} ${LIBJSONC_LIBRARY_DIRS})

target_include_directories(
        cachegrand-benchmark-generator
        PUBLIC
        ${LIBPCRE2_INCLUDE_DIRS} ${LIBJSONC_INCLUDE_DIRS})
