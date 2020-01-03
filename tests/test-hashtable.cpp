#include "catch.hpp"

#include "cachelinesize.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support.h"
#include "hashtable/hashtable_gc.h"
#include "hashtable/hashtable_op_get.h"
#include "hashtable/hashtable_op_set.h"

#define HASHTABLE_DATA_INIT(buckets_count_v, ...) \
{ \
    hashtable_data_t* hashtable_data = hashtable_data_init(buckets_count_v); \
    __VA_ARGS__; \
    hashtable_data_free(hashtable_data); \
}

#define HASHTABLE_INIT(initial_size_v, can_auto_resize_v, ...) \
{ \
    hashtable_config_t* hashtable_config = hashtable_config_init();  \
    hashtable_config->initial_size = initial_size_v; \
    hashtable_config->can_auto_resize = can_auto_resize_v; \
    \
    hashtable_t* hashtable = hashtable_init(hashtable_config); \
    \
    __VA_ARGS__ \
    \
    hashtable_free(hashtable); \
}

TEST_CASE("Hashtable", "[hashtable]") {
    // Shared Test data
    char test_key[] = "cachegrand v2"; // hash(test_key) == 4804135818922944713
    uint64_t test_hash = 4804135818922944713;
    hashtable_bucket_index_t test_index_53 = 51;
    hashtable_bucket_index_t test_index_101 = 77;
    hashtable_bucket_index_t test_index_307 = 20;

    uint64_t buckets_initial_count_5 = 5;
    uint64_t buckets_initial_count_100 = 100;
    uint64_t buckets_initial_count_305 = 305;

    uint64_t buckets_count_53 = 53;
    uint64_t buckets_count_101 = 101;
    uint64_t buckets_count_307 = 307;

    uint64_t buckets_count_real_64 = 64;
    uint64_t buckets_count_real_112 = 112;
    uint64_t buckets_count_real_320 = 320;

    SECTION("hashtable_config_init") {
        hashtable_config_t* hashtable_config = hashtable_config_init();
        hashtable_config_free(hashtable_config);
    }

    SECTION("hashtable_data_init") {
        SECTION("init") {
            HASHTABLE_DATA_INIT(buckets_initial_count_5, {
                /* do nothing */
            })
        }

        SECTION("right buckets_count") {
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

        SECTION("right buckets_count_real") {
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
            hashtable_data_t* hashtable_data;

            hashtable_data = hashtable_data_init(HASHTABLE_PRIMENUMBERS_MAX + 5);

            REQUIRE(hashtable_data == NULL);
        }
    }

    SECTION("hashtable") {
        SECTION("hashtable_init") {
            HASHTABLE_INIT(buckets_initial_count_5, false, { /* do nothing */ })
        }

        SECTION("hashtable_get") {
//            hashtable_t* hashtable = hashtable_init(hashtable_config_5);
//
//            hashtable_bucket_index_t index =
//
//            REQUIRE(hashtable_calculate_hash(test_key, sizeof(test_key)) == test_hash);
//
//            hashtable_free(hashtable);
        }

        SECTION("hashtable_set") {
            // TODO: implement test
        }

        SECTION("hashtable_delete") {
            // TODO: implement test
        }

        SECTION("hashtable_resize") {
            // TODO: implement test
        }
    }
}
