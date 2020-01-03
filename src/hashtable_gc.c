#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "xalloc.h"

#include "hashtable.h"
#include "hashtable_support.h"
#include "hashtable_gc.h"

void hashtable_garbage_collect_neighborhood(hashtable_t* hashtable, hashtable_bucket_index_t bucket_index) {
    volatile hashtable_data_t* hashtable_data = hashtable->ht_current;
    hashtable_bucket_index_t index_neighborhood_begin = hashtable_rounddown_to_cacheline(bucket_index);
    hashtable_bucket_index_t index_neighborhood_end = hashtable_roundup_to_cacheline_plus_one(bucket_index);

    for(
            hashtable_bucket_index_t index_outer = index_neighborhood_begin;
            index_outer < index_neighborhood_end;
            index_outer++) {
        HASHTABLE_MEMORY_FENCE_LOAD();

        for(
                hashtable_bucket_index_t index_inner = index_neighborhood_begin;
                index_inner < index_neighborhood_end;
                index_inner++) {
            if (hashtable_data->hashes[index_outer] != hashtable_data->hashes[index_inner]) {
                continue;
            }

            volatile hashtable_bucket_key_value_t* bucket_key_value = &hashtable_data->keys_values[index_inner];
            HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(bucket_key_value->flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED);

            // TODO: this has to change, the key has to be stored in an append-only storage because another thread may
            //       be using it and we don't want to perform useless data copy
            if (HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(
                    bucket_key_value->flags,
                    HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE) == false) {
                xfree(bucket_key_value->external_key.data);
            }

            hashtable_data->hashes[index_inner] = 0;
        }

        HASHTABLE_MEMORY_FENCE_STORE();
    }
}