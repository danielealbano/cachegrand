#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_index.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_support_index.c", "[hashtable][hashtable_support][hashtable_support_index]") {
    SECTION("hashtable_support_index_rounddown_to_cacheline") {
        REQUIRE(hashtable_support_index_rounddown_to_cacheline(buckets_initial_count_5) == 0);
        REQUIRE(hashtable_support_index_rounddown_to_cacheline(buckets_initial_count_100) == 96);
        REQUIRE(hashtable_support_index_rounddown_to_cacheline(buckets_initial_count_305) == 304);
    }

    SECTION("hashtable_support_index_roundup_to_cacheline_plus_one") {
        REQUIRE(hashtable_support_index_roundup_to_cacheline_plus_one(buckets_initial_count_5) == 16);
        REQUIRE(hashtable_support_index_roundup_to_cacheline_plus_one(buckets_initial_count_100) == 112);
        REQUIRE(hashtable_support_index_roundup_to_cacheline_plus_one(buckets_initial_count_305) == 320);
    }

    SECTION("hashtable_support_index_from_hash") {
        SECTION("buckets_initial_count_5") {
            REQUIRE(hashtable_support_index_from_hash(
                    buckets_count_53,
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

    SECTION("hashtable_support_index_calculate_neighborhood_from_index") {
        hashtable_bucket_index_t start, end;

        SECTION("buckets_count_53") {
            hashtable_support_index_calculate_neighborhood_from_index(
                    test_key_1_hash % buckets_count_53,
                    &start,
                    &end);

            REQUIRE(start == 8);
            REQUIRE(end == 23);
        }

        SECTION("buckets_count_101") {
            hashtable_support_index_calculate_neighborhood_from_index(
                    test_key_1_hash % buckets_count_101,
                    &start,
                    &end);

            REQUIRE(start == 64);
            REQUIRE(end == 79);
        }

        SECTION("buckets_initial_count_305") {
            hashtable_support_index_calculate_neighborhood_from_index(
                    test_key_1_hash % buckets_count_307,
                    &start,
                    &end);

            REQUIRE(start == 8);
            REQUIRE(end == 23);
        }
    }

    SECTION("hashtable_support_index_calculate_neighborhood_from_hash") {
        hashtable_bucket_index_t start, end;

        SECTION("buckets_count_53") {
            hashtable_support_index_calculate_neighborhood_from_hash(
                    buckets_count_53,
                    test_key_1_hash,
                    &start,
                    &end);

            REQUIRE(start == 8);
            REQUIRE(end == 23);
        }

        SECTION("buckets_count_101") {
            hashtable_support_index_calculate_neighborhood_from_hash(
                    buckets_count_101,
                    test_key_1_hash,
                    &start,
                    &end);

            REQUIRE(start == 64);
            REQUIRE(end == 79);
        }

        SECTION("buckets_initial_count_305") {
            hashtable_support_index_calculate_neighborhood_from_hash(
                    buckets_count_307,
                    test_key_1_hash,
                    &start,
                    &end);

            REQUIRE(start == 8);
            REQUIRE(end == 23);
        }
    }
}
