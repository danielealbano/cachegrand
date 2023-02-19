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
#include <string.h>
#include <stdatomic.h>
#include <assert.h>

#include "exttypes.h"
#include "memory_fences.h"
#include "fatal.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "slots_bitmap_spsc.h"

#define TAG "slots_bitmap_spsc"

uint64_t slots_bitmap_spsc_calculate_shard_count(
        uint64_t size) {
    return (size / SLOTS_BITMAP_SPSC_SHARD_SIZE) + 1;
}

slots_bitmap_spsc_t *slots_bitmap_spsc_init(
        uint64_t size) {

    size_t shards_count = slots_bitmap_spsc_calculate_shard_count(size);
    size_t allocation_size =
            sizeof(slots_bitmap_spsc_t) +
            (sizeof(slots_bitmap_spsc_shard_t) * shards_count) +
            (sizeof(uint8_t) * shards_count);

    slots_bitmap_spsc_t *bitmap = (slots_bitmap_spsc_t *)ffma_mem_alloc(allocation_size);

    if (bitmap == NULL) {
        return NULL;
    }

    memset(bitmap, 0, allocation_size);

    bitmap->size = size;
    bitmap->shards_count = shards_count;

    return bitmap;
}

void slots_bitmap_spsc_free(
        slots_bitmap_spsc_t *bitmap) {
    ffma_mem_free(bitmap);
}

slots_bitmap_spsc_shard_t* slots_bitmap_spsc_get_shard_ptr(
        slots_bitmap_spsc_t *bitmap,
        uint64_t shard_index) {

    return (slots_bitmap_spsc_shard_t*)
                   bitmap +
           offsetof(slots_bitmap_spsc_t, shards) +
           (sizeof(slots_bitmap_spsc_shard_t) * shard_index);
}

uint8_t* slots_bitmap_spsc_get_shard_used_count_ptr(
        slots_bitmap_spsc_t *bitmap,
        uint64_t shard_index) {

    return (uint8_t*)
                   bitmap +
           offsetof(slots_bitmap_spsc_t, shards) +
           (sizeof(slots_bitmap_spsc_shard_t) * bitmap->shards_count) +
           (sizeof(uint8_t) * shard_index);
}

bool slots_bitmap_spsc_get_first_available(
        slots_bitmap_spsc_t *bitmap,
        uint64_t *return_bitmap_index) {
    uint8_t *shard_used_count = slots_bitmap_spsc_get_shard_used_count_ptr(bitmap, 0);
    for(uint64_t bitmap_shard_index = 0; bitmap_shard_index < bitmap->shards_count; bitmap_shard_index++) {
        if (shard_used_count[bitmap_shard_index] == SLOTS_BITMAP_SPSC_SHARD_SIZE) {
            continue;
        }

        slots_bitmap_spsc_shard_t *bitmap_shard_ptr =
                slots_bitmap_spsc_get_shard_ptr(bitmap, bitmap_shard_index);
        for(
                uint64_t bitmap_shard_bit_index = 0;
                bitmap_shard_bit_index < SLOTS_BITMAP_SPSC_SHARD_SIZE;
                bitmap_shard_bit_index++) {
            uint64_t value =
                    (*bitmap_shard_ptr >> bitmap_shard_bit_index) & (SLOTS_BITMAP_SPSC_SHARD_DATA_TYPE)0x01;

            // If the shard bit is already set, skip the bit
            if (value != 0) {
                continue;
            }

            // Update the counter
            shard_used_count[bitmap_shard_index]++;

            // Calculate the new value of the shard
            *bitmap_shard_ptr =
                    *bitmap_shard_ptr | ((slots_bitmap_spsc_shard_t)1 << bitmap_shard_bit_index);

            // Update the return value
            *return_bitmap_index = (bitmap_shard_index * SLOTS_BITMAP_SPSC_SHARD_SIZE) + bitmap_shard_bit_index;

            return true;
        }

        // Should never ever get here as the shard_used_count was initially smaller than
        // SLOTS_BITMAP_SPSC_SHARD_SIZE and therefore there should be at least one bit available
        assert(false);
    }

    return false;
}

void slots_bitmap_spsc_release(
        slots_bitmap_spsc_t *bitmap,
        uint64_t bitmap_index) {
    uint8_t *shard_used_count = slots_bitmap_spsc_get_shard_used_count_ptr(bitmap, 0);
    uint16_t bitmap_shard_index = bitmap_index / SLOTS_BITMAP_SPSC_SHARD_SIZE;
    uint8_t bitmap_shard_bit_index = bitmap_index % SLOTS_BITMAP_SPSC_SHARD_SIZE;

    // Update the counter
    shard_used_count[bitmap_shard_index]--;

    // Update the shard value
    slots_bitmap_spsc_shard_t *bitmap_shard_ptr = slots_bitmap_spsc_get_shard_ptr(bitmap, bitmap_shard_index);
    *bitmap_shard_ptr = *bitmap_shard_ptr - ((SLOTS_BITMAP_SPSC_SHARD_DATA_TYPE)1 << bitmap_shard_bit_index);
}
