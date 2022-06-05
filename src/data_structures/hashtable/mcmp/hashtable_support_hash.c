/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdbool.h>
#include <numa.h>

#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
#include "t1ha.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
#include "xxhash.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
#include "hash/hash_crc32c.h"
#else
#error "Unsupported hash algorithm"
#endif

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable.h"
#include "hashtable_support_hash.h"

hashtable_hash_t hashtable_mcmp_support_hash_calculate(hashtable_key_data_t *key, hashtable_key_size_t key_size) {
#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
    return (hashtable_hash_t)t1ha2_atonce(key, key_size, HASHTABLE_SUPPORT_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
    return (hashtable_hash_t)XXH3_64bits_withSeed(key, key_size, HASHTABLE_SUPPORT_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
    uint32_t crc32 = hash_crc32c(key, key_size, HASHTABLE_SUPPORT_HASH_SEED);
    hashtable_hash_t hash = ((uint64_t)hash_crc32c(key, key_size, crc32) << 32u) | crc32;

    return hash;
#endif
}

hashtable_hash_half_t hashtable_mcmp_support_hash_half(hashtable_hash_t hash) {
    return (hash >> 32u) | 0x80000000u;
}

hashtable_hash_quarter_t hashtable_mcmp_support_hash_quarter(hashtable_hash_half_t hash_half) {
    return hash_half & 0xFFFFu;
}
