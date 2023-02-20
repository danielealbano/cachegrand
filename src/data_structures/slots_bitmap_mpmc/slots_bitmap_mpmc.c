/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "fatal.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "slots_bitmap_mpmc.h"

#define TAG "slots_bitmap_mpmc"

uint64_t slots_bitmap_mpmc_calculate_shard_count(
        uint64_t size) {
    return (uint64_t)ceil((double)size / (double)SLOTS_BITMAP_MPMC_SHARD_SIZE);
}

slots_bitmap_mpmc_t *slots_bitmap_mpmc_init(
        uint64_t size) {
    size_t shards_count = slots_bitmap_mpmc_calculate_shard_count(size);
    size_t allocation_size =
            sizeof(slots_bitmap_mpmc_t) +
            (sizeof(slots_bitmap_mpmc_shard_t) * shards_count) +
            (sizeof(uint8_t) * shards_count);

    slots_bitmap_mpmc_t *bitmap = (slots_bitmap_mpmc_t *)ffma_mem_alloc(allocation_size);

    if (bitmap == NULL) {
        return NULL;
    }

    memset(bitmap, 0, allocation_size);

    bitmap->size = size;
    bitmap->shards_count = shards_count;

    return bitmap;
}

void slots_bitmap_mpmc_free(
        slots_bitmap_mpmc_t *bitmap) {
    ffma_mem_free(bitmap);
}

slots_bitmap_mpmc_shard_t* slots_bitmap_mpmc_get_shard_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t shard_index) {
    uintptr_t bitmap_ptr = (uintptr_t)bitmap;
    uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_mpmc_t, shards);
    uintptr_t bitmap_shard_ptr = bitmap_shards_ptr + (sizeof(slots_bitmap_mpmc_shard_t) * shard_index);

    return (slots_bitmap_mpmc_shard_t*)bitmap_shard_ptr;
}

uint8_t* slots_bitmap_mpmc_get_shard_used_count_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t shard_index) {
    uintptr_t bitmap_ptr = (uintptr_t)bitmap;
    uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_mpmc_t, shards);
    uintptr_t bitmap_shards_used_slots_ptr =
            bitmap_shards_ptr + (sizeof(slots_bitmap_mpmc_shard_t) * bitmap->shards_count);
    uintptr_t bitmap_shard_used_slots_ptr =
            bitmap_shards_used_slots_ptr + (sizeof(uint8_t) * shard_index);

    return (uint8_t*)bitmap_shard_used_slots_ptr;
}

bool slots_bitmap_mpmc_get_next_available_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t *return_slots_bitmap_index) {
    bool should_restart_loop = false;
    uint8_t *shard_used_count = slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, 0);

    do {
        should_restart_loop = false;

        for(uint64_t bitmap_shard_index = 0; bitmap_shard_index < bitmap->shards_count; bitmap_shard_index++) {
            MEMORY_FENCE_LOAD();
            if (unlikely(shard_used_count[bitmap_shard_index] == SLOTS_BITMAP_MPMC_SHARD_SIZE)) {
                continue;
            }

            bool found = false;
            slots_bitmap_mpmc_shard_t *bitmap_shard_ptr = slots_bitmap_mpmc_get_shard_ptr(bitmap, bitmap_shard_index);
            for(
                    uint64_t bitmap_shard_bit_index = 0;
                    bitmap_shard_bit_index < SLOTS_BITMAP_MPMC_SHARD_SIZE;
                    bitmap_shard_bit_index++) {
                MEMORY_FENCE_LOAD();
                slots_bitmap_mpmc_shard_t bitmap_shard = *bitmap_shard_ptr;
                uint64_t value =
                        (bitmap_shard >> bitmap_shard_bit_index) & (SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE)0x01;

                // If the shard bit is already set, skip the bit
                if (value != 0) {
                    continue;
                }

                // Calculate the new value of the shard
                slots_bitmap_mpmc_shard_t bitmap_shard_new =
                        *bitmap_shard_ptr | ((slots_bitmap_mpmc_shard_t)1 << bitmap_shard_bit_index);

                // Try to set the bit to 1
                if (!__atomic_compare_exchange_n(
                        bitmap_shard_ptr,
                        (SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE*)&bitmap_shard,
                        bitmap_shard_new,
                        true,
                        __ATOMIC_ACQ_REL,
                        __ATOMIC_ACQUIRE)) {
                    // The CAS operation failed, another thread might have changed the bitmap shard, try again.
                    // For performance reasons the search doesn't start from the beginning again but instead continue
                    // looking at the next bit (or shard) but set the should_restart_loop flag to true so the loop
                    // will restart from the beginning if nothing is found.
                    should_restart_loop = true;
                    continue;
                }

                // Update the counter
                __atomic_fetch_add(&shard_used_count[bitmap_shard_index], 1, __ATOMIC_ACQ_REL);

                // The CAS operation succeeded, the bit has been set to 1, return the bitmap index
                *return_slots_bitmap_index = (bitmap_shard_index * SLOTS_BITMAP_MPMC_SHARD_SIZE) + bitmap_shard_bit_index;

                return true;
            }

            // If the initial expectation was that there should have been space but there wasn't
            if (unlikely(!found)) {
                break;
            }
        }
    } while(should_restart_loop);

    return false;
}

uint64_t slots_bitmap_mpmc_get_next_available(
        slots_bitmap_mpmc_t *slots_bitmap) {
    uint64_t index;

    if (slots_bitmap_mpmc_get_next_available_ptr(slots_bitmap, &index)) {
        return index;
    }

    return UINT64_MAX;
}

void slots_bitmap_mpmc_release(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t slots_bitmap_index) {
    uint8_t *shard_used_count = slots_bitmap_mpmc_get_shard_used_count_ptr(bitmap, 0);
    uint16_t bitmap_shard_index = slots_bitmap_index / SLOTS_BITMAP_MPMC_SHARD_SIZE;
    uint8_t bitmap_shard_bit_index = slots_bitmap_index % SLOTS_BITMAP_MPMC_SHARD_SIZE;

    // Update the shard value
    slots_bitmap_mpmc_shard_t bitmap_shard;
    slots_bitmap_mpmc_shard_t bitmap_shard_new;
    slots_bitmap_mpmc_shard_t *bitmap_shard_ptr = slots_bitmap_mpmc_get_shard_ptr(bitmap, bitmap_shard_index);

    do {
        MEMORY_FENCE_LOAD();
        bitmap_shard = *bitmap_shard_ptr;
        uint64_t value =
                (bitmap_shard >> bitmap_shard_bit_index) & (SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE)0x01;

        // If the shard bit is already set to 0, break the loop so the outer loop can move forward
        if (unlikely(value == 0)) {
            break;
        }

        // Calculate the new value of the shard
        bitmap_shard_new =
                bitmap_shard - ((SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE)1 << bitmap_shard_bit_index);
    } while (!__atomic_compare_exchange_n(
            bitmap_shard_ptr,
            (SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE*)&bitmap_shard,
            bitmap_shard_new,
            true,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE));

    // Update the counter
    __atomic_fetch_sub(&shard_used_count[bitmap_shard_index], 1, __ATOMIC_ACQ_REL);
}
