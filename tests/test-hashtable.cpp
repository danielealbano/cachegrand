#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable.c", "[hashtable]") {
    SECTION("hashtable_init") {
        HASHTABLE(buckets_initial_count_5, false, {
            REQUIRE(hashtable != NULL);
        })
    }

    SECTION("hashtable->hashtable_data->buckets_count") {
        HASHTABLE(buckets_initial_count_5, false, {
            REQUIRE(hashtable->ht_current->buckets_count == buckets_count_42);
        })

        HASHTABLE(buckets_initial_count_100, false, {
            REQUIRE(hashtable->ht_current->buckets_count == buckets_count_101);
        })

        HASHTABLE(buckets_initial_count_305, false, {
            REQUIRE(hashtable->ht_current->buckets_count == buckets_count_307);
        })
    }

    SECTION("hashtable struct size") {
        SECTION("sizeof(hashtable_bucket_key_value_t) == 32") {
            REQUIRE(sizeof(hashtable_key_value_t) == 32);
        }

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
        SECTION("sizeof(hashtable_bucket_t) == 128") {
            REQUIRE(sizeof(hashtable_half_hashes_chunk_atomic_t) == 128);
        }
#else
        SECTION("sizeof(hashtable_bucket_t) == 512") {
            REQUIRE(sizeof(hashtable_bucket_t) == 512);
        }
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
    }
}
