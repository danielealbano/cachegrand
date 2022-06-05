#ifndef CACHEGRAND_FIXTURES_HASHTABLE_H
#define CACHEGRAND_FIXTURES_HASHTABLE_H

#define HASHTABLE_SUPPORT_HASH_SEED    42U

#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
#include "t1ha.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
#include "xxhash.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
#include "hash/hash_crc32c.h"
#else
#error "Unsupported hash algorithm"
#endif

#ifdef __cplusplus
namespace
{
#endif


// Fixtures
uintptr_t test_value_1 = 12345;
uintptr_t test_value_2 = 54321;

uint64_t buckets_initial_count_5 = 5;
uint64_t buckets_initial_count_100 = 100;
uint64_t buckets_initial_count_305 = 305;

uint64_t buckets_count_42 = 42;
uint64_t buckets_count_101 = 101;
uint64_t buckets_count_307 = 307;

char test_key_same_bucket_key_prefix_external[] = "same_bucket_key_not_inline_";
char test_key_same_bucket_key_prefix_inline[] = "sb_key_inline_";

char test_key_1[32] = "test key 1";
hashtable_key_size_t test_key_1_len = 10;
#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
    hashtable_hash_t test_key_1_hash = (hashtable_hash_t)t1ha2_atonce(test_key_1, test_key_1_len, HASHTABLE_SUPPORT_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
    hashtable_hash_t test_key_1_hash = (hashtable_hash_t)XXH3_64bits_withSeed(test_key_1, test_key_1_len, HASHTABLE_SUPPORT_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32 == 1
    uint32_t crc32 = hash_crc32c(test_key_1, test_key_1_len, HASHTABLE_SUPPORT_HASH_SEED);
    hashtable_hash_t test_key_1_hash = ((uint64_t)hash_crc32c(test_key_1, test_key_1_len, crc32) << 32u) | crc32;
#endif
hashtable_hash_half_t test_key_1_hash_half = (test_key_1_hash >> 32u) | 0x80000000u;
hashtable_hash_quarter_t test_key_1_hash_quarter = test_key_1_hash_half & 0xFFFF;

char test_key_2[32] = "test key 2";
hashtable_key_size_t test_key_2_len = 10;
#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
    hashtable_hash_t test_key_2_hash = (hashtable_hash_t)t1ha2_atonce(test_key_2, test_key_2_len, HASHTABLE_SUPPORT_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
    hashtable_hash_t test_key_2_hash = (hashtable_hash_t)XXH3_64bits_withSeed(test_key_2, test_key_2_len, HASHTABLE_SUPPORT_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32 == 1
    uint32_t crc32 = hash_crc32c(test_key_2, test_key_2_len, HASHTABLE_SUPPORT_HASH_SEED);
    hashtable_hash_t test_key_2_hash = ((uint64_t)hash_crc32c(test_key_2, test_key_2_len, crc32) << 32u) | crc32;
#endif
hashtable_hash_half_t test_key_2_hash_half = (test_key_2_hash >> 32u) | 0x80000000u;
hashtable_hash_quarter_t test_key_2_hash_quarter = test_key_2_hash_half & 0xFFFF;

#define HASHTABLE_DATA(buckets_count_v, ...) \
{ \
    hashtable_data_t* hashtable_data = hashtable_mcmp_data_init(buckets_count_v); \
    __VA_ARGS__; \
    hashtable_mcmp_data_free(hashtable_data); \
}

#define HASHTABLE_INIT(initial_size_v, can_auto_resize_v) \
    hashtable_config_t* hashtable_config = hashtable_mcmp_config_init();  \
    hashtable_config->initial_size = initial_size_v; \
    hashtable_config->can_auto_resize = can_auto_resize_v; \
    \
    hashtable_t* hashtable = hashtable_mcmp_init(hashtable_config); \

#define HASHTABLE_INIT_NUMA_AWARE(initial_size_v, can_auto_resize_v, numa_nodes_bitmask_v) \
    hashtable_config_t* hashtable_config = hashtable_mcmp_config_init();  \
    hashtable_config->initial_size = initial_size_v; \
    hashtable_config->can_auto_resize = can_auto_resize_v; \
    hashtable_config->numa_aware = true; \
    hashtable_config->numa_nodes_bitmask = numa_nodes_bitmask_v; \
    \
    hashtable_t* hashtable = hashtable_mcmp_init(hashtable_config); \

#define HASHTABLE_FREE() \
    if (hashtable) {  \
        hashtable_mcmp_free(hashtable); \
    }

#define HASHTABLE(initial_size_v, can_auto_resize_v, ...) \
{ \
    HASHTABLE_INIT(initial_size_v, can_auto_resize_v); \
    \
    __VA_ARGS__ \
    \
    HASHTABLE_FREE(); \
}

#define HASHTABLE_NUMA_AWARE(initial_size_v, can_auto_resize_v, numa_nodes_bitmask_v, ...) \
{ \
    HASHTABLE_INIT_NUMA_AWARE(initial_size_v, can_auto_resize_v, numa_nodes_bitmask_v); \
    \
    __VA_ARGS__ \
    \
    HASHTABLE_FREE(); \
}

#define HASHTABLE_TO_CHUNK_INDEX(bucket_index) \
    (int)(bucket_index / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT)
#define HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index) \
    (chunk_index * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + chunk_slot_index

#define HASHTABLE_HALF_HASHES_CHUNK(chunk_index) \
    hashtable->ht_current->half_hashes_chunk[chunk_index]
#define HASHTABLE_KEYS_VALUES(chunk_index, chunk_slot_index) \
    hashtable->ht_current->keys_values[HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index)]

#define HASHTABLE_SET_INDEX_SHARED(chunk_index, chunk_slot_index, hash, value) \
    HASHTABLE_HALF_HASHES_CHUNK(chunk_index).half_hashes[chunk_slot_index].quarter_hash = \
        (hash >> 32u) & 0xFFFFu; \
    HASHTABLE_HALF_HASHES_CHUNK(chunk_index).half_hashes[chunk_slot_index].distance = 0; \
    HASHTABLE_HALF_HASHES_CHUNK(chunk_index).half_hashes[chunk_slot_index].filled = true; \
    HASHTABLE_KEYS_VALUES(chunk_index, chunk_slot_index).data = value;

#define HASHTABLE_SET_KEY_INLINE_BY_INDEX(chunk_index, chunk_slot_index, hash, key, key_size, value) \
    HASHTABLE_SET_INDEX_SHARED(chunk_index, chunk_slot_index, hash, value); \
    HASHTABLE_KEYS_VALUES(chunk_index, chunk_slot_index).flags = \
        HASHTABLE_KEY_VALUE_FLAG_FILLED | HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE; \
    strncpy((char*)&HASHTABLE_KEYS_VALUES(chunk_index, chunk_slot_index).inline_key.data, key, HASHTABLE_KEY_INLINE_MAX_LENGTH); \
    HASHTABLE_KEYS_VALUES(chunk_index, chunk_slot_index).inline_key.size = key_size;

#define HASHTABLE_SET_KEY_EXTERNAL_BY_INDEX(chunk_index, chunk_slot_index, hash, key, key_size, value) \
    HASHTABLE_SET_INDEX_SHARED(chunk_index, chunk_slot_index, hash, value); \
    HASHTABLE_KEYS_VALUES(chunk_index, chunk_slot_index).flags = \
        HASHTABLE_KEY_VALUE_FLAG_FILLED; \
    HASHTABLE_KEYS_VALUES(chunk_index, chunk_slot_index).external_key.data = \
        key; \
    HASHTABLE_KEYS_VALUES(chunk_index, chunk_slot_index).external_key.size = \
        key_size;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FIXTURES_HASHTABLE_H
