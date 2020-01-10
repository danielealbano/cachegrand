#ifndef CACHEGRAND_HASHTABLE_SUPPORT_INDEX_H
#define CACHEGRAND_HASHTABLE_SUPPORT_INDEX_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_HASHES_PER_CACHELINE      (int)(HASHTABLE_CACHELINE_LENGTH / HASHTABLE_HASH_SIZE)
#define HASHTABLE_CACHELINE_LENGTH          64
#define HASHTABLE_HASH_SIZE                 sizeof(hashtable_bucket_hash_t)

uint64_t hashtable_support_index_rounddown_to_cacheline(uint64_t number);

uint64_t hashtable_support_index_roundup_to_cacheline_plus_one(uint64_t number);

hashtable_bucket_index_t hashtable_support_index_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash);

void hashtable_support_index_calculate_neighborhood_from_index(
        hashtable_bucket_index_t index,
        hashtable_bucket_index_t *index_neighborhood_begin,
        hashtable_bucket_index_t *index_neighborhood_end);

void hashtable_support_index_calculate_neighborhood_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_index_t *index_neighborhood_begin,
        hashtable_bucket_index_t *index_neighborhood_end);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_INDEX_H
