#ifndef CACHEGRAND_HASHTABLE_OP_DELETE_H
#define CACHEGRAND_HASHTABLE_OP_DELETE_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_mcmp_op_delete(
        hashtable_t* hashtable,
        hashtable_database_number_t database_number,
        hashtable_key_data_t* key,
        hashtable_key_length_t key_length,
        hashtable_value_data_t *current_value);

bool hashtable_mcmp_op_delete_by_index(
        hashtable_t* hashtable,
        hashtable_database_number_t database_number,
        hashtable_bucket_index_t bucket_index,
        hashtable_value_data_t *current_value);

bool hashtable_mcmp_op_delete_by_index_all_databases(
        hashtable_t* hashtable,
        hashtable_bucket_index_t bucket_index,
        hashtable_value_data_t *current_value);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_DELETE_H
