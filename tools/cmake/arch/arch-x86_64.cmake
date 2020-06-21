include(CheckCCompilerFlag)
include(CheckCXXSourceCompiles)

check_c_compiler_flag(-mavx2                              COMPILER_HAS_MAVX2_FLAG)
check_c_compiler_flag(-mavx                               COMPILER_HAS_MAVX_FLAG)
check_c_compiler_flag(-mno-avx256-split-unaligned-load    COMPILER_HAS_MNO_AVX256_SPLIT_UNALIGNED_LOAD_FLAG)
check_c_compiler_flag(-mbmi                               COMPILER_HAS_MBMI_FLAG)
check_c_compiler_flag(-mlzcnt                             COMPILER_HAS_MLZCNT_FLAG)
check_c_compiler_flag(-mclflushopt                        COMPILER_HAS_CLFLUSHOPT_FLAG)

if (COMPILER_HAS_CLFLUSHOPT_FLAG)
    set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    list(APPEND CMAKE_REQUIRED_FLAGS "-mclflushopt -march=native")
    check_cxx_source_compiles("
#include <immintrin.h>
int main() {
    char temp[64] = {0};
    _mm_clflushopt(&temp);
    return 0;
}"
            HOST_HAS_MM_CLFLUSHOPT)
    set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})

    if (NOT HOST_HAS_MM_CLFLUSHOPT)
        set(HOST_HAS_MM_CLFLUSHOPT 0)
    endif()
endif()
