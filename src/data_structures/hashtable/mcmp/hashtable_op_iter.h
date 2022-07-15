#ifndef CACHEGRAND_HASHTABLE_OP_ITER_H
#define CACHEGRAND_HASHTABLE_OP_ITER_H

#ifdef __cplusplus
extern "C" {
#endif

void *hashtable_op_iter_next(
        hashtable_t *hashtable,
        uint64_t *bucket_index);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_ITER_H
