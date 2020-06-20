#include <stdlib.h>
#include <stdbool.h>
#include <t1ha.h>

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash.h"

hashtable_hash_t hashtable_support_hash_calculate(hashtable_key_data_t *key, hashtable_key_size_t key_size) {
    return (hashtable_hash_t)t1ha2_atonce(key, key_size, HASHTABLE_T1HA_SEED);
}

hashtable_hash_half_t hashtable_support_hash_half(hashtable_hash_t hash) {
    return (hash >> 32u) | 0x80000000u;
}
