#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_primenumbers.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_support_primenumbers.c", "[hashtable][hashtable_support][hashtable_support_primenumbers]") {
    SECTION("hashtable_support_primenumbers_next") {
        SECTION("allowed values") {
            HASHTABLE_PRIMENUMBERS_FOREACH(primenumbers, i, {
                REQUIRE(hashtable_support_primenumbers_next(primenumbers[i] - 1) == primenumbers[i]);
            });
        }

        SECTION("unsupported value") {
            REQUIRE(hashtable_support_primenumbers_next(HASHTABLE_PRIMENUMBERS_MAX + 1) == 0);
        }
    }

    SECTION("hashtable_support_primenumbers_mod") {
        SECTION("allowed values") {
            HASHTABLE_PRIMENUMBERS_FOREACH(primenumbers, i, {
                REQUIRE(hashtable_support_primenumbers_mod(primenumbers[i], primenumbers[i]) == 0);
            });
        }

        SECTION("unsupported value") {
            REQUIRE(hashtable_support_primenumbers_mod(1234, HASHTABLE_PRIMENUMBERS_MAX + 1) == 0);
            REQUIRE(hashtable_support_primenumbers_mod(4321, HASHTABLE_PRIMENUMBERS_MAX + 1) == 0);
            REQUIRE(hashtable_support_primenumbers_mod(1111, HASHTABLE_PRIMENUMBERS_MAX + 1) == 0);
            REQUIRE(hashtable_support_primenumbers_mod(4444, HASHTABLE_PRIMENUMBERS_MAX + 1) == 0);
        }
    }
}
