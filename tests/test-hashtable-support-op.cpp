#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support_op.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_support_op.c", "[hashtable][hashtable_support][hashtable_support_op]") {
#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
    SECTION("hashtable_support_op_bucket_lock") {
        SECTION("testing lock with retry") {
            auto bucket = (hashtable_bucket_t*)malloc(sizeof(hashtable_bucket_t));
            bucket->write_lock = 0;
            bucket->padding0[0] = 1; bucket->padding0[1] = 2; bucket->padding0[2] = 3;

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
            bucket->keys_values = (hashtable_bucket_key_value_t *)123;
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

            bool res = hashtable_support_op_bucket_lock(bucket, true);

            REQUIRE(res);
            REQUIRE(bucket->write_lock == 1);
            REQUIRE(bucket->padding0[0] == 1);
            REQUIRE(bucket->padding0[1] == 2);
            REQUIRE(bucket->padding0[2] == 3);

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
            REQUIRE((hashtable_bucket_key_value_t*)bucket->keys_values == (hashtable_bucket_key_value_t*)123);
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

            free(bucket);
        }
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
        SECTION("testing lock without retry") {
            auto bucket = (hashtable_bucket_t*)malloc(sizeof(hashtable_bucket_t));
            bucket->write_lock = 1;
            bucket->padding0[0] = 1; bucket->padding0[1] = 2; bucket->padding0[2] = 3;

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
            bucket->keys_values = (hashtable_bucket_key_value_t *)123;
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

            bool res = hashtable_support_op_bucket_lock(bucket, false);

            REQUIRE(!res);
            REQUIRE(bucket->write_lock == 1);
            REQUIRE(bucket->padding0[0] == 1);
            REQUIRE(bucket->padding0[1] == 2);
            REQUIRE(bucket->padding0[2] == 3);

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
            REQUIRE((hashtable_bucket_key_value_t*)bucket->keys_values == (hashtable_bucket_key_value_t*)123);
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

            free(bucket);
        }
    }
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
    SECTION("hashtable_support_op_bucket_unlock") {
        SECTION("testing unlock") {
            auto bucket = (hashtable_bucket_t*)malloc(sizeof(hashtable_bucket_t));
            bucket->write_lock = 1;
            bucket->padding0[0] = 1; bucket->padding0[1] = 2; bucket->padding0[2] = 3;

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
            bucket->keys_values = (hashtable_bucket_key_value_t *)123;
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0


            hashtable_support_op_bucket_unlock(bucket);

            REQUIRE(bucket->write_lock == 0);
            REQUIRE(bucket->padding0[0] == 1);
            REQUIRE(bucket->padding0[1] == 2);
            REQUIRE(bucket->padding0[2] == 3);
#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
            REQUIRE((hashtable_bucket_key_value_t*)bucket->keys_values == (hashtable_bucket_key_value_t*)123);
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

            free(bucket);
        }
    }
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1

    SECTION("hashtable_support_op_bucket_fetch_and_write_lock") {
#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1 || HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
        SECTION("testing fetching new bucket") {
            HASHTABLE_DATA(buckets_initial_count_5, {
                bool created_new_bucket = false;
                volatile hashtable_bucket_t* bucket = hashtable_support_op_bucket_fetch_and_write_lock(
                        hashtable_data,
                        0,
                        true,
                        &created_new_bucket);

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
                REQUIRE(created_new_bucket == true);
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
                REQUIRE(bucket != NULL);
#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
                REQUIRE(bucket->write_lock == 1);
#else
                REQUIRE(bucket->write_lock == 0);
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
                REQUIRE((hashtable_bucket_key_value_t*)bucket->keys_values != NULL);
            })
        }
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1 || HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1 || HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
        SECTION("testing fetching existing bucket") {
            HASHTABLE_DATA(buckets_initial_count_5, {
                bool created_new_bucket1 = false;
                bool created_new_bucket2 = false;
                volatile hashtable_bucket_t* bucket1 = hashtable_support_op_bucket_fetch_and_write_lock(
                        hashtable_data,
                        0,
                        true,
                        &created_new_bucket1);

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
                REQUIRE(bucket1->write_lock == 1);
#else
                REQUIRE(bucket1->write_lock == 0);
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
                REQUIRE((hashtable_bucket_key_value_t*)bucket1->keys_values != NULL);

#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
                hashtable_support_op_bucket_unlock(bucket1);
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1

                volatile hashtable_bucket_t* bucket2 = hashtable_support_op_bucket_fetch_and_write_lock(
                        hashtable_data,
                        0,
                        true,
                        &created_new_bucket2);

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
                REQUIRE(created_new_bucket1 == true);
                REQUIRE(created_new_bucket2 == false);
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
#if HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
                REQUIRE(bucket2->write_lock == 1);
#else
                REQUIRE(bucket2->write_lock == 0);
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1
                REQUIRE((hashtable_bucket_key_value_t*)bucket2->keys_values != NULL);
                REQUIRE(bucket1 == bucket2);
            })
        }
#endif // HASHTABLE_BUCKET_FEATURE_USE_LOCK == 1 || HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
        SECTION("testing fetching with create_new_if_missing false") {
            HASHTABLE_DATA(buckets_initial_count_5, {
                bool created_new_bucket = false;
                volatile hashtable_bucket_t* bucket = hashtable_support_op_bucket_fetch_and_write_lock(
                        hashtable_data,
                        0,
                        false,
                        &created_new_bucket);

                REQUIRE(created_new_bucket == false);
                REQUIRE(bucket == NULL);
            })
        }
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
    }
}




