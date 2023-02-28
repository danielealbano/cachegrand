#ifndef CACHEGRAND_SLOTS_BITMAP_MPMC_H
#define CACHEGRAND_SLOTS_BITMAP_MPMC_H

#ifdef __cplusplus
extern "C" {
#endif

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
 * Get the pointer to the shard
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return Pointer to the shard
 */
slots_bitmap_mpmc_shard_t* slots_bitmap_mpmc_get_shard_ptr(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index);

/**
 * Get the pointer to the shard used count of a shard
 *
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return Pointer to the shard used count
 */
uint8_t* slots_bitmap_mpmc_get_shard_used_count_ptr(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index);

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
