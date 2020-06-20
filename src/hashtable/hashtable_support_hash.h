#ifndef CACHEGRAND_HASHTABLE_SUPPORT_HASH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_T1HA_SEED    42U

hashtable_hash_t hashtable_support_hash_calculate(
        hashtable_key_data_t *key, hashtable_key_size_t key_size);

hashtable_hash_half_t hashtable_support_hash_half(
        hashtable_hash_t hash);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_HASH_H
