#include <stdlib.h>
#include <stdbool.h>
#include <t1ha.h>

#include "exttypes.h"
#include "spinlock.h"

#include "hash/hash_crc32c.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash.h"

hashtable_hash_t hashtable_support_hash_calculate(hashtable_key_data_t *key, hashtable_key_size_t key_size) {
#if CACHEGRAND_CMAKE_CONFIG_USE_HASHTABLE_HASH_ALGORITHM_T1HA2 == 1
    return (hashtable_hash_t)t1ha2_atonce(key, key_size, HASHTABLE_SUPPORT_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASHTABLE_HASH_ALGORITHM_CRC32C == 1
    uint32_t crc32 = hash_crc32c(key, key_size, HASHTABLE_SUPPORT_HASH_SEED);
    hashtable_hash_t hash = ((uint64_t)hash_crc32c(key, key_size, crc32) << 32u) | crc32;

   return hash;
#else
#error "Unsupported hash algorithm"
#endif
}

hashtable_hash_half_t hashtable_support_hash_half(hashtable_hash_t hash) {
    return (hash >> 32u) | 0x80000000u;
}

hashtable_hash_quarter_t hashtable_support_hash_quarter(hashtable_hash_half_t hash_half) {
    return hash_half & 0xFFFFu;
}
