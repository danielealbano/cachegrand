/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "misc.h"
#include "log/log.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_data.h"
#include "data_structures/hashtable/mcmp/hashtable_support_op.h"
#include "data_structures/hashtable/mcmp/hashtable_support_hash_search.h"

#if defined(__x86_64__)
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX512F == 1
#include "data_structures/hashtable/mcmp/hashtable_support_hash_search_avx512f.h"
#endif
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX2 == 1
#include "data_structures/hashtable/mcmp/hashtable_support_hash_search_avx2.h"
#endif
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX == 1
#include "data_structures/hashtable/mcmp/hashtable_support_hash_search_avx.h"
#endif
#endif

#include "data_structures/hashtable/mcmp/hashtable_support_hash_search_loop.h"

#include "fixtures-hashtable-mpmc.h"

#define HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_FUNC CONCAT(CONCAT(hashtable_mcmp_support_hash_search, CACHEGRAND_HASHTABLE_MCMP_SUPPORT_OP_ARCH_SUFFIX), 14)

#define TEST_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(SUFFIX) \
    SECTION("hashtable_mcmp_support_hash_search" STRINGIZE(SUFFIX)) { \
        SECTION("hash - found") { \
            hashtable_hash_half_volatile_t hashes[HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT] = { \
                    123, 234, 345, 456, 567, 678, 789, 890, 901, 12, 987, 876, 765, 654 \
            }; \
            hashtable_hash_half_t hash = 345; \
        \
            REQUIRE(__builtin_ctz(hashtable_mcmp_support_hash_search##SUFFIX(hash, hashes)) == 2); \
        } \
        \
        SECTION("hash - not found") { \
            hashtable_hash_half_volatile_t hashes[HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT] = { \
                    123, 234, 345, 456, 567, 678, 789, 890, 901, 12, 987, 876, 765, 654 \
            }; \
            hashtable_hash_half_t hash = 999999; \
        \
            REQUIRE(hashtable_mcmp_support_hash_search##SUFFIX(hash, hashes) == 0); \
        } \
        \
        SECTION("hash - multiple - find first") { \
            hashtable_hash_half_volatile_t hashes[HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT] = { \
                    123, 234, 345, 456, 567, 234, 789, 890, 901, 234, 987, 876, 765, 654 \
            }; \
            hashtable_hash_half_t hash = 234; \
        \
            REQUIRE(__builtin_ctz(hashtable_mcmp_support_hash_search##SUFFIX(hash, hashes)) == 1); \
        } \
    }

TEST_CASE("hashtable/hashtable_mcmp_support_hash_search.c",
        "[hashtable][hashtable_support][hashtable_support_hash][hashtable_mcmp_support_hash_search]") {
#if defined(__x86_64__)
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX512F == 1
#if CACHEGRAND_CMAKE_CONFIG_ENABLE_SUPPORT_AVX512F == 1
    TEST_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(_avx512f_14)
#endif
#endif
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX2 == 1
    TEST_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(_avx2_14)
#endif
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_AVX == 1
    TEST_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(_avx_14)
#endif
#endif
    TEST_HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(_loop_14)
}
