#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "xalloc.h"

#include "hashtable.h"
#include "hashtable_op_set.h"
#include "hashtable_support_index.h"
#include "hashtable_support_hash.h"
#include "hashtable_support_op.h"

bool hashtable_op_set(
        hashtable_t *hashtable,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t value) {
    bool created_new;
    hashtable_bucket_hash_t hash;
    hashtable_bucket_hash_half_t hash_half;
    volatile hashtable_bucket_t* bucket;
    volatile hashtable_bucket_chain_ring_t* bucket_chain_ring;
    hashtable_bucket_chain_ring_index_t bucket_chain_ring_index;
    volatile hashtable_bucket_key_value_t* bucket_key_value;
    hashtable_key_data_t* ht_key;

    hash = hashtable_support_hash_calculate(key, key_size);
    hash_half = hashtable_support_hash_half(hash);

    // TODO: the resize logic has to be reviewed, the underlying hash search function has to be aware that it hasn't
    //       to create a new item if it's missing
    bool ret = hashtable_support_op_search_key_or_create_new(
        hashtable->ht_current,
        key,
        key_size,
        hash,
        hash_half,
        true,
        &created_new,
        &bucket,
        &bucket_chain_ring,
        &bucket_chain_ring_index,
        &bucket_key_value);

    if (ret == false) {
        if (bucket) {
            hashtable_support_op_bucket_unlock(bucket);
        }

        return false;
    }

    HASHTABLE_MEMORY_FENCE_LOAD();

    if (created_new) {
        hashtable_bucket_key_value_flags_t flags = 0;

        // Get the destination pointer for the key
        if (key_size <= HASHTABLE_KEY_INLINE_MAX_LENGTH) {
            HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE);

            ht_key = (hashtable_key_data_t *)&bucket_key_value->inline_key.data;
            strncpy((char*)ht_key, key, key_size);
        } else {
#if defined(CACHEGRAND_HASHTABLE_KEY_CHECK_FULL)
            // TODO: The keys must be stored in an append only memory structure to avoid locking, memory can't be freed
            //       immediately after the bucket is freed because it can be in use and would cause a crash34567

            ht_key = xalloc_alloc(key_size + 1);
            ht_key[key_size] = '\0';
            strncpy((char*)ht_key, key, key_size);

            bucket_key_value->external_key.data = ht_key;
            bucket_key_value->external_key.size = key_size;
#else
            bucket_key_value->prefix_key.size = key_size;
            strncpy((char*)bucket_key_value->prefix_key.data, key, HASHTABLE_KEY_PREFIX_SIZE);
#endif // CACHEGRAND_HASHTABLE_KEY_CHECK_FULL
        }

        // Set the FILLED flag (drops the deleted flag as well)
        HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(flags, HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED);

        // Update the flags atomically
        bucket_key_value->flags = flags;
    }

    bucket_key_value->data = value;

    HASHTABLE_MEMORY_FENCE_STORE();

    hashtable_support_op_bucket_unlock(bucket);

    return true;
}
