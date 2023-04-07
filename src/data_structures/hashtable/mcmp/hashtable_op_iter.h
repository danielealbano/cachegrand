#ifndef CACHEGRAND_HASHTABLE_OP_ITER_H
#define CACHEGRAND_HASHTABLE_OP_ITER_H

#ifdef __cplusplus
extern "C" {
#endif

void *hashtable_mcmp_op_data_iter(
        hashtable_data_volatile_t *hashtable_data,
        uint64_t *bucket_index,
        uint64_t max_distance);

void *hashtable_mcmp_op_iter(
        hashtable_t *hashtable,
        uint64_t *bucket_index);

void *hashtable_mcmp_op_iter_max_distance(
        hashtable_t *hashtable,
        uint64_t *bucket_index,
        uint64_t max_distance);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_ITER_H
