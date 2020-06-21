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
