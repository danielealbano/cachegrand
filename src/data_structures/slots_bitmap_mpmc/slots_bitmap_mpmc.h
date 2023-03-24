#ifndef CACHEGRAND_SLOTS_BITMAP_MPMC_H
#define CACHEGRAND_SLOTS_BITMAP_MPMC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "slots_bitmap_mpmc_first_free_bit_table.h"

#define SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE           uint64_t
#define SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE_VOLATILE  uint64_volatile_t
#define SLOTS_BITMAP_MPMC_SHARD_SIZE                (sizeof(SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE) * 8)

typedef SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE_VOLATILE slots_bitmap_mpmc_shard_t;

/**
 * Bitmap structure
 *
 * The bitmap is divided in shards, each shard is a 64 bit integer and the amount of occupied slots is tracked in
 * shards_used_slots. When a shard is full, the corresponding value in shards_used_slots will match the amount of
 * available slots in the shard (SLOTS_BITMAP_MPMC_SHARD_SIZE).
 * The algorithm will use check shards_used_slots to find the first shard that is not full and then search a free slot
 * in that shard, although the shard may be full by the time the slot is found because another thread may have
 * allocated a slot in the same shard.
 * The algorithm uses CAS operations to update the shard, if the CAS fails the shard the algorithm will stop checking
 * that shard and will move to the next one marking that it might have to restart from the beginning if nothing is
 * found.
 * When a free bit is found the counter in shards_used_slots is incremented and the new value will match the amount of
 * available slots in the shard then the shard is marked as full.
 */
typedef struct slots_bitmap_mpmc slots_bitmap_mpmc_t;
struct slots_bitmap_mpmc {
    size_t size;
    size_t shards_count;
    slots_bitmap_mpmc_shard_t shards[0];
    uint8_volatile_t shards_used_slots[0];
};

/**
 * Get the pointer to the shard
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return Pointer to the shard
 */
static inline slots_bitmap_mpmc_shard_t* slots_bitmap_mpmc_get_shard_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t shard_index) {
    // Calculate the address of the specified shard and return a pointer to it
    uintptr_t bitmap_ptr = (uintptr_t)bitmap;
    uintptr_t bitmap_shards_ptr = bitmap_ptr + offsetof(slots_bitmap_mpmc_t, shards);
    uintptr_t bitmap_shard_ptr = bitmap_shards_ptr + (sizeof(slots_bitmap_mpmc_shard_t) * shard_index);

    // Return a pointer to the shard
    return (slots_bitmap_mpmc_shard_t*)bitmap_shard_ptr;
}

/**
 * Get the pointer to the shard used count of a shard
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return Pointer to the shard used count
 */
static inline uint8_t* slots_bitmap_mpmc_get_shard_used_count_ptr(
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

/**
 * Find the first zero bit in the given shard
 *
 * @param slots_bitmap_shard Shard to search
 * @return The index of the first zero bit or UINT8_MAX if no zero bit is found
 */
static inline uint8_t slots_bitmap_mpmc_shard_find_first_zero(
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

/**
 * Check if the given shard is full. A shard is full if the number of used slots is equal to the shard's size.
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return
 */
static inline bool slots_bitmap_mpmc_shard_is_full(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index) {
    // Get the shard's used count
    uint8_volatile_t *shard_used_count_ptr = slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, shard_index);
    MEMORY_FENCE_LOAD();
    uint8_volatile_t shard_used_count = *shard_used_count_ptr;

    // If the shard's used count is equal to the shard's size, then the shard is full
    return shard_used_count == SLOTS_BITMAP_MPMC_SHARD_SIZE;
}

/**
 * Increase the count of used slots (bits) in the given shard
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 */
static inline void slots_bitmap_mpmc_shard_increase_used_count(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index) {
    // Get the shard's used count
    uint8_volatile_t *shard_used_count_ptr =
            slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, shard_index);

    // Increase the shard's used count
    __atomic_fetch_add(shard_used_count_ptr, 1, __ATOMIC_ACQ_REL);
}

/**
 * Decrease the count of used slots (bits) in the given shard
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 */
static inline void slots_bitmap_mpmc_shard_decrease_used_count(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index) {
    // Get the shard's used count
    uint8_volatile_t *shard_used_count_ptr =
            slots_bitmap_mpmc_get_shard_used_count_ptr(slots_bitmap, shard_index);

    // Increase the shard's used count
    __atomic_fetch_sub(shard_used_count_ptr, 1, __ATOMIC_ACQ_REL);
}

/**
 * Check if the given bit is set in the given shard
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @param bit_index Index of the bit
 * @return
 */
