include(CheckCSourceCompiles)

list(APPEND CMAKE_REQUIRED_LIBRARIES "-llzma")
check_c_source_compiles("
#include <lzma.h>
int main() {
    int version = lzma_version_number();
    return 0;
}"
        HOST_HAS_LIBLZMA)
set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})

if (NOT HOST_HAS_LIBLZMA)
    message(FATAL_ERROR "Can't find liblzma")
endif()

find_library(LIBLZMA_LIBRARY_DIRS NAMES lzma NAMES_PER_DIR)
find_path(LIBLZMA_INCLUDE_DIRS lzma.h)
set(LIBLZMA_LIBRARIES "lzma")

list(APPEND DEPS_LIST_LIBRARIES "${LIBLZMA_LIBRARIES}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBLZMA_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBLZMA_LIBRARY_DIRS}")
