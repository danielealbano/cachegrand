#include "catch.hpp"

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_index.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_support_index.c", "[hashtable][hashtable_support][hashtable_support_index]") {
    SECTION("hashtable_support_index_from_hash") {
        SECTION("buckets_initial_count_5") {
            REQUIRE(hashtable_support_index_from_hash(
                    buckets_count_42,
                    test_key_1_hash) == 12);
        }

        SECTION("buckets_initial_count_100") {
            REQUIRE(hashtable_support_index_from_hash(
                    buckets_count_101,
                    test_key_1_hash) == 70);
        }

        SECTION("buckets_initial_count_305") {
            REQUIRE(hashtable_support_index_from_hash(
                    buckets_count_307,
                    test_key_1_hash) == 13);
        }
    }
}
