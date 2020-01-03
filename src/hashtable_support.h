#ifndef CACHEGRAND_HASHTABLE_SUPPORT_H
#define CACHEGRAND_HASHTABLE_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_HASHES_PER_CACHELINE      (int)(HASHTABLE_CACHELINE_LENGTH / HASHTABLE_HASH_SIZE)
#define HASHTABLE_PRIMENUMBERS_MAX          5144556059U
#define HASHTABLE_CACHELINE_LENGTH          64
#define HASHTABLE_HASH_SIZE                 sizeof(hashtable_bucket_hash_t)

#define HASHTABLE_MEMORY_FENCE_LOAD() atomic_thread_fence(memory_order_acquire)
#define HASHTABLE_MEMORY_FENCE_STORE() atomic_thread_fence(memory_order_release)
#define HASHTABLE_MEMORY_FENCE_LOAD_STORE() atomic_thread_fence(memory_order_acq_rel)

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

bool hashtable_primenumbers_supported(uint64_t number);

uint64_t hashtable_primenumbers_next(uint64_t number);

uint64_t hashtable_primenumbers_mod(uint64_t number, uint64_t prime);

uint64_t hashtable_rounddown_to_cacheline(uint64_t number);

uint64_t hashtable_roundup_to_cacheline_plus_one(uint64_t number);

hashtable_bucket_hash_t hashtable_calculate_hash(
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size);

hashtable_bucket_index_t hashtable_bucket_index_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash);

void hashtable_calculate_neighborhood(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_index_t* index_neighborhood_begin,
        hashtable_bucket_index_t* index_neighborhood_end);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_H
