#ifndef CACHEGRAND_HASHTABLE_SUPPORT_HASH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_SUPPORT_HASH_SEED    42U

#define HASHTABLE_SUPPORT_HASH_ALGORITHM_T1HA2_STR      "T1HA2"
#define HASHTABLE_SUPPORT_HASH_ALGORITHM_CRC32C_STR     "CRC32C"

#if CACHEGRAND_CMAKE_CONFIG_USE_HASHTABLE_HASH_ALGORITHM_T1HA2 == 1
#define HASHTABLE_SUPPORT_HASH_NAME             HASHTABLE_SUPPORT_HASH_ALGORITHM_T1HA2_STR
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASHTABLE_HASH_ALGORITHM_CRC32C == 1
#define HASHTABLE_SUPPORT_HASH_NAME             HASHTABLE_SUPPORT_HASH_ALGORITHM_CRC32C_STR
#endif

hashtable_hash_t hashtable_support_hash_calculate(
        hashtable_key_data_t *key, hashtable_key_size_t key_size);

hashtable_hash_half_t hashtable_support_hash_half(
        hashtable_hash_t hash);

hashtable_hash_quarter_t hashtable_support_hash_quarter(
        hashtable_hash_half_t hash_half);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_HASH_H
