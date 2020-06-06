#ifndef CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND     32u

typedef hashtable_bucket_chain_ring_index_t (*hashtable_support_hash_search_fp_t)(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes);

extern hashtable_bucket_chain_ring_index_t hashtable_support_hash_search_avx2(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes);
extern hashtable_bucket_chain_ring_index_t hashtable_support_hash_search_avx(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes);
extern hashtable_bucket_chain_ring_index_t hashtable_support_hash_search_loop(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes);

void hashtable_support_hash_search_select_instruction_set();

hashtable_support_hash_search_fp_t hashtable_support_hash_search;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_HASH_SEARCH_H
