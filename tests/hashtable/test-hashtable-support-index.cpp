#include <catch2/catch.hpp>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_support_index.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable_support_index.c", "[hashtable][hashtable_support][hashtable_support_index]") {
    SECTION("hashtable_mcmp_support_index_from_hash") {
        SECTION("buckets_initial_count_5") {
            REQUIRE(hashtable_mcmp_support_index_from_hash(
                    0x80u,
                    test_key_1_hash) == 76);
        }

        SECTION("buckets_initial_count_100") {
            REQUIRE(hashtable_mcmp_support_index_from_hash(
                    0x8000u,
                    test_key_1_hash) == 24908);
        }

        SECTION("buckets_initial_count_305") {
            REQUIRE(hashtable_mcmp_support_index_from_hash(
                    0x800000u,
                    test_key_1_hash) == 4940108);
        }
    }
}
