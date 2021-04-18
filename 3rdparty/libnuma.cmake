include(CheckIncludeFiles)
include(CheckCSourceCompiles)

# Can't use pkg-config for libnuma, not all the distros provide numa.pc as part of the numa package

set(OLD_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
list(APPEND CMAKE_REQUIRED_LIBRARIES "-lnuma")

check_c_source_compiles("
#include <numa.h>
int main() {
    int res = numa_available();
    return 0;
}"
    HOST_HAS_LIBNUMA)
set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})

if (NOT HOST_HAS_LIBNUMA)
    message(FATAL_ERROR "Can't find libnuma")
endif()

find_library(LIBNUMA_LIBRARY_DIRS NAMES numa NAMES_PER_DIR)
find_path(LIBNUMA_INCLUDE_DIRS numa.h)
set(LIBNUMA_LIBRARIES "numa")

list(APPEND DEPS_LIST_LIBRARIES "${LIBNUMA_LIBRARIES}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBNUMA_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBNUMA_LIBRARY_DIRS}")
