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
}
