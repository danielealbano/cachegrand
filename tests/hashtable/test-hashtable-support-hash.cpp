#include <catch2/catch.hpp>

#include "exttypes.h"
#include "spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_support_hash.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable_support_hash.c", "[hashtable][hashtable_support][hashtable_support_hash]") {
    SECTION("hashtable_mcmp_support_hash_calculate") {
        SECTION("hash calculation") {
            REQUIRE(hashtable_mcmp_support_hash_calculate(
                    test_key_1,
                    test_key_1_len) == test_key_1_hash);
        }
    }

    SECTION("hashtable_mcmp_support_hash_half") {
        SECTION("hash half calculation") {
            REQUIRE(hashtable_mcmp_support_hash_half(test_key_1_hash) == test_key_1_hash_half);
        }

        SECTION("ensure the upper bit is always set to 1") {
            REQUIRE((hashtable_mcmp_support_hash_half(test_key_1_hash) & 0x80000000u) == 0x80000000u);
        }
    }
}
