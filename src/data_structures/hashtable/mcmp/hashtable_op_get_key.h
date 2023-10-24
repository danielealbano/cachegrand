#ifndef CACHEGRAND_HASHTABLE_OP_GET_KEY_H
#define CACHEGRAND_HASHTABLE_OP_GET_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_mcmp_op_get_key(
        hashtable_t *hashtable,
        hashtable_database_number_t database_number,
        transaction_t *transaction,
        hashtable_bucket_index_t bucket_index,
        hashtable_key_data_t **key,
        hashtable_key_length_t *key_length);

bool hashtable_mcmp_op_get_key_all_databases(
        hashtable_t *hashtable,
        hashtable_bucket_index_t bucket_index,
        transaction_t *transaction,
        hashtable_database_number_t *database_number,
        hashtable_key_data_t **key,
        hashtable_key_length_t *key_length);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_GET_KEY_H
