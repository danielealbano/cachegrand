FIND_PATH(LIBYAML_INCLUDE_DIR NAMES yaml.h)
FIND_LIBRARY(LIBYAML_LIBRARIES NAMES yaml libyaml)

message(STATUS "Performing Test HOST_HAS_LIBYAML")

set(libyaml_check_compiles_source "
#include <yaml.h>
int main() {
    int major, minor, patch;
    yaml_get_version(&major, &minor, &patch);
    printf(\"%d.%d.%d\", major, minor, patch);
    return 0;
}")

# We want to catch the version used to build cachegrand, need to use directly try_run
file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/libyaml_check_compiles_source.c"
        "${libyaml_check_compiles_source}\n")

try_run(
    HOST_HAS_LIBYAML_RUN_EXITCODE
    HOST_HAS_LIBYAML_COMPILE_RESULT
    ${CMAKE_CURRENT_BINARY_DIR}/
    "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/libyaml_check_compiles_source.c"
    LINK_LIBRARIES "-lyaml"
    COMPILE_OUTPUT_VARIABLE HOST_HAS_LIBYAML_COMPILE_OUTPUT
    RUN_OUTPUT_VARIABLE HOST_HAS_LIBYAML_RUN_OUTPUT)

# Did compilation succeed and process return 0 (success)?
IF("${HOST_HAS_LIBYAML_COMPILE_RESULT}" AND ("${HOST_HAS_LIBYAML_RUN_EXITCODE}" EQUAL 0))
    set(HOST_HAS_LIBYAML 1)
    string(STRIP "${HOST_HAS_LIBYAML_RUN_OUTPUT}" HOST_HAS_LIBYAML_VERSION)
    message(STATUS "Performing Test HOST_HAS_LIBYAML - Success")
ELSE()
    message(STATUS "Performing Test HOST_HAS_LIBYAML - Failure")
    message(FATAL_ERROR "Can't find libyaml")
ENDIF()

find_library(LIBYAML_LIBRARY_DIRS NAMES yaml NAMES_PER_DIR)
find_path(LIBYAML_INCLUDE_DIRS yaml.h)
set(LIBYAML_LIBRARIES "yaml")

list(APPEND DEPS_LIST_LIBRARIES "${LIBYAML_LIBRARIES}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBYAML_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBYAML_LIBRARY_DIRS}")
