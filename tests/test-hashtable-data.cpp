#include "catch.hpp"

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support_primenumbers.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_data.c", "[hashtable][hashtable_data]") {
    SECTION("hashtable_data_init") {
        HASHTABLE_DATA(buckets_initial_count_5, {
            /* do nothing */
        })
    }

    SECTION("hashtable_data_init - mmap zero'ed") {
        HASHTABLE_DATA(buckets_initial_count_5, {
            for(
                    hashtable_chunk_index_t chunk_index = 0;
                    chunk_index < hashtable_data->chunks_count;
                    chunk_index++) {
                for(
                        hashtable_chunk_slot_index_t chunk_slot_index = 0;
                        chunk_slot_index < HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;
                        chunk_slot_index++) {
                    REQUIRE(hashtable_data->half_hashes_chunk[chunk_index].half_hashes[chunk_slot_index] == 0);
                }
            }

            for(
                    hashtable_bucket_index_t bucket_index = 0;
                    bucket_index < hashtable_data->buckets_count_real;
                    bucket_index++) {
                REQUIRE(hashtable_data->keys_values[bucket_index].data == 0);
            }
        })
    }

    SECTION("hashtable_data->buckets_count") {
        HASHTABLE_DATA(buckets_initial_count_5, {
            REQUIRE(hashtable_data->buckets_count == buckets_initial_count_5);
        })

        HASHTABLE_DATA(buckets_initial_count_100, {
            REQUIRE(hashtable_data->buckets_count == buckets_initial_count_100);
        })

        HASHTABLE_DATA(buckets_initial_count_305, {
            REQUIRE(hashtable_data->buckets_count == buckets_initial_count_305);
        })
    }

    SECTION("hashtable_data->buckets_count_real") {
        HASHTABLE_DATA(buckets_initial_count_5, {
            uint64_t buckets_count_real_calc =
                    buckets_initial_count_5 +
                    (buckets_initial_count_5 % HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) +
                    (HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);
            REQUIRE(hashtable_data->buckets_count_real == buckets_count_real_calc);
        })

        HASHTABLE_DATA(buckets_initial_count_100, {
            uint64_t buckets_count_real_calc =
                    buckets_initial_count_100 +
                    (buckets_initial_count_100 % HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) +
                    (HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);
            REQUIRE(hashtable_data->buckets_count_real == buckets_count_real_calc);
        })

        HASHTABLE_DATA(buckets_initial_count_305, {
            uint64_t buckets_count_real_calc =
                    buckets_initial_count_305 +
                    (buckets_initial_count_305 % HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT) +
                    (HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX * HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);
            REQUIRE(hashtable_data->buckets_count_real == buckets_count_real_calc);
        })
    }

    SECTION("hashtable_data->chunks_count") {
        HASHTABLE_DATA(buckets_initial_count_5, {
            REQUIRE(hashtable_data->chunks_count == hashtable_data->buckets_count_real / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);
        })

        HASHTABLE_DATA(buckets_initial_count_100, {
            REQUIRE(hashtable_data->chunks_count == hashtable_data->buckets_count_real / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);
        })

        HASHTABLE_DATA(buckets_initial_count_305, {
            REQUIRE(hashtable_data->chunks_count == hashtable_data->buckets_count_real / HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT);
        })
    }

    SECTION("invalid buckets_count") {
        hashtable_data_t *hashtable_data;

        hashtable_data = hashtable_data_init(HASHTABLE_PRIMENUMBERS_MAX + 1);

        REQUIRE(hashtable_data == NULL);
    }
}
