#ifndef CACHEGRAND_HASHTABLE_SUPPORT_OP_H
#define CACHEGRAND_HASHTABLE_SUPPORT_OP_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_support_op_search_key(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_hash_half_t hash_half,
        volatile hashtable_bucket_chain_ring_t **found_chain_ring,
        hashtable_bucket_chain_ring_index_t *found_chain_ring_index,
        volatile hashtable_bucket_key_value_t **found_bucket_key_value);

bool hashtable_support_op_search_key_or_create_new(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_hash_half_t hash_half,
        bool create_new_if_missing,
        bool *created_new,
        volatile hashtable_bucket_t **found_bucket,
        volatile hashtable_bucket_chain_ring_t **found_chain_ring,
        hashtable_bucket_chain_ring_index_t *found_chain_ring_index,
        volatile hashtable_bucket_key_value_t **found_key_value);

bool hashtable_support_op_bucket_lock(
        volatile hashtable_bucket_t* bucket,
        bool retry);

void hashtable_support_op_bucket_unlock(
        volatile hashtable_bucket_t* bucket);

volatile hashtable_bucket_t* hashtable_support_op_bucket_fetch_and_write_lock(
        volatile hashtable_data_t *hashtable_data,
        hashtable_bucket_index_t bucket_index,
        bool initialize_new_if_missing,
        bool *initialized);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_OP_H
