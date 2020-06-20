#include "catch.hpp"

#include "exttypes.h"
#include "spinlock.h"
#include "log.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support_op.h"
#include "hashtable/hashtable_support_hash_search.h"

#include "fixtures-hashtable.h"

#define TEST_HASHTABLE_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(INSTRUCTION_SET) \
    SECTION("hashtable_support_hash_search_##INSTRUCTION_SET##_14") { \
        SECTION("hash - found") { \
            hashtable_hash_half_volatile_t hashes[HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT] = { \
                    123, 234, 345, 456, 567, 678, 789, 890, 901, 012, 987, 876, 765, 654 \
            }; \
            hashtable_hash_half_t hash = 345; \
            uint32_t skip_indexes_mask = 0; \
        \
            REQUIRE(hashtable_support_hash_search_##INSTRUCTION_SET##_14(hash, hashes, skip_indexes_mask) == 2); \
        } \
        \
        SECTION("hash - not found") { \
            hashtable_hash_half_volatile_t hashes[HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT] = { \
                    123, 234, 345, 456, 567, 678, 789, 890, 901, 012, 987, 876, 765, 654 \
            }; \
            hashtable_hash_half_t hash = 999999; \
            uint32_t skip_indexes_mask = 0; \
        \
            REQUIRE(hashtable_support_hash_search_##INSTRUCTION_SET##_14(hash, hashes, skip_indexes_mask) == HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX); \
        } \
        \
        SECTION("hash - multiple - find first") { \
            hashtable_hash_half_volatile_t hashes[HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT] = { \
                    123, 234, 345, 456, 567, 234, 789, 890, 901, 234, 987, 876, 765, 654 \
            }; \
            hashtable_hash_half_t hash = 234; \
            uint32_t skip_indexes_mask = 0; \
        \
            fprintf(stdout, "%d\n", hashtable_support_hash_search_##INSTRUCTION_SET##_14(hash, hashes, skip_indexes_mask)); \
            fflush(stdout); \
        \
            REQUIRE(hashtable_support_hash_search_##INSTRUCTION_SET##_14(hash, hashes, skip_indexes_mask) == 1); \
        } \
        \
        SECTION("hash - multiple - find second") { \
            hashtable_hash_half_volatile_t hashes[HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT] = { \
                    123, 234, 345, 456, 567, 234, 789, 890, 901, 234, 987, 876, 765, 654 \
            }; \
            hashtable_hash_half_t hash = 234; \
            uint32_t skip_indexes_mask = 1 << 1; \
        \
            fprintf(stdout, "%d\n", hashtable_support_hash_search_##INSTRUCTION_SET##_14(hash, hashes, skip_indexes_mask)); \
            fflush(stdout); \
        \
            REQUIRE(hashtable_support_hash_search_##INSTRUCTION_SET##_14(hash, hashes, skip_indexes_mask) == 5); \
        } \
    }

TEST_CASE("hashtable/hashtable_support_hash_search.c",
        "[hashtable][hashtable_support][hashtable_support_hash][hashtable_support_hash_search]") {
#if defined(__x86_64__)
    TEST_HASHTABLE_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(avx2)
    TEST_HASHTABLE_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(avx)
#endif
    TEST_HASHTABLE_SUPPORT_HASH_SEARCH_PLATFORM_DEPENDENT(loop)
}

static void *hashtable_support_hash_search_resolve(void)
{
    __builtin_cpu_init();

    LOG_DI("Selecting optimal hashtable_support_hash_search_resolve");

#if defined(__x86_64__)
    LOG_DI("CPU FOUND: %s", "X64");
    LOG_DI(">  HAS AVX: %s", __builtin_cpu_supports("avx") ? "yes" : "no");
    LOG_DI("> HAS AVX2: %s", __builtin_cpu_supports("avx2") ? "yes" : "no");

    if (__builtin_cpu_supports("avx2")) {
        LOG_DI("Selecting AVX2");

        return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(avx2, 14);
    } else if (__builtin_cpu_supports("avx")) {
        LOG_DI("Selecting AVX");

        return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(avx, 14);
    }
#endif

    LOG_DI("No optimization available, selecting loop-based search algorithm");

    return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(loop, 14);
}
