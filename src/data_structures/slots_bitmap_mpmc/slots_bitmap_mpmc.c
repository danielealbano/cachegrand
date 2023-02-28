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

#include "slots_bitmap_mpmc_first_free_bit_table.h"
#include "slots_bitmap_mpmc.h"

#define TAG "slots_bitmap_mpmc"

uint64_t slots_bitmap_mpmc_calculate_shard_count(
        uint64_t size) {
    // ceil function is used to round up to the nearest integer the result of the division
    return (uint64_t)ceil((double)size / (double)SLOTS_BITMAP_MPMC_SHARD_SIZE);
}

slots_bitmap_mpmc_t *slots_bitmap_mpmc_init(
        uint64_t size) {
    if (size == 0) {
        return NULL;
    }

    // Calculate the number of shards needed to store the given size
    size_t shards_count = slots_bitmap_mpmc_calculate_shard_count(size);

    // Allocate memory for the concurrent bitmap, shards, and used slots
    size_t allocation_size =
            sizeof(slots_bitmap_mpmc_t) +
            (sizeof(slots_bitmap_mpmc_shard_t) * shards_count) +
            (sizeof(uint8_t) * shards_count);

    slots_bitmap_mpmc_t *slots_bitmap = (slots_bitmap_mpmc_t *)ffma_mem_alloc(allocation_size);

    // If memory allocation fails, return NULL
    if (slots_bitmap == NULL) {
        return NULL;
    }

    // Initialize the bitmap, set its size and shards count
    memset(slots_bitmap, 0, allocation_size);
    slots_bitmap->size = shards_count * SLOTS_BITMAP_MPMC_SHARD_SIZE;
    slots_bitmap->shards_count = shards_count;

    return slots_bitmap;
}

void slots_bitmap_mpmc_free(
        slots_bitmap_mpmc_t *bitmap) {
    // Free the memory allocated for the concurrent bitmap
    ffma_mem_free(bitmap);
}

slots_bitmap_mpmc_shard_t* slots_bitmap_mpmc_get_shard_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t shard_index) {
    // Calculate the address of the specified shard and return a pointer to it
    uintptr_t bitmap_ptr = (uintptr_t)bitmap;
    uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_mpmc_t, shards);
    uintptr_t bitmap_shard_ptr = bitmap_shards_ptr + (sizeof(slots_bitmap_mpmc_shard_t) * shard_index);

    // Return a pointer to the shard
    return (slots_bitmap_mpmc_shard_t*)bitmap_shard_ptr;
}

uint8_t* slots_bitmap_mpmc_get_shard_used_count_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t shard_index) {
    // Calculate the address of the specified shard's used count and return a pointer to it
    uintptr_t bitmap_ptr = (uintptr_t)bitmap;
    uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_mpmc_t, shards);
    uintptr_t bitmap_shards_used_slots_ptr =
            bitmap_shards_ptr + (sizeof(slots_bitmap_mpmc_shard_t) * bitmap->shards_count);
    uintptr_t bitmap_shard_used_slots_ptr =
            bitmap_shards_used_slots_ptr + (sizeof(uint8_t) * shard_index);

    // Return a pointer to the shard's used count
    return (uint8_t*)bitmap_shard_used_slots_ptr;
}

uint8_t slots_bitmap_mpmc_shard_find_first_zero(
        slots_bitmap_mpmc_shard_t slots_bitmap_shard) {
    // Iterate over the 8 bytes of the shard
    for(int16_t uint16_bytes_index = 0; uint16_bytes_index < 8; uint16_bytes_index += 2) {
        // Get the 2 bytes of the shard
        uint16_t uint16_bytes = (uint16_t)((slots_bitmap_shard >> (uint16_bytes_index * 8)) & 0xFFFF);

        // Fetch from the mapping table the index of the first zero bit
        uint8_t first_free_zero_bit_index = slots_bitmap_mpmc_first_free_bit_table[uint16_bytes];

        // If the index is UINT8_MAX, then there are no zero bits in the 2 bytes
        if (unlikely(first_free_zero_bit_index == UINT8_MAX)) {
            continue;
        }

        // Return the index of the first zero bit
        return first_free_zero_bit_index + (uint16_bytes_index * 8);
    }

    // If no zero bits were found, return UINT8_MAX
    return UINT8_MAX;
}

bool slots_bitmap_mpmc_shard_is_full(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index) {
    // Get the shard's used count
    uint8_volatile_t *shard_used_count_ptr = slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, shard_index);
    MEMORY_FENCE_LOAD();
    uint8_volatile_t shard_used_count = *shard_used_count_ptr;

    // If the shard's used count is equal to the shard's size, then the shard is full
    return shard_used_count == SLOTS_BITMAP_MPMC_SHARD_SIZE;
}

void slots_bitmap_mpmc_shard_increase_used_count(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index) {
    // Get the shard's used count
    uint8_volatile_t *shard_used_count_ptr =
            slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, shard_index);

    // Increase the shard's used count
    __atomic_fetch_add(shard_used_count_ptr, 1, __ATOMIC_ACQ_REL);
}

void slots_bitmap_mpmc_shard_decrease_used_count(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index) {
    // Get the shard's used count
    uint8_volatile_t *shard_used_count_ptr =
            slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, shard_index);

    // Increase the shard's used count
    __atomic_fetch_sub(shard_used_count_ptr, 1, __ATOMIC_ACQ_REL);
}

