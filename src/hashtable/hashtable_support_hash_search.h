#ifndef CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND     32u

#define HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(METHOD, SIZE) \
    hashtable_support_hash_search_##METHOD##_##SIZE

#define HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(METHOD, SIZE) \
    extern hashtable_bucket_chain_ring_index_t HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(METHOD, SIZE)( \
        hashtable_bucket_hash_half_t hash, \
        hashtable_bucket_hash_half_atomic_t* hashes, \
        uint32_t skip_indexes)

HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(avx2, 14);
HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(avx2, 8);
HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(avx, 14);
HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(avx, 8);
HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(loop, 14);
HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(loop, 8);

extern hashtable_bucket_chain_ring_index_t hashtable_support_hash_search(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
