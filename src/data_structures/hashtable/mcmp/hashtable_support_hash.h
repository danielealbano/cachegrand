#ifndef CACHEGRAND_HASHTABLE_SUPPORT_HASH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_SUPPORT_HASH_SEED    42U

#define HASHTABLE_MCMP_SUPPORT_HASH_ALGORITHM_XXH3_STR       "xxh3"
#define HASHTABLE_MCMP_SUPPORT_HASH_ALGORITHM_T1HA2_STR      "t1ha2"
#define HASHTABLE_MCMP_SUPPORT_HASH_ALGORITHM_CRC32C_STR     "crc32c"

#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
#define HASHTABLE_SUPPORT_HASH_NAME             HASHTABLE_MCMP_SUPPORT_HASH_ALGORITHM_T1HA2_STR
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
#define HASHTABLE_SUPPORT_HASH_NAME             HASHTABLE_MCMP_SUPPORT_HASH_ALGORITHM_XXH3_STR
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
#define HASHTABLE_SUPPORT_HASH_NAME             HASHTABLE_MCMP_SUPPORT_HASH_ALGORITHM_CRC32C_STR
#endif

#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
#include "t1ha.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
#include "xxhash.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
#include "hash/hash_crc32c.h"
#else
#error "Unsupported hash algorithm"
#endif

static inline hashtable_hash_t hashtable_mcmp_support_hash_calculate(
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size) {
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

static inline hashtable_hash_half_t hashtable_mcmp_support_hash_half(
        hashtable_hash_t hash) {
    return (hash >> 32u) | 0x80000000u;
}

static inline hashtable_hash_quarter_t hashtable_mcmp_support_hash_quarter(
        hashtable_hash_half_t hash_half) {
    return hash_half & 0xFFFFu;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_HASH_H
