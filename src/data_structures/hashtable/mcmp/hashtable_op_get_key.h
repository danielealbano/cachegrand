#ifndef CACHEGRAND_HASHTABLE_OP_GET_KEY_H
#define CACHEGRAND_HASHTABLE_OP_GET_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_mcmp_op_get_key(
        hashtable_t *hashtable,
        hashtable_bucket_index_t bucket_index,
        hashtable_key_data_t **key,
        hashtable_key_size_t *key_size);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_GET_KEY_H
