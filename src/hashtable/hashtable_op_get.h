#ifndef CACHEGRAND_HASHTABLE_OP_GET_H
#define CACHEGRAND_HASHTABLE_OP_GET_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_search_key(
        volatile hashtable_data_t* hashtable_data,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        hashtable_bucket_index_t* found_index,
        volatile hashtable_bucket_key_value_t** found_key_value);

bool hashtable_get(
        hashtable_t* hashtable,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t* data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_GET_H
