include(CheckCSourceCompiles)

list(APPEND CMAKE_REQUIRED_LIBRARIES "-lyaml")
check_c_source_compiles("
#include <yaml.h>
int main() {
    int major, minor, patch;
    yaml_get_version(&major, &minor, &patch);
    return 0;
}"
        HOST_HAS_LIBYAML)
set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})

if (NOT HOST_HAS_LIBYAML)
    message(FATAL_ERROR "Can't find libyaml")
endif()

find_library(LIBYAML_LIBRARY_DIRS NAMES yaml NAMES_PER_DIR)
find_path(LIBYAML_INCLUDE_DIRS yaml.h)
set(LIBYAML_LIBRARIES "yaml")

list(APPEND DEPS_LIST_LIBRARIES "${LIBYAML_LIBRARIES}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBYAML_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBYAML_LIBRARY_DIRS}")
