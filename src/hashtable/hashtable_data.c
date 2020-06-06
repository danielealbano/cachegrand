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

    hashtable_data_t* hashtable_data = (hashtable_data_t*)xalloc_alloc(sizeof(hashtable_data_t));

    hashtable_data->buckets_count = buckets_count;
    hashtable_data->buckets_size = sizeof(hashtable_bucket_t) * buckets_count;

    hashtable_data->buckets = (hashtable_bucket_t*)xalloc_mmap_alloc(hashtable_data->buckets_size);

    return hashtable_data;
}

void hashtable_data_free_buckets_chain_rings(volatile hashtable_bucket_t* bucket) {
    volatile hashtable_bucket_chain_ring_t* chain_ring = bucket->chain_first_ring;
    while(chain_ring != NULL) {
        volatile hashtable_bucket_chain_ring_t* next = chain_ring->next_ring;

        xalloc_free((void*)chain_ring);

        chain_ring = next;
    }
}

void hashtable_data_free_buckets(hashtable_data_t* hashtable_data) {
    for(hashtable_bucket_index_t index = 0; index < hashtable_data->buckets_count; index++) {
        if (&hashtable_data->buckets[index]) {
            continue;
        }

        volatile hashtable_bucket_t* bucket = &hashtable_data->buckets[index];

        hashtable_data_free_buckets_chain_rings(bucket);

        xalloc_mmap_free((void*)bucket, sizeof(hashtable_bucket_t));
    }
}

void hashtable_data_free(hashtable_data_t* hashtable_data) {
    hashtable_data_free_buckets(hashtable_data);
    xalloc_mmap_free((void*)hashtable_data->buckets, hashtable_data->buckets_size);
    xalloc_free((void*)hashtable_data);
}