bool slots_bitmap_mpmc_get_next_available_ptr_with_step(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint16_t shard_index_start,
        uint16_t shard_index_step,
        uint64_t *return_slots_bitmap_index) {
    bool should_restart_loop = false;
    // Get a pointer to the first shard
    slots_bitmap_mpmc_shard_t *bitmap_shard_base_ptr = slots_bitmap_mpmc_get_shard_ptr(slots_bitmap, 0);

    // For performance reasons if a shard is initially found free but later was when it tries to be update the operation
    // fail because another thread has changed it, instead of trying again to search on that one it moves forward to
    // search for another slot in another shard.
    // This approach will reduce the amount of concurrency issues and improve performance, but it will also increase
    // the probability of having to restart the loop.
    do {
        // Reset the restart loop flag
        should_restart_loop = false;

        // Iterate over all shards
        for(
                uint64_t shard_index = shard_index_start;
                likely(shard_index < slots_bitmap->shards_count);
                shard_index += shard_index_step) {
            MEMORY_FENCE_LOAD();

            // Check if the shard is full
            if (unlikely(slots_bitmap_mpmc_shard_is_full(slots_bitmap, shard_index))) {
                continue;
            }

            // Fetch the shard pointer and try to find a free slot
            slots_bitmap_mpmc_shard_t *shard_ptr = &bitmap_shard_base_ptr[shard_index];
            slots_bitmap_mpmc_shard_t shard_value = *shard_ptr;
            uint8_t shard_bit_index = slots_bitmap_mpmc_shard_find_first_zero(shard_value);

            // Check if the find first zero function was able to find a free slot
            if (unlikely(shard_bit_index == UINT8_MAX)) {
                continue;
            }

            // Calculate the new value of the shard
            slots_bitmap_mpmc_shard_t shard_value_new =
                    *shard_ptr | ((slots_bitmap_mpmc_shard_t)1 << shard_bit_index);

            // Try to set the bit to 1
            if (unlikely(!__atomic_compare_exchange_n(
                    shard_ptr,
                    (SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE*)&shard_value,
                    shard_value_new,
                    true,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_ACQUIRE))) {
                // The CAS operation failed, another thread might have changed the bitmap shard, try again.
                // For performance reasons the search doesn't start from the beginning again but instead continue
                // looking at the next bit (or shard) but set the should_restart_loop flag to true so the loop
                // will restart from the beginning if nothing is found.
                should_restart_loop = true;
                continue;
            }

            // Update the counter
            slots_bitmap_mpmc_shard_increase_used_count(slots_bitmap, shard_index);

            // The CAS operation succeeded, the bit has been set to 1, return the bitmap index
            *return_slots_bitmap_index = (shard_index * SLOTS_BITMAP_MPMC_SHARD_SIZE) + shard_bit_index;

            return true;
        }
    } while(likely(should_restart_loop));

    return false;
}

uint64_t slots_bitmap_mpmc_get_next_available_with_step(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint16_t shard_index_start,
        uint16_t shard_index_step) {
    uint64_t index;

    // Try to get the next available slot
    if (likely(slots_bitmap_mpmc_get_next_available_ptr_with_step(
            slots_bitmap,
            shard_index_start,
            shard_index_step,
            &index))) {
        // Return the index
        return index;
    }

    // No slot was found, return UINT64_MAX
    return UINT64_MAX;
}

bool slots_bitmap_mpmc_get_next_available_ptr(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t *return_slots_bitmap_index) {
    return slots_bitmap_mpmc_get_next_available_ptr_with_step(
            slots_bitmap,
            0,
            1,
            return_slots_bitmap_index);
}

uint64_t slots_bitmap_mpmc_get_next_available(
        slots_bitmap_mpmc_t *slots_bitmap) {
    uint64_t index;

    // Try to get the next available slot
    if (likely(slots_bitmap_mpmc_get_next_available_ptr(slots_bitmap, &index))) {
        // Return the index
        return index;
    }

    // No slot was found, return UINT64_MAX
    return UINT64_MAX;
}

void slots_bitmap_mpmc_release(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t slots_bitmap_index) {
    uint16_t shard_index = slots_bitmap_index / SLOTS_BITMAP_MPMC_SHARD_SIZE;
    uint8_t shard_bit_index = slots_bitmap_index % SLOTS_BITMAP_MPMC_SHARD_SIZE;

    // Update the shard value
    slots_bitmap_mpmc_shard_t shard_value;
    slots_bitmap_mpmc_shard_t shard_value_new;
    slots_bitmap_mpmc_shard_t *shard_ptr = slots_bitmap_mpmc_get_shard_ptr(bitmap, shard_index);

    // Other threads might try to change the shard value, so the loop will try to set the bit to 0 until it succeeds
    // using the CAS operation.
    do {
        MEMORY_FENCE_LOAD();

        // Get the shard value
        shard_value = *shard_ptr;

        // Get the current value of the shard bit
        uint64_t value =
                (shard_value >> shard_bit_index) & (SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE)0x01;

        // If the shard bit is already set to 0, break the loop so the outer loop can move forward
        if (unlikely(value == 0)) {
            return;
        }

        // Calculate the new value of the shard
        shard_value_new = shard_value - ((SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE)1 << shard_bit_index);
    } while (unlikely(!__atomic_compare_exchange_n(
            shard_ptr,
            (SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE*)&shard_value,
            shard_value_new,
            true,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)));

    // Update the counter
    slots_bitmap_mpmc_shard_decrease_used_count(bitmap, shard_index);
}
