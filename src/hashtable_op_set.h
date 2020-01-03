#ifndef CACHEGRAND_HASHTABLE_OP_SET_H
#define CACHEGRAND_HASHTABLE_OP_SET_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_search_key_or_create_new(
        volatile hashtable_data_t* hashtable_data,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size,
        hashtable_bucket_hash_t hash,
        bool* created_new,
        hashtable_bucket_index_t* found_index,
        volatile hashtable_bucket_key_value_t** found_key_value);

bool hashtable_set(hashtable_t* hashtable, hashtable_key_data_t* key, hashtable_key_size_t key_size, void* value);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_SET_H
