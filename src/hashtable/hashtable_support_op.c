#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "log.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_op.h"
#include "hashtable/hashtable_support_op_arch.h"

#if defined(__x86_64__)
#define HASHTABLE_SUPPORT_OP_FUNC_RESOLVER(FUNC) \
    static void* FUNC##_resolve(void) { \
        __builtin_cpu_init(); \
        LOG_DI("Selecting optimal " #FUNC); \
        \
        LOG_DI("   CPU FOUND: %s", "X64"); \
        LOG_DI(">   HAS SSE3: %s", __builtin_cpu_supports("sse3") ? "yes" : "no"); \
        LOG_DI("> HAS SSE4.2: %s", __builtin_cpu_supports("sse4.2") ? "yes" : "no"); \
        LOG_DI(">    HAS AVX: %s", __builtin_cpu_supports("avx") ? "yes" : "no"); \
        LOG_DI(">   HAS AVX2: %s", __builtin_cpu_supports("avx2") ? "yes" : "no"); \
        \
        if (__builtin_cpu_supports("avx2")) { \
            LOG_DI("Selecting AVX2"); \
            return HASHTABLE_SUPPORT_OP_FUNC_METHOD(FUNC, avx2); \
        } else if (__builtin_cpu_supports("avx")) { \
            LOG_DI("Selecting AVX"); \
            return HASHTABLE_SUPPORT_OP_FUNC_METHOD(FUNC, avx); \
        } else if (__builtin_cpu_supports("sse4.2")) { \
            LOG_DI("Selecting SSE4.2"); \
            return HASHTABLE_SUPPORT_OP_FUNC_METHOD(FUNC, sse42); \
        } else if (__builtin_cpu_supports("sse3")) { \
            LOG_DI("Selecting SSE3"); \
            return HASHTABLE_SUPPORT_OP_FUNC_METHOD(FUNC, sse3); \
        } \
        \
        LOG_DI("No optimized function available for the current achitecture, switching to generic"); \
        \
        return HASHTABLE_SUPPORT_OP_FUNC_METHOD(FUNC, defaultopt); \
    }
#else
#define HASHTABLE_SUPPORT_OP_FUNC_RESOLVER(FUNC) \
    static void* FUNC##_resolve(void) { \
        return HASHTABLE_SUPPORT_OP_FUNC_METHOD(FUNC, defaultopt); \
    }
#endif

bool hashtable_support_op_search_key(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        volatile hashtable_key_value_t **found_key_value)
__attribute__ ((ifunc ("hashtable_support_op_search_key_resolve")));
HASHTABLE_SUPPORT_OP_FUNC_RESOLVER(hashtable_support_op_search_key)

bool hashtable_support_op_search_key_or_create_new(
        volatile hashtable_data_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        bool *created_new,
        hashtable_half_hashes_chunk_atomic_t **found_half_hashes_chunk,
        volatile hashtable_key_value_t **found_key_value)
__attribute__ ((ifunc ("hashtable_support_op_search_key_or_create_new_resolve")));
HASHTABLE_SUPPORT_OP_FUNC_RESOLVER(hashtable_support_op_search_key_or_create_new)

bool hashtable_support_op_half_hashes_chunk_lock(
        hashtable_half_hashes_chunk_atomic_t *half_hashes_chunk,
        bool retry)
__attribute__ ((ifunc ("hashtable_support_op_half_hashes_chunk_lock_resolve")));
HASHTABLE_SUPPORT_OP_FUNC_RESOLVER(hashtable_support_op_half_hashes_chunk_lock)

void hashtable_support_op_half_hashes_chunk_unlock(
        hashtable_half_hashes_chunk_atomic_t *half_hashes_chunk)
__attribute__ ((ifunc ("hashtable_support_op_half_hashes_chunk_unlock_resolve")));
HASHTABLE_SUPPORT_OP_FUNC_RESOLVER(hashtable_support_op_half_hashes_chunk_unlock)
