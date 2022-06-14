option(BUILD_TESTS "Build Tests" OFF)
option(BUILD_INTERNAL_BENCHES "Build Internal Benches" OFF)

option(USE_HASH_ALGORITHM_XXH3 "Use xxh3 (xxHash) as hash algorithm for the hashtable" 0)
option(USE_HASH_ALGORITHM_T1HA2 "Use T1HA2 as hash algorithm for the hashtable" 0)
option(USE_HASH_ALGORITHM_CRC32C "Use CRC32C as hash algorithm for the hashtable" 0)
# Architecture specific options
if (CACHEGRAND_ARCH_TARGET EQUAL "x86_64")
    option(ENABLE_SUPPORT_AVX512F "Enable the AVX512F instruction set support, will be used at runtime if available" 0)
endif()
