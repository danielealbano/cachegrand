#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support_op.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_support_op.c", "[hashtable][hashtable_support][hashtable_support_op]") {
    SECTION("hashtable_support_op_bucket_lock") {
        SECTION("testing lock with retry") {
            hashtable_bucket_t* bucket = (hashtable_bucket_t*)malloc(sizeof(hashtable_bucket_t));
            bucket->write_lock = 0;
            bucket->reserved0 = 123;
            bucket->reserved1 = 123;
            bucket->reserved2 = 123;
            bucket->reserved3 = 123;
            bucket->chain_first_ring = (volatile hashtable_bucket_chain_ring_t*)123;

            bool res = hashtable_support_op_bucket_lock(bucket, true);

            REQUIRE(res);
            REQUIRE(bucket->write_lock == 1);
            REQUIRE(bucket->reserved0 == 123);
            REQUIRE(bucket->reserved1 == 123);
            REQUIRE(bucket->reserved2 == 123);
            REQUIRE(bucket->reserved3 == 123);
            REQUIRE(bucket->chain_first_ring == (volatile hashtable_bucket_chain_ring_t*)123);

            free(bucket);
        }

        SECTION("testing lock without retry") {
            hashtable_bucket_t* bucket = (hashtable_bucket_t*)malloc(sizeof(hashtable_bucket_t));
            bucket->write_lock = 1;
            bucket->reserved0 = 123;
            bucket->reserved1 = 123;
            bucket->reserved2 = 123;
            bucket->reserved3 = 123;
            bucket->chain_first_ring = (volatile hashtable_bucket_chain_ring_t*)123;

            bool res = hashtable_support_op_bucket_lock(bucket, false);

            REQUIRE(!res);
            REQUIRE(bucket->write_lock == 1);
            REQUIRE(bucket->reserved0 == 123);
            REQUIRE(bucket->reserved1 == 123);
            REQUIRE(bucket->reserved2 == 123);
            REQUIRE(bucket->reserved3 == 123);
            REQUIRE(bucket->chain_first_ring == (volatile hashtable_bucket_chain_ring_t*)123);

            free(bucket);
        }
    }

    SECTION("hashtable_support_op_bucket_unlock") {
        SECTION("testing unlock") {
            hashtable_bucket_t* bucket = (hashtable_bucket_t*)malloc(sizeof(hashtable_bucket_t));
            bucket->write_lock = 1;
            bucket->reserved0 = 123;
            bucket->reserved1 = 123;
            bucket->reserved2 = 123;
            bucket->reserved3 = 123;
            bucket->chain_first_ring = (volatile hashtable_bucket_chain_ring_t*)123;

            hashtable_support_op_bucket_unlock(bucket);

            REQUIRE(bucket->write_lock == 0);
            REQUIRE(bucket->reserved0 == 123);
            REQUIRE(bucket->reserved1 == 123);
            REQUIRE(bucket->reserved2 == 123);
            REQUIRE(bucket->reserved3 == 123);
            REQUIRE(bucket->chain_first_ring == (volatile hashtable_bucket_chain_ring_t*)123);

            free(bucket);
        }
    }

    SECTION("hashtable_support_op_bucket_fetch_and_write_lock") {
        SECTION("testing fetching new bucket") {
            HASHTABLE_DATA(buckets_initial_count_5, {
                bool created_new_bucket = false;
                volatile hashtable_bucket_t* bucket = hashtable_support_op_bucket_fetch_and_write_lock(
                        hashtable_data,
                        0,
                        true,
                        &created_new_bucket);

                REQUIRE(created_new_bucket == true);
                REQUIRE(bucket != NULL);
                REQUIRE(bucket->write_lock == 1);
            })
        }

        SECTION("testing fetching existing bucket") {
            HASHTABLE_DATA(buckets_initial_count_5, {
                bool created_new_bucket1 = false;
                bool created_new_bucket2 = false;
                volatile hashtable_bucket_t* bucket1 = hashtable_support_op_bucket_fetch_and_write_lock(
                        hashtable_data,
                        0,
                        true,
                        &created_new_bucket1);
                REQUIRE(bucket1->write_lock == 1);
                hashtable_support_op_bucket_unlock(bucket1);

                volatile hashtable_bucket_t* bucket2 = hashtable_support_op_bucket_fetch_and_write_lock(
                        hashtable_data,
                        0,
                        true,
                        &created_new_bucket2);

                REQUIRE(created_new_bucket1 == true);
                REQUIRE(created_new_bucket2 == false);
                REQUIRE(bucket2->write_lock == 1);
                REQUIRE(bucket1 == bucket2);
            })
        }

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
    }
}




