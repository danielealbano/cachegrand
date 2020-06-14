#ifndef CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_H
#define CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_SUPPORT_OP_FUNC_METHOD(FUNCTION, SUFFIX) \
    FUNCTION##_##SUFFIX

#define HASHTABLE_SUPPORT_OP_ARCH(SUFFIX) \
    bool HASHTABLE_SUPPORT_OP_FUNC_METHOD(hashtable_support_op_search_key, SUFFIX)( \
            volatile hashtable_data_t *hashtable_data, \
            hashtable_key_data_t *key, \
            hashtable_key_size_t key_size, \
            hashtable_hash_t hash, \
            volatile hashtable_key_value_t **found_key_value); \
     \
    bool HASHTABLE_SUPPORT_OP_FUNC_METHOD(hashtable_support_op_search_key_or_create_new, SUFFIX)( \
            volatile hashtable_data_t *hashtable_data, \
            hashtable_key_data_t *key, \
            hashtable_key_size_t key_size, \
            hashtable_hash_t hash, \
            bool create_new_if_missing, \
            bool *created_new, \
            hashtable_half_hashes_chunk_atomic_t **found_half_hashes_chunk, \
            volatile hashtable_key_value_t **found_key_value); \
     \
    bool HASHTABLE_SUPPORT_OP_FUNC_METHOD(hashtable_support_op_half_hashes_chunk_lock, SUFFIX)( \
            hashtable_half_hashes_chunk_atomic_t *half_hashes_chunk, \
            bool retry); \
     \
    void HASHTABLE_SUPPORT_OP_FUNC_METHOD(hashtable_support_op_half_hashes_chunk_unlock, SUFFIX)( \
            hashtable_half_hashes_chunk_atomic_t *half_hashes_chunk); \

#if defined(__x86_64__)
HASHTABLE_SUPPORT_OP_ARCH(avx2)
HASHTABLE_SUPPORT_OP_ARCH(avx)
HASHTABLE_SUPPORT_OP_ARCH(sse42)
HASHTABLE_SUPPORT_OP_ARCH(genericx64)
#endif

HASHTABLE_SUPPORT_OP_ARCH(noopt)

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_SUPPORT_OP_ARCH_H