static inline bool slots_bitmap_mpmc_shard_is_bit_set(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint16_t shard_index,
        uint16_t bit_index) {
    // Get a pointer to the shard
    slots_bitmap_mpmc_shard_t *bitmap_shard = slots_bitmap_mpmc_get_shard_ptr(slots_bitmap, shard_index);

    // Check if the bit is set
    MEMORY_FENCE_LOAD();
    return (*bitmap_shard & (1ULL << bit_index)) != 0;
}

/**
 * Calculate the number of shards needed to store the given size
 *
 * @param size Size of the bitmap
 * @return The number of shards needed to store the given size
 */
uint64_t slots_bitmap_mpmc_calculate_shard_count(
        uint64_t size);

/**
 * Initialize a new concurrent bitmap with the given size
 *
 * @param size Size of the bitmap
 * @return A pointer to the new concurrent bitmap, or NULL if memory allocation fails or size is 0
 */
slots_bitmap_mpmc_t *slots_bitmap_mpmc_init(
        uint64_t size);

/**
 * Free the bitmap
 *
 * @param slots_bitmap Pointer to the bitmap
 */
void slots_bitmap_mpmc_free(
        slots_bitmap_mpmc_t *slots_bitmap);

/**
 * Iterate over the bitmap and return the index of the first slot that is set
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param from The index of the first slot to check
 * @return
 */
uint64_t slots_bitmap_mpmc_iter(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t from);

/**
 * Get the first available slot in the bitmap, the slot is returned as a pointer to the slot, a boolean is returned
 * to indicate if a slot was found or not.
 * The main difference between slots_bitmap_mpmc_get_next_available_ptr_with_step and
 * slots_bitmap_mpmc_get_next_available_ptr is that this function allows to specify the shard to start the search
 * from and the step to use when searching for the next shard.
 * This mechanism allows to implement a more efficient search strategy when using multiple threads, for example
 * each thread can start the search from a different shard and use a step of 1, this way each thread will search
 * a different shard and will clash less with other threads.
 * When using this strategy though it's suggested to invoke the function again with start 0 and step 1 if the
 * first invocation returns false, as this will allow to search the shards and handle the case in which all the shards
 * of a thread are full.
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index_start Index of the shard to start the search from
 * @param shard_index_step Step to use when searching for the next shard
 * @param return_slots_bitmap_index Pointer to the variable that will contain the index of the first available slot
 * @return True if a slot was found, false otherwise
 */
bool slots_bitmap_mpmc_get_next_available_ptr_with_step(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint16_t shard_index_start,
        uint16_t shard_index_step,
        uint64_t *return_slots_bitmap_index);

/**
 * Get the first available slot in the bitmap, an index is returned to indicate the slot, in case of failure UINT64_MAX
 * is returned.
 * The main difference between slots_bitmap_mpmc_get_next_available_with_step and
 * slots_bitmap_mpmc_get_next_available is that this function allows to specify the shard to start the search
 * from and the step to use when searching for the next shard.
 * This mechanism allows to implement a more efficient search strategy when using multiple threads, for example
 * each thread can start the search from a different shard and use a step of 1, this way each thread will search
 * a different shard and will clash less with other threads.
 * When using this strategy though it's suggested to invoke the function again with start 0 and step 1 if the
 * first invocation returns false, as this will allow to search the shards and handle the case in which all the shards
 * of a thread are full.
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index_start Index of the shard to start the search from
 * @param shard_index_step Step to use when searching for the next shard
 * @return Index of the first available slot
 */
uint64_t slots_bitmap_mpmc_get_next_available_with_step(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint16_t shard_index_start,
        uint16_t shard_index_step);

/**
 * Get the first available slot in the bitmap, the slot is returned as a pointer to the slot, a boolean is returned
 * to indicate if a slot was found or not.
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param return_slots_bitmap_index Pointer to the variable that will contain the index of the first available slot
 * @return True if a slot was found, false otherwise
 */
bool slots_bitmap_mpmc_get_next_available_ptr(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t *return_slots_bitmap_index);

/**
 * Get the first available slot in the bitmap, an index is returned to indicate the slot, in case of failure UINT64_MAX
 * is returned.
 *
 * @param slots_bitmap Pointer to the bitmap
 * @return Index of the first available slot
 */
uint64_t slots_bitmap_mpmc_get_next_available(
        slots_bitmap_mpmc_t *slots_bitmap);

/**
 * Release a slot in the bitmap
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param slots_bitmap_index Index of the slot to release
 */
void slots_bitmap_mpmc_release(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t slots_bitmap_index);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SLOTS_BITMAP_MPMC_H
