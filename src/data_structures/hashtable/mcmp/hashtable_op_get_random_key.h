#ifndef CACHEGRAND_HASHTABLE_OP_GET_RANDOM_KEY_H
#define CACHEGRAND_HASHTABLE_OP_GET_RANDOM_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_mcmp_op_get_random_key_try(
        hashtable_t *hashtable,
        char **key,
        hashtable_key_size_t *key_size);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_GET_RANDOM_KEY_H
