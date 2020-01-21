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

    SECTION("hashtable_support_index_roundup_to_cacheline_to_probe") {
        REQUIRE(hashtable_support_index_roundup_to_cacheline_to_probe(
                buckets_initial_count_5, cachelines_to_probe_2) == 32);
        REQUIRE(hashtable_support_index_roundup_to_cacheline_to_probe(
                buckets_initial_count_100, cachelines_to_probe_2) == 128);
        REQUIRE(hashtable_support_index_roundup_to_cacheline_to_probe(
                buckets_initial_count_305, cachelines_to_probe_2) == 336);
    }

    SECTION("hashtable_support_index_from_hash") {
        SECTION("buckets_initial_count_5") {
            REQUIRE(hashtable_support_index_from_hash(
                    buckets_count_42,
                    test_key_1_hash) == 20);
        }

        SECTION("buckets_initial_count_100") {
            REQUIRE(hashtable_support_index_from_hash(
                    buckets_count_101,
                    test_key_1_hash) == 0);
        }

        SECTION("buckets_initial_count_305") {
            REQUIRE(hashtable_support_index_from_hash(
                    buckets_count_307,
                    test_key_1_hash) == 272);
        }
    }

    SECTION("hashtable_support_index_calculate_neighborhood_from_index") {
        hashtable_bucket_index_t start, end;

        SECTION("buckets_count_42") {
            hashtable_support_index_calculate_neighborhood_from_index(
                    test_key_1_hash % buckets_count_42,
                    cachelines_to_probe_2,
                    &start,
                    &end);

            REQUIRE(start == 16);
            REQUIRE(end == 47);
        }

        SECTION("buckets_count_101") {
            hashtable_support_index_calculate_neighborhood_from_index(
                    test_key_1_hash % buckets_count_101,
                    cachelines_to_probe_2,
                    &start,
                    &end);

            REQUIRE(start == 0);
            REQUIRE(end == 31);
        }

        SECTION("buckets_initial_count_305") {
            hashtable_support_index_calculate_neighborhood_from_index(
                    test_key_1_hash % buckets_count_307,
                    cachelines_to_probe_2,
                    &start,
                    &end);

            REQUIRE(start == 272);
            REQUIRE(end == 303);
        }
    }

    SECTION("hashtable_support_index_calculate_neighborhood_from_hash") {
        hashtable_bucket_index_t start, end;

        SECTION("buckets_count_42") {
            hashtable_support_index_calculate_neighborhood_from_hash(
                    buckets_count_42,
                    test_key_1_hash,
                    cachelines_to_probe_2,
                    &start,
                    &end);

            REQUIRE(start == 16);
            REQUIRE(end == 47);
        }

        SECTION("buckets_count_101") {
            hashtable_support_index_calculate_neighborhood_from_hash(
                    buckets_count_101,
                    test_key_1_hash,
                    cachelines_to_probe_2,
                    &start,
                    &end);

            REQUIRE(start == 0);
            REQUIRE(end == 31);
        }

        SECTION("buckets_initial_count_305") {
            hashtable_support_index_calculate_neighborhood_from_hash(
                    buckets_count_307,
                    test_key_1_hash,
                    cachelines_to_probe_2,
                    &start,
                    &end);

            REQUIRE(start == 272);
            REQUIRE(end == 303);
        }
    }
}
