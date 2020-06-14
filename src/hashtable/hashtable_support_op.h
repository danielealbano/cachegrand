#ifndef CACHEGRAND_HASHTABLE_SUPPORT_OP_H
#define CACHEGRAND_HASHTABLE_SUPPORT_OP_H

#ifdef __cplusplus
extern "C" {
#endif

extern bool hashtable_support_op_search_key(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        volatile hashtable_key_value_t **found_key_value);

extern bool hashtable_support_op_search_key_or_create_new(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        bool *created_new,
        hashtable_half_hashes_chunk_atomic_t **found_half_hashes_chunk,
        volatile hashtable_key_value_t **found_key_value);

extern bool hashtable_support_op_half_hashes_chunk_lock(
        hashtable_half_hashes_chunk_atomic_t *half_hashes_chunk,
        bool retry);

extern void hashtable_support_op_half_hashes_chunk_unlock(
        hashtable_half_hashes_chunk_atomic_t *half_hashes_chunk);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_OP_H
