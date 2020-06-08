#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xalloc.h"
#include "log.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support_primenumbers.h"

static const char* TAG = "hashtable/data";

hashtable_data_t* hashtable_data_init(hashtable_bucket_count_t buckets_count) {
    if (hashtable_support_primenumbers_valid(buckets_count) == false) {
        LOG_E(TAG, "The buckets_count is greater than the maximum allowed value %lu", HASHTABLE_PRIMENUMBERS_MAX);
        return NULL;
    }

    hashtable_data_t* hashtable_data = (hashtable_data_t*)xalloc_alloc(sizeof(hashtable_data_t));

    hashtable_data->buckets_count = buckets_count;
    hashtable_data->buckets_size = sizeof(hashtable_bucket_t) * buckets_count;

    hashtable_data->buckets = (hashtable_bucket_t*)xalloc_mmap_alloc(hashtable_data->buckets_size);

    return hashtable_data;
}

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
void hashtable_data_free_buckets(hashtable_data_t* hashtable_data) {
    for(hashtable_bucket_index_t index = 0; index < hashtable_data->buckets_count; index++) {
        if (hashtable_data->buckets[index].keys_values == NULL) {
            continue;
        }

        xalloc_free((hashtable_bucket_key_value_t*)hashtable_data->buckets[index].keys_values);
    }
}
#endif

void hashtable_data_free(hashtable_data_t* hashtable_data) {
#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
    hashtable_data_free_buckets(hashtable_data);
#endif
    xalloc_mmap_free((void*)hashtable_data->buckets, hashtable_data->buckets_size);
    xalloc_free((void*)hashtable_data);
}
