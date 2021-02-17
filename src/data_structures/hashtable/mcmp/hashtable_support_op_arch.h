#ifndef CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(FUNCTION, SUFFIX) \
    FUNCTION##_##SUFFIX

#define HASHTABLE_MCMP_SUPPORT_OP_ARCH(SUFFIX) \
    bool HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(hashtable_mcmp_support_op_search_key, SUFFIX)( \
            hashtable_data_volatile_t *hashtable_data, \
            hashtable_key_data_t *key, \
            hashtable_key_size_t key_size, \
            hashtable_hash_t hash, \
            hashtable_chunk_index_t *found_chunk_index, \
            hashtable_chunk_slot_index_t *found_chunk_slot_index, \
            hashtable_key_value_volatile_t **found_key_value); \
     \
    bool HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(hashtable_mcmp_support_op_search_key_or_create_new, SUFFIX)( \
            hashtable_data_t *hashtable_data, \
            hashtable_key_data_t *key, \
            hashtable_key_size_t key_size, \
            hashtable_hash_t hash, \
            bool create_new_if_missing, \
            bool *created_new, \
            hashtable_half_hashes_chunk_volatile_t **found_half_hashes_chunk, \
            hashtable_key_value_volatile_t **found_key_value);

#if defined(__x86_64__)
HASHTABLE_MCMP_SUPPORT_OP_ARCH(avx2)
HASHTABLE_MCMP_SUPPORT_OP_ARCH(avx)
HASHTABLE_MCMP_SUPPORT_OP_ARCH(sse42)
HASHTABLE_MCMP_SUPPORT_OP_ARCH(sse3)
#endif

HASHTABLE_MCMP_SUPPORT_OP_ARCH(defaultopt)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_H
