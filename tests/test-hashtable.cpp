#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable.c", "[hashtable]") {
    SECTION("hashtable_init") {
        HASHTABLE_INIT(buckets_initial_count_5, false, {
            REQUIRE(hashtable != NULL);
        })
    }
}
