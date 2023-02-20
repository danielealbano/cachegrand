#ifndef CACHEGRAND_SLOTS_BITMAP_SPSC_H
#define CACHEGRAND_SLOTS_BITMAP_SPSC_H

#ifdef __cplusplus
extern "C" {
#endif

#define SLOTS_BITMAP_SPSC_SHARD_DATA_TYPE   uint64_t
#define SLOTS_BITMAP_SPSC_SHARD_SIZE        (sizeof(SLOTS_BITMAP_SPSC_SHARD_DATA_TYPE) * 8)

typedef SLOTS_BITMAP_SPSC_SHARD_DATA_TYPE slots_bitmap_spsc_shard_t;

/**
 * Bitmap structure
 *
 * The bitmap is divided in shards, each shard is a 64 bit integer and the amount of occupied slots is tracked in
 * shards_used_slots. When a shard is full, the corresponding bit in shards_full is set.
 * The algorithm will use the shards_full bitmap to find the first shard that is not full and then search a free slot
 * in that shard.
 * When a free bit is found the counter in shards_used_slots is incremented and the new value will match the amount of
 * available slots in the shard then the shard is marked as full.
 */
typedef struct slots_bitmap_spsc slots_bitmap_spsc_t;
struct slots_bitmap_spsc {
    size_t size;
    size_t shards_count;
    slots_bitmap_spsc_shard_t shards[0];
    uint8_t shards_used_slots[0];
    uint64_t shards_full[0];
};

/**
 * Calculate the number of shards needed to store the given size
 * @param size Size of the bitmap
 * @return Number of shards needed to store the given size
 */
uint64_t slots_bitmap_spsc_calculate_shard_count(
        uint64_t size);

/**
 * Initialize a new bitmap
 * @param size Size of the bitmap
 * @return Pointer to the bitmap
 */
slots_bitmap_spsc_t *slots_bitmap_spsc_init(
        uint64_t size);

/**
 * Free the bitmap
 * @param slots_bitmap Pointer to the bitmap
 */
void slots_bitmap_spsc_free(
        slots_bitmap_spsc_t *slots_bitmap);

/**
 * Get the pointer to the shard
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return Pointer to the shard
 */
slots_bitmap_spsc_shard_t* slots_bitmap_spsc_get_shard_ptr(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_index);

/**
 * Get the pointer to the shard used count of a shard
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return Pointer to the shard used count
 */
uint8_t* slots_bitmap_spsc_get_shard_used_count_ptr(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_index);

/**
 * Get the pointer to the shard full bitmap
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_full_index Index of the shard full bitmap (index of the shard / 64)
 * @return Pointer to the shard full bitmap
 */
uint64_t* slots_bitmap_spsc_get_shard_full_ptr(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_full_index);

/**
 * Set the full bit of a shard
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @param full True if the shard is full, false otherwise
 */
void slots_bitmap_spsc_set_shard_full_bit(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_index,
        bool full);

/**
 * Get the full bit of a shard
 * @param slots_bitmap Pointer to the bitmap
 * @param shard_index Index of the shard
 * @return True if the shard is full, false otherwise
 */
bool slots_bitmap_spsc_is_shard_full(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t shard_index);

/**
 * Get the first available slot in the bitmap
 * @param slots_bitmap Pointer to the bitmap
 * @param return_slots_bitmap_index Pointer to the variable that will contain the index of the first available slot
 * @return True if a slot was found, false otherwise
 */
bool slots_bitmap_spsc_get_first_available(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t *return_slots_bitmap_index);

/**
 * Release a slot in the bitmap
 * @param slots_bitmap Pointer to the bitmap
 * @param slots_bitmap_index Index of the slot to release
 */
void slots_bitmap_spsc_release(
        slots_bitmap_spsc_t *slots_bitmap,
        uint64_t slots_bitmap_index);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SLOTS_BITMAP_SPSC_H
