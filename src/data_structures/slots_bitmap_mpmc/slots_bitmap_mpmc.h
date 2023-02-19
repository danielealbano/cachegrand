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

uint64_t slots_bitmap_mpmc_calculate_shard_count(
        uint64_t size);

slots_bitmap_mpmc_t *slots_bitmap_mpmc_init(
        uint64_t size);

void slots_bitmap_mpmc_free(
        slots_bitmap_mpmc_t *bitmap);

slots_bitmap_mpmc_shard_t* slots_bitmap_mpmc_get_shard_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t shard_index);

uint8_t* slots_bitmap_mpmc_get_shard_used_count_ptr(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t shard_index);

bool slots_bitmap_mpmc_get_first_available(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t *return_bitmap_index);

void slots_bitmap_mpmc_release(
        slots_bitmap_mpmc_t *bitmap,
        uint64_t bitmap_index);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SLOTS_BITMAP_MPMC_H
