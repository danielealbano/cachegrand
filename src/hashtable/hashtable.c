#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_data.h"

//https://jameshfisher.com/2018/03/30/round-up-power-2/
uint64_t next_pow2m1(uint64_t x) {
    x |= x>>1;
    x |= x>>2;
    x |= x>>4;
    x |= x>>8;
    x |= x>>16;
    x |= x>>32;

    return x;
}
uint64_t next_pow2(uint64_t x) {
    return next_pow2m1(x-1)+1;
}

hashtable_t* hashtable_init(hashtable_config_t* hashtable_config) {
    hashtable_bucket_count_t buckets_count = next_pow2(hashtable_config->initial_size);
    hashtable_t* hashtable = (hashtable_t*)xalloc_alloc(sizeof(hashtable_t));
    hashtable_data_t* hashtable_data = hashtable_data_init(buckets_count);

    hashtable->is_resizing = false;
    hashtable->is_shutdowning = false;
    hashtable->ht_current = hashtable_data;
    hashtable->ht_old = NULL;

    hashtable->config = hashtable_config;

    return hashtable;
}

void hashtable_free(hashtable_t* hashtable) {
    if (hashtable->ht_current) {
        hashtable_data_free((hashtable_data_t*)hashtable->ht_current);
        hashtable->ht_current = NULL;
    }

    if (hashtable->ht_old) {
        hashtable_data_free((hashtable_data_t*)hashtable->ht_old);
        hashtable->ht_old = NULL;
    }

    hashtable_config_free(hashtable->config);

    xalloc_free(hashtable);
}

