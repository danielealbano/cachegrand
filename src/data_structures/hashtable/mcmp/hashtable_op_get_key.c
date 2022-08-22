/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <numa.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "log/log.h"

#include "hashtable.h"

bool hashtable_mcmp_op_get_key(
        hashtable_t *hashtable,
        hashtable_bucket_index_t bucket_index,
        hashtable_key_data_t **key,
        hashtable_key_size_t *key_size) {
    MEMORY_FENCE_LOAD();

    hashtable_data_volatile_t* hashtable_data = hashtable->ht_current;
    hashtable_key_value_volatile_t *key_value = &hashtable_data->keys_values[bucket_index];

    if (
            unlikely(HASHTABLE_KEY_VALUE_IS_EMPTY(key_value->flags)) ||
            unlikely(HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_DELETED))) {
        return false;
    }

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE==1
    // The key may potentially change if the item is first deleted and then recreated, if it's inline it
    // doesn't really matter because the key will mismatch and the execution will continue but if the key is
    // stored externally and the allocated memory is freed it may crash.
    if (HASHTABLE_KEY_VALUE_HAS_FLAG(key_value->flags, HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE)) {
        *key = key_value->inline_key.data;
        *key_size = key_value->inline_key.size;
    } else {
#endif
        *key = key_value->external_key.data;
        *key_size = key_value->external_key.size;
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE==1
    }
#endif

    return true;
}
