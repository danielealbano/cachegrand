#ifndef CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_NOT_FOUND     32u

#define HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_METHOD_SIZE(METHOD, SIZE) \
    hashtable_mcmp_support_hash_search_##METHOD##_##SIZE

#define HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(METHOD, SIZE) \
    extern hashtable_chunk_slot_index_t HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_METHOD_SIZE(METHOD, SIZE)( \
        hashtable_hash_half_t hash, \
        hashtable_hash_half_volatile_t* hashes, \
        uint32_t skip_indexes)

HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(avx512f, 14);
HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(avx2, 14);
HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(avx, 14);
HASHTABLE_MCMP_SUPPORT_HASH_SEARCH_METHOD_SIZE_SIGNATURE(loop, 14);

extern hashtable_chunk_slot_index_t hashtable_mcmp_support_hash_search(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes,
        uint32_t skip_indexes);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
