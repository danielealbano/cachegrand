#ifndef CACHEGRAND_HASHTABLE_SUPPORT_INDEX_H
#define CACHEGRAND_HASHTABLE_SUPPORT_INDEX_H

#ifdef __cplusplus
extern "C" {
#endif

hashtable_bucket_index_t hashtable_support_index_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_INDEX_H
