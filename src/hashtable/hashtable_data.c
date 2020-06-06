#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xalloc.h"
#include "log.h"

#include "hashtable.h"
#include "hashtable_data.h"
#include "hashtable_support_primenumbers.h"
#include "hashtable_support_index.h"

static const char* TAG = "hashtable/data";

hashtable_data_t* hashtable_data_init(hashtable_bucket_count_t buckets_count) {
    if (hashtable_support_primenumbers_valid(buckets_count) == false) {
        LOG_E(TAG, "The buckets_count is greater than the maximum allowed value %lu", HASHTABLE_PRIMENUMBERS_MAX);
        return NULL;
    }

    size_t hashes_size = sizeof(hashtable_bucket_hash_t) * buckets_count_real;
    size_t keys_values_size = sizeof(hashtable_bucket_key_value_t) * buckets_count_real;

    hashtable_data_t* hashtable_data = (hashtable_data_t*)xalloc_alloc(sizeof(hashtable_data_t));

    hashtable_data->buckets_count = buckets_count;
    hashtable_data->hashes_size = hashes_size;
    hashtable_data->keys_values_size = keys_values_size;
    hashtable_data->hashes = (hashtable_bucket_hash_atomic_t*)xalloc_mmap_alloc(hashes_size);
    hashtable_data->keys_values = (hashtable_bucket_key_value_t*)xalloc_mmap_alloc(keys_values_size);

    return hashtable_data;
}

extern inline void hashtable_data_free(volatile hashtable_data_t* hashtable_data) {
    xalloc_mmap_free((void*)hashtable_data->hashes, hashtable_data->hashes_size);
    xalloc_mmap_free((void*)hashtable_data->keys_values, hashtable_data->keys_values_size);
    xalloc_free((void*)hashtable_data);
}
