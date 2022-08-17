#ifndef CACHEGRAND_HASHTABLE_SUPPORT_OP_H
#define CACHEGRAND_HASHTABLE_SUPPORT_OP_H

#ifdef __cplusplus
extern "C" {
#endif

extern bool hashtable_mcmp_support_op_search_key(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value);

extern bool hashtable_mcmp_support_op_search_key_or_create_new(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        bool *created_new,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_half_hashes_chunk_volatile_t **found_half_hashes_chunk,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value);

extern bool hashtable_mcmp_support_op_half_hashes_chunk_lock(
        hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk,
        bool retry);

extern void hashtable_mcmp_support_op_half_hashes_chunk_unlock(
        hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_OP_H
