#ifndef CACHEGRAND_HASHTABLE_DATA_H
#define CACHEGRAND_HASHTABLE_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

uint16_t hashtable_data_cachelines_to_probe_from_buckets_count(
        hashtable_config_t* hashtable_config,
        hashtable_bucket_count_t buckets_count);

hashtable_data_t* hashtable_data_init(
        hashtable_bucket_count_t buckets_count,
        uint16_t cachelines_to_probe);

void hashtable_data_free(
        volatile hashtable_data_t* hashtable_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_DATA_H
