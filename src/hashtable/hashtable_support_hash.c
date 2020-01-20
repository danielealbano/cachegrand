#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <t1ha.h>

#include "hashtable.h"
#include "hashtable_support_hash.h"

hashtable_bucket_hash_t hashtable_support_hash_calculate(hashtable_key_data_t *key, hashtable_key_size_t key_size) {
    return (hashtable_bucket_hash_t)t1ha2_atonce(key, key_size, HASHTABLE_T1HA2_SEED);
}

hashtable_bucket_hash_t hashtable_support_hash_ensure_not_zero(hashtable_bucket_hash_t hash) {
    if (hash == 0) {
        return -1;
    }

    return hash;
}