#ifndef CACHEGRAND_HASHTABLE_OP_SET_H
#define CACHEGRAND_HASHTABLE_OP_SET_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_mcmp_op_set(
        hashtable_t *hashtable,
        hashtable_database_number_t database_number,
        hashtable_key_data_t *key,
        hashtable_key_length_t key_length,
        hashtable_value_data_t new_value,
        hashtable_value_data_t *previous_value,
        hashtable_bucket_index_t *out_bucket_index,
        bool *out_should_free_key);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_SET_H
