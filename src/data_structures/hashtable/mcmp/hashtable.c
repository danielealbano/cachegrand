/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"
#include "pow2.h"

#include "hashtable.h"
#include "hashtable_config.h"
#include "hashtable_data.h"

hashtable_t* hashtable_mcmp_init(hashtable_config_t* hashtable_config) {
    hashtable_bucket_count_t buckets_count = pow2_next(hashtable_config->initial_size);
    hashtable_t* hashtable = (hashtable_t*)xalloc_alloc(sizeof(hashtable_t));
    hashtable_data_t* hashtable_data = hashtable_mcmp_data_init(buckets_count);

    if (hashtable_data == NULL) {
        xalloc_free(hashtable);
        return NULL;
    }

    if (hashtable_config->numa_aware) {
        if (!hashtable_mcmp_data_numa_interleave_memory(
                hashtable_data,
                hashtable_config->numa_nodes_bitmask)) {
            hashtable_mcmp_data_free(hashtable_data);
            return NULL;
        }
    }

    hashtable->is_resizing = false;
    hashtable->ht_current = hashtable_data;
    hashtable->ht_old = NULL;

    hashtable->config = hashtable_config;

    return hashtable;
}

void hashtable_mcmp_free(hashtable_t* hashtable) {
    if (hashtable->ht_current) {
        hashtable_mcmp_data_free((hashtable_data_t *) hashtable->ht_current);
        hashtable->ht_current = NULL;
    }

    if (hashtable->ht_old) {
        hashtable_mcmp_data_free((hashtable_data_t *) hashtable->ht_old);
        hashtable->ht_old = NULL;
    }

    hashtable_mcmp_config_free(hashtable->config);

    xalloc_free(hashtable);
}
