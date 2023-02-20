#ifndef CACHEGRAND_SLOTS_BITMAP_MPMC_H
#define CACHEGRAND_SLOTS_BITMAP_MPMC_H

#ifdef __cplusplus
extern "C" {
#endif

#define SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE           uint64_t
#define SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE_VOLATILE  uint64_volatile_t
#define SLOTS_BITMAP_MPMC_SHARD_SIZE                (sizeof(SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE) * 8)

typedef SLOTS_BITMAP_MPMC_SHARD_DATA_TYPE_VOLATILE slots_bitmap_mpmc_shard_t;

typedef struct slots_bitmap_mpmc slots_bitmap_mpmc_t;
struct slots_bitmap_mpmc {
    size_t size;
    size_t shards_count;
    slots_bitmap_mpmc_shard_t shards[0];
    uint8_volatile_t shards_used_slots[0];
};

/**
 * Calculate the number of shards needed to store the given size
 * @param size
 * @return
 */
uint64_t slots_bitmap_mpmc_calculate_shard_count(
        uint64_t size);

/**
 * Calculate the number of shards needed to store the given size
 * @param size Size of the bitmap
 * @return Number of shards needed to store the given size
 */
slots_bitmap_mpmc_t *slots_bitmap_mpmc_init(
        uint64_t size);

/**
 * Free the bitmap
 * @param slots_bitmap Pointer to the bitmap
 */
void slots_bitmap_mpmc_free(
        slots_bitmap_mpmc_t *slots_bitmap);

/**
 * Get the pointer to the shard
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return Pointer to the shard
 */
slots_bitmap_mpmc_shard_t* slots_bitmap_mpmc_get_shard_ptr(
        slots_bitmap_mpmc_t *slots_bitmap,
        uint64_t shard_index);

/**
 * Get the pointer to the shard used count of a shard
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
 * @param slots_bitmap Pointer to the bitmap
 * @param return_slots_bitmap_index Pointer to the variable that will contain the index of the first available slot
 * @return True if a slot was found, false otherwise
 */
bool slots_bitmap_mpmc_get_next_available_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t *return_slots_bitmap_index);

/**
 * Get the first available slot in the bitmap, an index is returned to indicate the slot, in case of failure UINT64_MAX
 * is returned.
 * @param slots_bitmap Pointer to the bitmap
 * @return Index of the first available slot
 */
uint64_t slots_bitmap_mpmc_get_next_available(
        slots_bitmap_mpmc_t *slots_bitmap);

/**
 * Release a slot in the bitmap
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
