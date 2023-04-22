/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "data_structures/slots_bitmap_spsc/slots_bitmap_spsc.h"

TEST_CASE("data_structures/slots_bitmap_spsc/slots_bitmap_spsc.c", "[data_structures][slots_bitmap_spsc]") {
    SECTION("slots_bitmap_spsc_calculate_shard_count") {
        SECTION("Size is zero") {
            REQUIRE(slots_bitmap_spsc_calculate_shard_count(0) == 0);
        }

        SECTION("Size is 1") {
            REQUIRE(slots_bitmap_spsc_calculate_shard_count(1) == 1);
        }

        SECTION("Size is a multiple of shard size") {
            REQUIRE(slots_bitmap_spsc_calculate_shard_count(64) == 1);
            REQUIRE(slots_bitmap_spsc_calculate_shard_count(128) == 2);
        }

        SECTION("Size is not a multiple of shard size") {
            REQUIRE(slots_bitmap_spsc_calculate_shard_count(65) == 2);
            REQUIRE(slots_bitmap_spsc_calculate_shard_count(127) == 2);
            REQUIRE(slots_bitmap_spsc_calculate_shard_count(129) == 3);
        }
    }

    SECTION("slots_bitmap_spsc_init") {
        SECTION("Size is zero") {
            slots_bitmap_spsc_t *bitmap = slots_bitmap_spsc_init(0);
            REQUIRE(bitmap == NULL);
        }

        SECTION("Size is positive") {
            uint64_t size = 10;
            slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);
            REQUIRE(slots_bitmap != NULL);
            REQUIRE(slots_bitmap->size == 64);
            REQUIRE(slots_bitmap->shards_count == 1);
            REQUIRE(slots_bitmap_spsc_get_shard_ptr(slots_bitmap, 0) != NULL);
            REQUIRE(slots_bitmap_spsc_get_shard_used_count_ptr(slots_bitmap, 0) != NULL);
            REQUIRE(slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0) != NULL);
            slots_bitmap_spsc_free(slots_bitmap);

            size = 65;
            slots_bitmap = slots_bitmap_spsc_init(size);
            REQUIRE(slots_bitmap != NULL);
            REQUIRE(slots_bitmap->size == 128);
            REQUIRE(slots_bitmap->shards_count == 2);
            REQUIRE(slots_bitmap_spsc_get_shard_ptr(slots_bitmap, 0) != NULL);
            REQUIRE(slots_bitmap_spsc_get_shard_ptr(slots_bitmap, 1) != NULL);
            REQUIRE(slots_bitmap_spsc_get_shard_used_count_ptr(slots_bitmap, 0) != NULL);
            REQUIRE(slots_bitmap_spsc_get_shard_used_count_ptr(slots_bitmap, 1) != NULL);
            REQUIRE(slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0) != NULL);
            REQUIRE(slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 1) != NULL);
            slots_bitmap_spsc_free(slots_bitmap);
        }
    }

    SECTION("slots_bitmap_spsc_free") {
        // Cannot test since it only frees memory, no return value or side effects to test
    }

    SECTION("slots_bitmap_spsc_get_shard_ptr") {
        uint64_t size = 10;
        slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);
        REQUIRE(slots_bitmap_spsc_get_shard_ptr(slots_bitmap, 0) == slots_bitmap->shards);
        slots_bitmap_spsc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_spsc_init(size);
        REQUIRE(slots_bitmap_spsc_get_shard_ptr(slots_bitmap, 0) == slots_bitmap->shards);
        REQUIRE(slots_bitmap_spsc_get_shard_ptr(slots_bitmap, 1) == &(slots_bitmap->shards[1]));
        slots_bitmap_spsc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_spsc_get_shard_used_count_ptr") {
        uint64_t size = 10;
        slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);

        auto bitmap_ptr = (uintptr_t)slots_bitmap;
        uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_spsc_t, shards);
        uintptr_t bitmap_shards_used_slots_ptr =
                bitmap_shards_ptr + (sizeof(slots_bitmap_spsc_shard_t) * 1);

        REQUIRE(slots_bitmap_spsc_get_shard_used_count_ptr(slots_bitmap, 0) ==
            (uint8_t*)bitmap_shards_used_slots_ptr);
        slots_bitmap_spsc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_spsc_init(size);

        bitmap_ptr = (uintptr_t)slots_bitmap;
        bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_spsc_t, shards);
        bitmap_shards_used_slots_ptr =
                bitmap_shards_ptr + (sizeof(slots_bitmap_spsc_shard_t) * 2);

        REQUIRE(slots_bitmap_spsc_get_shard_used_count_ptr(slots_bitmap, 0) ==
                (uint8_t*)bitmap_shards_used_slots_ptr);
        slots_bitmap_spsc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_spsc_get_shard_full_ptr") {
        uint64_t size = 10;

        slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);
        auto bitmap_ptr = (uintptr_t)slots_bitmap;
        uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_spsc_t, shards);
        uintptr_t bitmap_shards_used_slots_ptr =
                bitmap_shards_ptr + (sizeof(slots_bitmap_spsc_shard_t) * 1);
        uintptr_t bitmap_shard_full_ptr =
                bitmap_shards_used_slots_ptr + (sizeof(uint8_t) * 1);

        REQUIRE(slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0) == (uint64_t*)bitmap_shard_full_ptr);
        slots_bitmap_spsc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_spsc_init(size);
        bitmap_ptr = (uintptr_t)slots_bitmap;
        bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_spsc_t, shards);
        bitmap_shards_used_slots_ptr =
                bitmap_shards_ptr + (sizeof(slots_bitmap_spsc_shard_t) * 2);
        bitmap_shard_full_ptr =
                bitmap_shards_used_slots_ptr + (sizeof(uint8_t) * 2);
        REQUIRE(slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0) == (uint64_t*)bitmap_shard_full_ptr);

        slots_bitmap_spsc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_spsc_set_shard_full_bit") {
        uint64_t size = 10;
        slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, true);
        REQUIRE(*slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0) == 1);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, false);
        REQUIRE(*slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0) == 0);
        slots_bitmap_spsc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_spsc_init(size);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, true);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 1, true);
        REQUIRE(*slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0) == 3);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, false);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 1, false);
        REQUIRE(*slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0) == 0);
        slots_bitmap_spsc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_spsc_is_shard_full") {
        uint64_t size = 10;
        slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 0) == false);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, true);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 0) == true);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, false);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 0) == false);
        slots_bitmap_spsc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_spsc_init(size);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 0) == false);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 1) ==false);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, true);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 1, true);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 0) == true);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 1) == true);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, false);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 1, false);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 0) == false);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 1) == false);
        slots_bitmap_spsc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_spsc_get_next_available_ptr") {
        uint64_t size = 10;
        slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);
        uint64_t index;
        REQUIRE(slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index) == true);
        REQUIRE(index == 0);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, true);
        REQUIRE(slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index) == false);
        slots_bitmap_spsc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_spsc_init(size);
        REQUIRE(slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index) == true);
        REQUIRE(index == 0);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, true);
        REQUIRE(slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index) == true);
        REQUIRE(index == SLOTS_BITMAP_SPSC_SHARD_SIZE);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 1, true);
        REQUIRE(slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index) == false);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, false);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 1, false);
        REQUIRE(slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index) == true);
        REQUIRE(index == 1);
        slots_bitmap_spsc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_spsc_get_next_available") {
        uint64_t size = 10;

        slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);
        REQUIRE(slots_bitmap_spsc_get_next_available(slots_bitmap) == 0);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, true);
        REQUIRE(slots_bitmap_spsc_get_next_available(slots_bitmap) == UINT64_MAX);
        slots_bitmap_spsc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_spsc_init(size);
        REQUIRE(slots_bitmap_spsc_get_next_available(slots_bitmap) == 0);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, true);
        REQUIRE(slots_bitmap_spsc_get_next_available(slots_bitmap) == SLOTS_BITMAP_SPSC_SHARD_SIZE);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 1, true);
        REQUIRE(slots_bitmap_spsc_get_next_available(slots_bitmap) == UINT64_MAX);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 0, false);
        slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, 1, false);
        REQUIRE(slots_bitmap_spsc_get_next_available(slots_bitmap) == 1);
        slots_bitmap_spsc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_spsc_release") {
        uint64_t size = 10;
        slots_bitmap_spsc_t *slots_bitmap = slots_bitmap_spsc_init(size);
        uint64_t index = 0;
        slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index);
        slots_bitmap_spsc_release(slots_bitmap, index);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 0) == false);
        slots_bitmap_spsc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_spsc_init(size);
        slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index);
        slots_bitmap_spsc_release(slots_bitmap, index);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 0) == false);
        slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index);
        slots_bitmap_spsc_release(slots_bitmap, index);
        REQUIRE(slots_bitmap_spsc_is_shard_full(slots_bitmap, 1) == false);
        slots_bitmap_spsc_free(slots_bitmap);
    }
}