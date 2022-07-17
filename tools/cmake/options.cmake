# Copyright (C) 2018-2022 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

option(BUILD_TESTS "Build Tests" 0)
option(BUILD_INTERNAL_BENCHES "Build Internal Benches" 0)
option(SLAB_ALLOCATOR_DEBUG_ALLOCS_FREES "Enable the slab allocator allocs/frees debugging" 0)

option(USE_HASH_ALGORITHM_XXH3 "Use xxh3 (xxHash) as hash algorithm for the hashtable" 0)
option(USE_HASH_ALGORITHM_T1HA2 "Use t1ha2 as hash algorithm for the hashtable" 0)
option(USE_HASH_ALGORITHM_CRC32C "Use crc32c as hash algorithm for the hashtable" 0)

# Architecture specific options
if (CACHEGRAND_ARCH_TARGET EQUAL "x86_64")
    option(ENABLE_SUPPORT_AVX512F "Enable the AVX512F instruction set support, will be used at runtime if available" 0)
endif()

# These options are available as define via the cmake_config.h.in, they can only be 0 and 1
if (NOT USE_HASH_ALGORITHM_XXH3)
    set(USE_HASH_ALGORITHM_XXH3 0)
else()
    set(USE_HASH_ALGORITHM_XXH3 1)
endif()

if (NOT USE_HASH_ALGORITHM_T1HA2)
    set(USE_HASH_ALGORITHM_T1HA2 0)
else()
    set(USE_HASH_ALGORITHM_T1HA2 1)
endif()

if (NOT USE_HASH_ALGORITHM_CRC32C)
    set(USE_HASH_ALGORITHM_CRC32C 0)
else()
    set(USE_HASH_ALGORITHM_CRC32C 1)
endif()

if (NOT ENABLE_SUPPORT_AVX512F)
    set(ENABLE_SUPPORT_AVX512F 0)
else()
    set(ENABLE_SUPPORT_AVX512F 1)
endif()
