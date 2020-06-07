#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_support_hash.c", "[hashtable][hashtable_support][hashtable_support_hash]") {
    SECTION("hashtable_support_hash_calculate") {
        SECTION("test hash calculation") {
            REQUIRE(hashtable_support_hash_calculate(
                    test_key_1,
                    test_key_1_len) == test_key_1_hash);
        }
    }

    SECTION("hashtable_support_hash_half") {
        SECTION("test hash half calculation") {
            REQUIRE(hashtable_support_hash_half(test_key_1_hash) == test_key_1_hash_half);
        }
    }
}
