#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support_op.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable_support_op.c", "[hashtable][hashtable_support][hashtable_support_op]") {
    SECTION("hashtable_support_op_bucket_lock") {
        SECTION("lock with retry") {
            auto chunk = (hashtable_half_hashes_chunk_atomic_t*)malloc(sizeof(hashtable_half_hashes_chunk_atomic_t));
            chunk->metadata.write_lock = 0;
            chunk->metadata.overflowed_chunks_counter = 1;
            chunk->metadata.changes_counter = 2;
            chunk->metadata.is_full = 3;

            bool res = hashtable_support_op_half_hashes_chunk_lock(chunk, true);

            REQUIRE(res);
            REQUIRE(chunk->metadata.write_lock == 1);
            REQUIRE(chunk->metadata.overflowed_chunks_counter == 1);
            REQUIRE(chunk->metadata.changes_counter == 2);
            REQUIRE(chunk->metadata.is_full == 3);

            free((void*)chunk);
        }

        SECTION("lock without retry") {
            auto chunk = (hashtable_half_hashes_chunk_atomic_t*)malloc(sizeof(hashtable_half_hashes_chunk_atomic_t));
            chunk->metadata.write_lock = 1;
            chunk->metadata.overflowed_chunks_counter = 1;
            chunk->metadata.changes_counter = 2;
            chunk->metadata.is_full = 3;

            bool res = hashtable_support_op_half_hashes_chunk_lock(chunk, false);

            REQUIRE(!res);
            REQUIRE(chunk->metadata.write_lock == 1);
            REQUIRE(chunk->metadata.overflowed_chunks_counter == 1);
            REQUIRE(chunk->metadata.changes_counter == 2);
            REQUIRE(chunk->metadata.is_full == 3);

            free((void*)chunk);
        }
    }

    SECTION("hashtable_support_op_bucket_unlock") {
        auto chunk = (hashtable_half_hashes_chunk_atomic_t*)malloc(sizeof(hashtable_half_hashes_chunk_atomic_t));
        chunk->metadata.write_lock = 1;
        chunk->metadata.overflowed_chunks_counter = 1;
        chunk->metadata.changes_counter = 2;
        chunk->metadata.is_full = 3;

        hashtable_support_op_half_hashes_chunk_unlock(chunk);

        REQUIRE(chunk->metadata.write_lock == 0);
        REQUIRE(chunk->metadata.overflowed_chunks_counter == 1);
        REQUIRE(chunk->metadata.changes_counter == 2);
        REQUIRE(chunk->metadata.is_full == 3);

        free((void*)chunk);
    }
}




