#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "xalloc.h"
#include "hashtable.h"
#include "hashtable_config.h"
#include "hashtable_data.h"
#include "hashtable_support_primenumbers.h"

hashtable_t* hashtable_init(hashtable_config_t* hashtable_config) {
    hashtable_bucket_count_t buckets_count = hashtable_support_primenumbers_next(hashtable_config->initial_size);
    uint16_t cachelines_to_probe = hashtable_data_cachelines_to_probe_from_buckets_count(
            hashtable_config,
            buckets_count);

    hashtable_t* hashtable = (hashtable_t*)xalloc_alloc(sizeof(hashtable_t));
    hashtable_data_t* hashtable_data = hashtable_data_init(
            buckets_count,
            cachelines_to_probe);

    hashtable->is_resizing = false;
    hashtable->is_shutdowning = false;
    hashtable->ht_current = hashtable_data;
    hashtable->ht_old = NULL;

    hashtable->config = hashtable_config;

    hashtable_support_hash_search_select_instruction_set();

    return hashtable;
}

void hashtable_free(hashtable_t* hashtable) {
    if (hashtable->ht_current) {
        hashtable_data_free(hashtable->ht_current);
        hashtable->ht_current = NULL;
    }

    if (hashtable->ht_old) {
        hashtable_data_free(hashtable->ht_old);
        hashtable->ht_old = NULL;
    }

    hashtable_config_free(hashtable->config);

    xalloc_free(hashtable);
}

