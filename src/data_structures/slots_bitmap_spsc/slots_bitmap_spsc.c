/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "exttypes.h"
#include "memory_fences.h"
#include "fatal.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"

#include "slots_bitmap_spsc_first_free_bit_table.h"
#include "slots_bitmap_spsc.h"

#define TAG "slots_bitmap_spsc"

uint64_t slots_bitmap_spsc_calculate_shard_count(
        uint64_t size) {
    return (uint64_t)ceil((double)size / (double)SLOTS_BITMAP_SPSC_SHARD_SIZE);
}

uint64_t slots_bitmap_spsc_calculate_shard_full_count(
        uint64_t shards_count) {
    return (uint64_t)ceil((double)shards_count / (double)sizeof(uint64_t));
}

slots_bitmap_spsc_t *slots_bitmap_spsc_init(
        uint64_t size) {
    if (size == 0) {
        return NULL;
    }

    // Calculate the number of shards needed to store the given size
    size_t shards_count = slots_bitmap_spsc_calculate_shard_count(size);

    // Allocate memory for the bitmap, shards, and used slots
    size_t allocation_size =
            sizeof(slots_bitmap_spsc_t) +
            (sizeof(slots_bitmap_spsc_shard_t) * shards_count) +
            (sizeof(uint8_t) * shards_count) +
            (sizeof(uint64_t) * (size_t)slots_bitmap_spsc_calculate_shard_full_count(shards_count));

    slots_bitmap_spsc_t *slots_bitmap = (slots_bitmap_spsc_t *)ffma_mem_alloc(allocation_size);

    // If memory allocation fails, return NULL
    if (slots_bitmap == NULL) {
        return NULL;
    }

    // Initialize the bitmap, set its size and shards count
    memset(slots_bitmap, 0, allocation_size);

    slots_bitmap->size = shards_count * SLOTS_BITMAP_SPSC_SHARD_SIZE;
    slots_bitmap->shards_count = shards_count;

    return slots_bitmap;
}

void slots_bitmap_spsc_free(
        slots_bitmap_spsc_t *slots_bitmap) {
    // Free the memory allocated for the bitmap
    ffma_mem_free(slots_bitmap);
}

slots_bitmap_spsc_shard_t* slots_bitmap_spsc_get_shard_ptr(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_index) {
    uintptr_t bitmap_ptr = (uintptr_t)slots_bitmap;
    uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_spsc_t, shards);
    uintptr_t bitmap_shard_ptr = bitmap_shards_ptr + (sizeof(slots_bitmap_spsc_shard_t) * shard_index);

    return (slots_bitmap_spsc_shard_t*)bitmap_shard_ptr;
}

uint8_t* slots_bitmap_spsc_get_shard_used_count_ptr(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_index) {
    uintptr_t bitmap_ptr = (uintptr_t)slots_bitmap;
    uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_spsc_t, shards);
    uintptr_t bitmap_shards_used_slots_ptr =
            bitmap_shards_ptr + (sizeof(slots_bitmap_spsc_shard_t) * slots_bitmap->shards_count);
    uintptr_t bitmap_shard_used_slots_ptr =
            bitmap_shards_used_slots_ptr + (sizeof(uint8_t) * shard_index);

    return (uint8_t*)bitmap_shard_used_slots_ptr;
}

uint64_t* slots_bitmap_spsc_get_shard_full_ptr(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_full_index) {
    uintptr_t bitmap_ptr = (uintptr_t)slots_bitmap;
    uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_spsc_t, shards);
    uintptr_t bitmap_shards_used_slots_ptr =
            bitmap_shards_ptr + (sizeof(slots_bitmap_spsc_shard_t) * slots_bitmap->shards_count);
    uintptr_t bitmap_shard_full_ptr =
            bitmap_shards_used_slots_ptr + (sizeof(uint8_t) * slots_bitmap->shards_count);
    uintptr_t bitmap_shard_used_slots_ptr =
            bitmap_shard_full_ptr + (sizeof(uint64_t) * shard_full_index);

    return (uint64_t*)bitmap_shard_used_slots_ptr;
}

void slots_bitmap_spsc_set_shard_full_bit(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_index,
        bool full) {
    uint64_t shards_full_index = shard_index / sizeof(slots_bitmap->shards_full[0]);
    uint64_t shards_full_bit_index = shard_index % sizeof(slots_bitmap->shards_full[0]);

    if (full) {
        *slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, shards_full_index) |= (1 << shards_full_bit_index);
    } else {
        *slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, shards_full_index) &= ~(1 << shards_full_bit_index);
    }
}

