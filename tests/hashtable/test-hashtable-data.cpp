#include "catch2/catch.hpp"

#include "exttypes.h"
#include "spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_data.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable_data.c", "[hashtable][hashtable_data]") {
    SECTION("hashtable_mcmp_data_init") {
        HASHTABLE_DATA(0x80u, {
            /* do nothing */
        })
    }

    SECTION("hashtable_mcmp_data_init - mmap zero'ed") {
        HASHTABLE_DATA(0x80u, {
            for (
                    hashtable_chunk_index_t chunk_index = 0;
                    chunk_index < hashtable_data->chunks_count;
                    chunk_index++) {
                for (
                        hashtable_chunk_slot_index_t chunk_slot_index = 0;
                        chunk_slot_index < HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;
                        chunk_slot_index++) {
                    REQUIRE(hashtable_data->half_hashes_chunk[chunk_index].half_hashes[chunk_slot_index].slot_id == 0);
                }
            }

            for (
                    hashtable_bucket_index_t bucket_index = 0;
                    bucket_index < hashtable_data->buckets_count_real;
                    bucket_index++) {
                REQUIRE(hashtable_data->keys_values[bucket_index].data == 0);
            }
        })
    }

    SECTION("hashtable_data->buckets_count") {
        HASHTABLE_DATA(0x08u, {
            REQUIRE(hashtable_data->buckets_count == 0x08u);
        })
    }

    SECTION("hashtable_data->buckets_count_real") {
        HASHTABLE_DATA(0x80u, {
            uint64_t buckets_count_real_calc =
                    hashtable_data->buckets_count +
                    (hashtable_data->buckets_count % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) +
                    (HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
            REQUIRE(hashtable_data->buckets_count_real == buckets_count_real_calc);
        })
    }

    SECTION("hashtable_data->chunks_count") {
        HASHTABLE_DATA(0x80u, {
            REQUIRE(hashtable_data->chunks_count ==
                    hashtable_data->buckets_count_real / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
        })
    }

    SECTION("invalid buckets_count") {
        hashtable_data_t *hashtable_data;

        hashtable_data = hashtable_mcmp_data_init(0x81);

        REQUIRE(hashtable_data == NULL);
    }
}
