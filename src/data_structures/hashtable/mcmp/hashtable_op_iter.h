#ifndef CACHEGRAND_HASHTABLE_OP_ITER_H
#define CACHEGRAND_HASHTABLE_OP_ITER_H

#ifdef __cplusplus
extern "C" {
#endif

uint64_t hashtable_mcmp_op_iter_internal(
        hashtable_data_volatile_t *hashtable_data,
        bool all_databases,
        hashtable_database_number_t database_number,
        uint64_t bucket_index,
        uint64_t max_distance);

uint64_t hashtable_mcmp_op_iter(
        hashtable_t *hashtable,
        hashtable_database_number_t database_number,
        uint64_t bucket_index);

uint64_t hashtable_mcmp_op_iter_max_distance(
        hashtable_t *hashtable,
        hashtable_database_number_t database_number,
        uint64_t bucket_index,
        uint64_t max_distance);

uint64_t hashtable_mcmp_op_iter_all_databases(
        hashtable_t *hashtable,
        uint64_t bucket_index);

uint64_t hashtable_mcmp_op_iter_max_distance_all_databases(
        hashtable_t *hashtable,
        uint64_t bucket_index,
        uint64_t max_distance);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_ITER_H
