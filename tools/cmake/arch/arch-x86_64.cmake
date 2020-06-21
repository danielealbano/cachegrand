include(CheckCXXCompilerFlag)

check_cxx_compiler_flag(-mavx2                              COMPILER_HAS_MAVX2_FLAG)
check_cxx_compiler_flag(-mavx                               COMPILER_HAS_MAVX_FLAG)
check_cxx_compiler_flag(-mno-avx256-split-unaligned-load    COMPILER_HAS_MNO_AVX256_SPLIT_UNALIGNED_LOAD_FLAG)
check_cxx_compiler_flag(-mbmi                               COMPILER_HAS_MBMI_FLAG)
check_cxx_compiler_flag(-mlzcnt                             COMPILER_HAS_MLZCNT_FLAG)
check_cxx_compiler_flag(-mclflushopt                        COMPILER_HAS_CLFLUSHOPT_FLAG)
