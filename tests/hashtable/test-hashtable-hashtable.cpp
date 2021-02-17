#include <catch2/catch.hpp>

#include "exttypes.h"
#include "spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable.c", "[hashtable][hashtable]") {
    SECTION("hashtable_mcmp_init") {
        HASHTABLE(buckets_initial_count_5, false, {
            REQUIRE(hashtable != NULL);
        })
    }

    SECTION("hashtable->hashtable_data->buckets_count") {
        HASHTABLE(0x07, false, {
            REQUIRE(hashtable->ht_current->buckets_count == 0x08u);
        })

        HASHTABLE(0x7F, false, {
            REQUIRE(hashtable->ht_current->buckets_count == 0x80u);
        })

        HASHTABLE(0x1FF, false, {
            REQUIRE(hashtable->ht_current->buckets_count == 0x200u);
        })
    }

    SECTION("hashtable const vales") {
        SECTION("HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT == 14") {
            REQUIRE(HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT == 14);
        }
    }

    SECTION("hashtable struct size") {
        SECTION("sizeof(hashtable_key_value_t) == 32") {
            REQUIRE(sizeof(hashtable_key_value_t) == 32);
        }

        SECTION("sizeof(hashtable_half_hashes_chunk_atomic_t) == 64") {
            REQUIRE(sizeof(hashtable_half_hashes_chunk_volatile_t) == 64);
        }

        SECTION("sizeof(hashtable_half_hashes_chunk_atomic.metadata.padding) == 4") {
            hashtable_half_hashes_chunk_volatile_t hashtable_half_hashes_chunk_atomic = { 0 };
            REQUIRE(sizeof(hashtable_half_hashes_chunk_atomic.metadata.padding) == 4);
        }

        SECTION("sizeof(hashtable_half_hashes_chunk_atomic.half_hashes) == 4 * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT)") {
            hashtable_half_hashes_chunk_volatile_t hashtable_half_hashes_chunk_atomic = { 0 };
            REQUIRE(sizeof(hashtable_half_hashes_chunk_atomic.half_hashes) == 4 * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
        }
    }
}
