#ifndef CACHEGRAND_HASHTABLE_DATA_H
#define CACHEGRAND_HASHTABLE_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

hashtable_data_t* hashtable_data_init(
        hashtable_bucket_count_t buckets_count);

void hashtable_data_free(
        hashtable_data_t* hashtable_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_DATA_H
