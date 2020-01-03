#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support.h"
#include "hashtable/hashtable_gc.h"
#include "hashtable/hashtable_op_get.h"
#include "hashtable/hashtable_op_set.h"

#include "test-hashtable.h"

TEST_CASE("hashtable_data.c", "[hashtable][hashtable_data]") {
    SECTION("hashtable_data_init") {
        HASHTABLE_DATA_INIT(buckets_initial_count_5, {
            /* do nothing */
        })
    }

    SECTION("hashtable_data->buckets_count") {
        HASHTABLE_DATA_INIT(buckets_initial_count_5, {
            REQUIRE(hashtable_data->buckets_count == buckets_count_53);
        })

        HASHTABLE_DATA_INIT(buckets_initial_count_100, {
            REQUIRE(hashtable_data->buckets_count == buckets_count_101);
        })

        HASHTABLE_DATA_INIT(buckets_initial_count_305, {
            REQUIRE(hashtable_data->buckets_count == buckets_count_307);
        })
    }

    SECTION("hashtable_data->buckets_count_real") {
        HASHTABLE_DATA_INIT(buckets_initial_count_5, {
            REQUIRE(hashtable_data->buckets_count_real == buckets_count_real_64);
        })

        HASHTABLE_DATA_INIT(buckets_initial_count_100, {
            REQUIRE(hashtable_data->buckets_count_real == buckets_count_real_112);
        })

        HASHTABLE_DATA_INIT(buckets_initial_count_305, {
            REQUIRE(hashtable_data->buckets_count_real == buckets_count_real_320);
        })
    }

    SECTION("invalid buckets_count") {
        hashtable_data_t *hashtable_data;

        hashtable_data = hashtable_data_init(HASHTABLE_PRIMENUMBERS_MAX + 5);

        REQUIRE(hashtable_data == NULL);
    }
}
