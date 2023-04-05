include(CheckCSourceCompiles)

list(APPEND CMAKE_REQUIRED_LIBRARIES "-llzf")
check_c_source_compiles("
#include <stdlib.h>
#include <liblzf/lzf.h>
int main() {
    size_t compressed_size;
    char string[] = \"Hello World!\";
    size_t string_len = sizeof(string);
    char *output[1024] = { 0 };
    size_t output_len = sizeof(output);

    compressed_size = lzf_compress(string, string_len, output, output_len);

    return 0;
}"
        HOST_HAS_LIBLZF)
set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})

if (NOT HOST_HAS_LIBLZF)
    message(FATAL_ERROR "Can't find liblzf")
endif()

find_library(LIBLZF_LIBRARY_DIRS NAMES lzf NAMES_PER_DIR)
find_path(LIBLZF_INCLUDE_DIRS liblzf/lzf.h)
set(LIBLZF_LIBRARIES "lzf")

list(APPEND DEPS_LIST_LIBRARIES "${LIBLZF_LIBRARIES}")
list(APPEND DEPS_LIST_INCLUDE_DIRS "${LIBLZF_INCLUDE_DIRS}")
list(APPEND DEPS_LIST_LIBRARY_DIRS "${LIBLZF_LIBRARY_DIRS}")
