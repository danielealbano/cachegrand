/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <numa.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "log/log.h"

#include "hashtable.h"
#include "hashtable_support_op.h"
#include "hashtable_support_op_arch.h"

#if !defined(__aarch64__)
#if defined(__x86_64__)
#define HASHTABLE_MCMP_SUPPORT_OP_FUNC_RESOLVER(FUNC) \
    static void* FUNC##_resolve(void) { \
        __builtin_cpu_init(); \
        LOG_DI("Selecting optimal " #FUNC); \
        \
        LOG_DI("    CPU FOUND: %s", "X64"); \
        LOG_DI(">     HAS AVX: %s", __builtin_cpu_supports("avx") ? "yes" : "no"); \
        LOG_DI(">    HAS AVX2: %s", __builtin_cpu_supports("avx2") ? "yes" : "no"); \
        LOG_DI("> HAS AVX512F: %s", __builtin_cpu_supports("avx512f") ? "yes" : "no"); \
        \
        if (CACHEGRAND_CMAKE_CONFIG_ENABLE_SUPPORT_AVX512F && __builtin_cpu_supports("avx512f")) { \
            LOG_DI("Selecting AVX512F"); \
            return HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(FUNC, avx512f); \
        } else if (__builtin_cpu_supports("avx2")) { \
            LOG_DI("Selecting AVX2"); \
            return HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(FUNC, avx2); \
        } else if (__builtin_cpu_supports("avx")) { \
            LOG_DI("Selecting AVX"); \
            return HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(FUNC, avx); \
        } \
        \
        LOG_DI("No optimized function available for the current architecture, switching to generic"); \
        \
        return HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(FUNC, loop); \
    }
#else
#define HASHTABLE_MCMP_SUPPORT_OP_FUNC_RESOLVER(FUNC) \
    static void* FUNC##_resolve(void) { \
        return HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(FUNC, loop); \
    }
#endif

bool hashtable_mcmp_support_op_search_key(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value)
__attribute__ ((ifunc ("hashtable_mcmp_support_op_search_key_resolve")));
HASHTABLE_MCMP_SUPPORT_OP_FUNC_RESOLVER(hashtable_mcmp_support_op_search_key)

bool hashtable_mcmp_support_op_search_key_or_create_new(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        transaction_t *transaction,
        bool *created_new,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_half_hashes_chunk_volatile_t **found_half_hashes_chunk,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value)
__attribute__ ((ifunc ("hashtable_mcmp_support_op_search_key_or_create_new_resolve")));
HASHTABLE_MCMP_SUPPORT_OP_FUNC_RESOLVER(hashtable_mcmp_support_op_search_key_or_create_new)
#else

bool hashtable_mcmp_support_op_search_key(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value) {
    return HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(hashtable_mcmp_support_op_search_key_or_create_new, armv8a_neon)(
            hashtable_data,
            key,
            key_size,
            hash,
            found_chunk_index,
            found_chunk_slot_index,
            found_key_value
    );
}

bool hashtable_mcmp_support_op_search_key_or_create_new(
        hashtable_data_volatile_t *hashtable_data,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_hash_t hash,
        bool create_new_if_missing,
        transaction_t *transaction,
        bool *created_new,
        hashtable_chunk_index_t *found_chunk_index,
        hashtable_half_hashes_chunk_volatile_t **found_half_hashes_chunk,
        hashtable_chunk_slot_index_t *found_chunk_slot_index,
        hashtable_key_value_volatile_t **found_key_value) {
    return HASHTABLE_MCMP_SUPPORT_OP_FUNC_METHOD(hashtable_mcmp_support_op_search_key_or_create_new, armv8a_neon)(
        hashtable_data,
        key,
        key_size,
        hash,
        create_new_if_missing,
        transaction,
        created_new,
        found_chunk_index,
        found_half_hashes_chunk,
        found_chunk_slot_index,
        found_key_value);
}
#endif