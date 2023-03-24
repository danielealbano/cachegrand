include(CheckIncludeFiles)
include(CheckCSourceCompiles)

# Can't use pkg-config for libhiredis, not all the distros provide hiredis.pc as part of the hiredis package

set(OLD_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
set(CMAKE_REQUIRED_LIBRARIES "-lhiredis")

check_c_source_compiles("
#include <stdlib.h>
#include <hiredis/hiredis.h>
int main() {
    char *command;
    int len = redisFormatCommand(&command, \"SET hello world\");
    free(command);
    return 0;
}"
    HOST_HAS_LIBHIREDIS)
set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})
set(CMAKE_REQUIRED_INCLUDES ${OLD_CMAKE_REQUIRED_INCLUDES})

if (NOT HOST_HAS_LIBHIREDIS)
    message(FATAL_ERROR "Can't find libhiredis")
endif()

find_library(LIBHIREDIS_LIBRARY_DIRS NAMES hiredis NAMES_PER_DIR)
find_path(LIBHIREDIS_INCLUDE_DIRS hiredis/hiredis.h)
set(LIBHIREDIS_LIBRARIES "hiredis")

list(APPEND TESTSDEPS_LIST_LIBRARIES "${LIBHIREDIS_LIBRARIES}")
list(APPEND TESTSDEPS_LIST_INCLUDE_DIRS "${LIBHIREDIS_INCLUDE_DIRS}")
list(APPEND TESTSDEPS_LIST_LIBRARY_DIRS "${LIBHIREDIS_LIBRARY_DIRS}")
