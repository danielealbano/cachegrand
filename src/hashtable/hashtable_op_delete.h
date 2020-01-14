#ifndef CACHEGRAND_HASHTABLE_OP_DELETE_H
#define CACHEGRAND_HASHTABLE_OP_DELETE_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_op_delete(
        hashtable_t* hashtable,
        hashtable_key_data_t* key,
        hashtable_key_size_t key_size);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_DELETE_H
