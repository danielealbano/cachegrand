#ifndef CACHEGRAND_HASHTABLE_OP_GET_H
#define CACHEGRAND_HASHTABLE_OP_GET_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_mcmp_op_get(
        hashtable_t *hashtable,
        hashtable_database_number_t database_number,
        hashtable_key_data_t *key,
        hashtable_key_length_t key_length,
        hashtable_value_data_t *data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_GET_H
