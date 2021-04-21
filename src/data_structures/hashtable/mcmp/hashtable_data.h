#ifndef CACHEGRAND_HASHTABLE_DATA_H
#define CACHEGRAND_HASHTABLE_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

hashtable_data_t* hashtable_mcmp_data_init(
        hashtable_bucket_count_t buckets_count);

bool hashtable_mcmp_data_numa_interleave_memory(
        hashtable_data_t* hashtable_data,
        struct bitmask* numa_nodes_bitmask);

void hashtable_mcmp_data_free(
        hashtable_data_t* hashtable_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_DATA_H
