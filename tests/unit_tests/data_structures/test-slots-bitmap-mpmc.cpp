/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"

#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"

TEST_CASE("data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.c", "[data_structures][slots_bitmap_mpmc]") {
    SECTION("slots_bitmap_mpmc_calculate_shard_count") {
        SECTION("Size is zero") {
            REQUIRE(slots_bitmap_mpmc_calculate_shard_count(0) == 0);
        }

        SECTION("Size is 1") {
            REQUIRE(slots_bitmap_mpmc_calculate_shard_count(1) == 1);
        }

        SECTION("Size is a multiple of shard size") {
            REQUIRE(slots_bitmap_mpmc_calculate_shard_count(64) == 1);
            REQUIRE(slots_bitmap_mpmc_calculate_shard_count(128) == 2);
        }

        SECTION("Size is not a multiple of shard size") {
            REQUIRE(slots_bitmap_mpmc_calculate_shard_count(65) == 2);
            REQUIRE(slots_bitmap_mpmc_calculate_shard_count(127) == 2);
            REQUIRE(slots_bitmap_mpmc_calculate_shard_count(129) == 3);
        }
    }

    SECTION("slots_bitmap_mpmc_init") {
        SECTION("Size is zero") {
            slots_bitmap_mpmc_t *bitmap = slots_bitmap_mpmc_init(0);
            REQUIRE(bitmap == NULL);
        }

        SECTION("Size is positive") {
            uint64_t size = 10;
            slots_bitmap_mpmc_t *slots_bitmap = slots_bitmap_mpmc_init(size);
            REQUIRE(slots_bitmap != NULL);
            REQUIRE(slots_bitmap->size == 64);
            REQUIRE(slots_bitmap->shards_count == 1);
            REQUIRE(slots_bitmap_mpmc_get_shard_ptr(slots_bitmap, 0) != NULL);
            REQUIRE(slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, 0) != NULL);
            slots_bitmap_mpmc_free(slots_bitmap);

            size = 65;
            slots_bitmap = slots_bitmap_mpmc_init(size);
            REQUIRE(slots_bitmap != NULL);
            REQUIRE(slots_bitmap->size == 128);
            REQUIRE(slots_bitmap->shards_count == 2);
            REQUIRE(slots_bitmap_mpmc_get_shard_ptr(slots_bitmap, 0) != NULL);
            REQUIRE(slots_bitmap_mpmc_get_shard_ptr(slots_bitmap, 1) != NULL);
            REQUIRE(slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, 0) != NULL);
            REQUIRE(slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, 1) != NULL);
            slots_bitmap_mpmc_free(slots_bitmap);
        }
    }

    SECTION("slots_bitmap_mpmc_free") {
        // Cannot test since it only frees memory, no return value or side effects to test
    }

    SECTION("slots_bitmap_mpmc_get_shard_ptr") {
        uint64_t size = 10;
        slots_bitmap_mpmc_t *slots_bitmap = slots_bitmap_mpmc_init(size);
        REQUIRE(slots_bitmap_mpmc_get_shard_ptr(slots_bitmap, 0) == slots_bitmap->shards);
        slots_bitmap_mpmc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_mpmc_init(size);
        REQUIRE(slots_bitmap_mpmc_get_shard_ptr(slots_bitmap, 0) == slots_bitmap->shards);
        REQUIRE(slots_bitmap_mpmc_get_shard_ptr(slots_bitmap, 1) == &(slots_bitmap->shards[1]));
        slots_bitmap_mpmc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_mpmc_get_shard_used_count_ptr") {
        uint64_t size = 10;
        slots_bitmap_mpmc_t *slots_bitmap = slots_bitmap_mpmc_init(size);

        auto bitmap_ptr = (uintptr_t)slots_bitmap;
        uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_mpmc_t, shards);
        uintptr_t bitmap_shards_used_slots_ptr =
                bitmap_shards_ptr + (sizeof(slots_bitmap_mpmc_shard_t) * 1);

        REQUIRE(slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, 0) ==
            (uint8_t*)bitmap_shards_used_slots_ptr);
        slots_bitmap_mpmc_free(slots_bitmap);

        size = 65;
        slots_bitmap = slots_bitmap_mpmc_init(size);

        bitmap_ptr = (uintptr_t)slots_bitmap;
        bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_mpmc_t, shards);
        bitmap_shards_used_slots_ptr =
                bitmap_shards_ptr + (sizeof(slots_bitmap_mpmc_shard_t) * 2);

        REQUIRE(slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, 0) ==
                (uint8_t*)bitmap_shards_used_slots_ptr);
        slots_bitmap_mpmc_free(slots_bitmap);
    }

    SECTION("slots_bitmap_mpmc_get_next_available_ptr_with_step") {
        slots_bitmap_mpmc_t *bitmap = slots_bitmap_mpmc_init(512);
        uint64_t index;

        SECTION("No space available") {
            for (uint64_t i = 0; i < bitmap->size; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, i / 64) |= ((slots_bitmap_mpmc_shard_t)1 << i);
                *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, i / 64) += 1;
            }

            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == false);
        }

        SECTION("Space available") {
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == true);
            REQUIRE(index == 0);
        }

        SECTION("Multiple available") {
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == true);
            REQUIRE(index == 0);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == true);
            REQUIRE(index == 1);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == true);
            REQUIRE(index == 2);
        }

        SECTION("After a filled shard") {
            for (uint64_t i = 0; i < 64; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, i / 64) |= ((slots_bitmap_mpmc_shard_t)1 << i);
                *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, i / 64) += 1;
            }

            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == true);
            REQUIRE(index == 64);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == true);
            REQUIRE(index == 65);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == true);
            REQUIRE(index == 66);
        }

        SECTION("Start 1 step 1") {
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 1, 1, &index) == true);
            REQUIRE(index == SLOTS_BITMAP_MPMC_SHARD_SIZE + 0);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 1, 1, &index) == true);
            REQUIRE(index == SLOTS_BITMAP_MPMC_SHARD_SIZE + 1);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 1, 1, &index) == true);
            REQUIRE(index == SLOTS_BITMAP_MPMC_SHARD_SIZE + 2);
        }

        SECTION("Start 1 step 2, shard 1 almost filled up") {
            for (uint64_t i = SLOTS_BITMAP_MPMC_SHARD_SIZE; i < SLOTS_BITMAP_MPMC_SHARD_SIZE + 63; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, i / 64) |= ((slots_bitmap_mpmc_shard_t)1 << i);
                *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, i / 64) += 1;
            }
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 0, 1, &index) == true);
            REQUIRE(index == 0);

            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 1, 2, &index) == true);
            REQUIRE(index == (SLOTS_BITMAP_MPMC_SHARD_SIZE * 1) + 63);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 1, 2, &index) == true);
            REQUIRE(index == (SLOTS_BITMAP_MPMC_SHARD_SIZE * 3) + 0);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr_with_step(bitmap, 1, 2, &index) == true);
            REQUIRE(index == (SLOTS_BITMAP_MPMC_SHARD_SIZE * 3) + 1);
        }
    }

    SECTION("slots_bitmap_mpmc_get_next_available_with_step") {
        slots_bitmap_mpmc_t *bitmap = slots_bitmap_mpmc_init(512);

        SECTION("No space available") {
            for (uint64_t i = 0; i < bitmap->size; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, i / 64) |= ((slots_bitmap_mpmc_shard_t)1 << i);
                *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, i / 64) += 1;
            }

            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == UINT64_MAX);
        }

        SECTION("Space available") {
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == 0);
        }

        SECTION("Multiple available") {
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == 0);
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == 1);
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == 2);
        }

        SECTION("After a filled shard") {
            for (uint64_t i = 0; i < 64; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, i / 64) |= ((slots_bitmap_mpmc_shard_t)1 << i);
                *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, i / 64) += 1;
            }

            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == 64);
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == 65);
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == 66);
        }

        SECTION("Start 1 step 1") {
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 1, 1) == SLOTS_BITMAP_MPMC_SHARD_SIZE + 0);
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 1, 1) == SLOTS_BITMAP_MPMC_SHARD_SIZE + 1);
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 1, 1) == SLOTS_BITMAP_MPMC_SHARD_SIZE + 2);
        }

        SECTION("Start 1 step 2, shard 1 almost filled up") {
            for (uint64_t i = SLOTS_BITMAP_MPMC_SHARD_SIZE; i < SLOTS_BITMAP_MPMC_SHARD_SIZE + 63; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, i / 64) |= ((slots_bitmap_mpmc_shard_t)1 << i);
                *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, i / 64) += 1;
            }
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 0, 1) == 0);

            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 1, 2) == (SLOTS_BITMAP_MPMC_SHARD_SIZE * 1) + 63);
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 1, 2) == (SLOTS_BITMAP_MPMC_SHARD_SIZE * 3) + 0);
            REQUIRE(slots_bitmap_mpmc_get_next_available_with_step(bitmap, 1, 2) == (SLOTS_BITMAP_MPMC_SHARD_SIZE * 3) + 1);
        }
    }

    SECTION("slots_bitmap_mpmc_get_next_available_ptr") {
        slots_bitmap_mpmc_t *bitmap = slots_bitmap_mpmc_init(128);
        uint64_t index;

        SECTION("No space available") {
            for (uint64_t i = 0; i < 128; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, 0) |= ((slots_bitmap_mpmc_shard_t)1 << i);
            }
            *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, 0) = 64;
            *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, 1) = 64;

            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr(bitmap, &index) == false);
        }

        SECTION("Space available") {
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr(bitmap, &index) == true);
            REQUIRE(index == 0);
        }

        SECTION("Multiple available") {
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr(bitmap, &index) == true);
            REQUIRE(index == 0);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr(bitmap, &index) == true);
            REQUIRE(index == 1);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr(bitmap, &index) == true);
            REQUIRE(index == 2);
        }

        SECTION("After a filled shard") {
            for (uint64_t i = 0; i < 128; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, 0) |= ((slots_bitmap_mpmc_shard_t)1 << i);
            }
            *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, 0) = 64;

            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr(bitmap, &index) == true);
            REQUIRE(index == 64);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr(bitmap, &index) == true);
            REQUIRE(index == 65);
            REQUIRE(slots_bitmap_mpmc_get_next_available_ptr(bitmap, &index) == true);
            REQUIRE(index == 66);
        }

        slots_bitmap_mpmc_free(bitmap);
    }

    SECTION("slots_bitmap_mpmc_get_next_available") {
        slots_bitmap_mpmc_t *bitmap = slots_bitmap_mpmc_init(64);

        SECTION("No space available") {
            for (uint64_t i = 0; i < 64; i++) {
                *slots_bitmap_mpmc_get_shard_ptr(bitmap, 0) |= ((slots_bitmap_mpmc_shard_t)1 << i);
                *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, 0) = 64;
            }

            REQUIRE(slots_bitmap_mpmc_get_next_available(bitmap) == UINT64_MAX);
        }

        SECTION("Space available") {
            REQUIRE(slots_bitmap_mpmc_get_next_available(bitmap) == 0);
        }

        slots_bitmap_mpmc_free(bitmap);
    }

    SECTION("slots_bitmap_mpmc_release") {
        const uint64_t size = 64;
        slots_bitmap_mpmc_t *bitmap = slots_bitmap_mpmc_init(size);

        SECTION("When the shard bit is already set to 0") {
            uint64_t slots_bitmap_index = 0;
            *slots_bitmap_mpmc_get_shard_ptr(bitmap, 0) = 0;
            *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, 0) = 0;

            slots_bitmap_mpmc_release(bitmap, slots_bitmap_index);

            REQUIRE(bitmap->shards_used_slots[0] == 0);
            REQUIRE(*slots_bitmap_mpmc_get_shard_ptr(bitmap, 0) == 0);
        }

        SECTION("When the shard bit is set to 1") {
            uint64_t slots_bitmap_index = 0;
            *slots_bitmap_mpmc_get_shard_ptr(bitmap, 0) = 1;
            *slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, 0) = 1;

            slots_bitmap_mpmc_release(bitmap, slots_bitmap_index);

            REQUIRE(bitmap->shards_used_slots[0] == 0);
            REQUIRE(*slots_bitmap_mpmc_get_shard_ptr(bitmap, 0) == 0);
        }

        slots_bitmap_mpmc_free(bitmap);
    }
}