bool slots_bitmap_spsc_is_shard_full(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_index) {
    uint64_t *bitmap_shards_full_base_ptr = slots_bitmap_spsc_get_shard_full_ptr(slots_bitmap, 0);

    uint64_t shard_full_index = shard_index / sizeof(slots_bitmap->shards_full[0]);
    uint64_t shard_full_bit_index = shard_index % sizeof(slots_bitmap->shards_full[0]);
    bool full = (bitmap_shards_full_base_ptr[shard_full_index] >> shard_full_bit_index) & (uint16_t)0x01;

    return full;
}

uint8_t slots_bitmap_spsc_shard_find_first_zero(
        slots_bitmap_spsc_shard_t slots_bitmap_shard) {
    for(int16_t uint16_bytes_index = 0; uint16_bytes_index < 8; uint16_bytes_index += 2) {
        uint16_t uint16_bytes = (uint16_t)((slots_bitmap_shard >> (uint16_bytes_index * 8)) & 0xFFFF);
        uint8_t first_free_zero_bit_index = slots_bitmap_spsc_first_free_bit_table[uint16_bytes];

        if (first_free_zero_bit_index == UINT8_MAX) {
            continue;
        }

        return first_free_zero_bit_index + (uint16_bytes_index * 8);
    }

    return UINT8_MAX;
}

bool slots_bitmap_spsc_get_next_available_ptr(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t *return_slots_bitmap_index) {
    uint8_t *shard_used_count_base_ptr = slots_bitmap_spsc_get_shard_used_count_ptr(slots_bitmap, 0);
    slots_bitmap_spsc_shard_t *bitmap_shard_base_ptr = slots_bitmap_spsc_get_shard_ptr(slots_bitmap, 0);

    for(uint64_t shard_index = 0; shard_index < slots_bitmap->shards_count; shard_index++) {
        if (slots_bitmap_spsc_is_shard_full(slots_bitmap, shard_index)) {
            continue;
        }

        slots_bitmap_spsc_shard_t *bitmap_shard_ptr = &bitmap_shard_base_ptr[shard_index];
        int16_t bitmap_shard_bit_index = slots_bitmap_spsc_shard_find_first_zero(*bitmap_shard_ptr);

        // There should always be a bit available if the shard is not full
        assert(bitmap_shard_bit_index < UINT8_MAX);

        // Update the counter
        shard_used_count_base_ptr[shard_index]++;

        // Calculate the new value of the shard
        *bitmap_shard_ptr = *bitmap_shard_ptr | ((slots_bitmap_spsc_shard_t)1 << bitmap_shard_bit_index);

        // Update the return value
        *return_slots_bitmap_index = (shard_index * SLOTS_BITMAP_SPSC_SHARD_SIZE) + bitmap_shard_bit_index;

        // If the shard is full after acquiring this bit, update the shards_full bitmap
        if (shard_used_count_base_ptr[shard_index] == SLOTS_BITMAP_SPSC_SHARD_SIZE) {
            slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, shard_index, true);
        }

        return true;
    }

    return false;
}

uint64_t slots_bitmap_spsc_get_next_available(
        slots_bitmap_spsc_t *slots_bitmap) {
    uint64_t index;

    if (slots_bitmap_spsc_get_next_available_ptr(slots_bitmap, &index)) {
        return index;
    }

    return UINT64_MAX;
}

void slots_bitmap_spsc_release(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t bitmap_index) {
    uint8_t *shard_used_count = slots_bitmap_spsc_get_shard_used_count_ptr(slots_bitmap, 0);
    uint16_t shard_index = bitmap_index / SLOTS_BITMAP_SPSC_SHARD_SIZE;
    uint8_t bitmap_shard_bit_index = bitmap_index % SLOTS_BITMAP_SPSC_SHARD_SIZE;

    // Update the counter
    shard_used_count[shard_index]--;
    slots_bitmap_spsc_set_shard_full_bit(slots_bitmap, shard_index, false);

    // Update the shard value
    slots_bitmap_spsc_shard_t *bitmap_shard_ptr = slots_bitmap_spsc_get_shard_ptr(slots_bitmap, shard_index);
    *bitmap_shard_ptr = *bitmap_shard_ptr - ((slots_bitmap_spsc_shard_t)1 << bitmap_shard_bit_index);
}
