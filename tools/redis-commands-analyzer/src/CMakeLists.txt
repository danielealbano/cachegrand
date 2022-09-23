file(GLOB_RECURSE SRC_FILES_ANALYZER "*.c")

#################################################################
include(FindPkgConfig)

pkg_check_modules(LIBPCRE2 REQUIRED libpcre2-8)
#################################################################

##################################################
# Build the executable
##################################################
add_executable(
        redis-commands-analyzer
        analyzer.c
        ${SRC_FILES_ANALYZER})

set_target_properties(
        redis-commands-analyzer
        PROPERTIES
        LINKER_LANGUAGE C)

target_link_libraries(
        redis-commands-analyzer
        PUBLIC
        ${LIBPCRE2_LIBRARIES} -lpcre2-8)

target_link_directories(
        redis-commands-analyzer
        PUBLIC
        ${LIBPCRE2_LIBRARY_DIRS})

target_include_directories(
        redis-commands-analyzer
        PUBLIC
        ${LIBPCRE2_INCLUDE_DIRS})
