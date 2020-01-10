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
        hashtable_bucket_index_t *found_index,
        volatile hashtable_bucket_key_value_t **found_key_value);

hashtable_search_key_or_create_new_ret_t hashtable_support_op_search_key_or_create_new(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        bool *created_new,
        hashtable_bucket_index_t *found_index,
        volatile hashtable_bucket_key_value_t **found_key_value);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_OP_H
