#ifndef CACHEGRAND_FIXTURES_HASHTABLE_H
#define CACHEGRAND_FIXTURES_HASHTABLE_H

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

char test_key_1[] = "test key 1";
hashtable_key_size_t test_key_1_len = 10;
hashtable_bucket_hash_t test_key_1_hash = (hashtable_bucket_hash_t)0xf1bdcc8aaccb614c;
hashtable_bucket_hash_half_t test_key_1_hash_half = test_key_1_hash >> 32u;
hashtable_bucket_index_t test_index_1_buckets_count_42 = test_key_1_hash % buckets_count_42;

char test_key_2[] = "test key 2";
hashtable_key_size_t test_key_2_len = 10;
hashtable_bucket_hash_t test_key_2_hash = (hashtable_bucket_hash_t)0x8c8b1b670da1324d;
hashtable_bucket_hash_half_t test_key_2_hash_half = test_key_2_hash >> 32u;
hashtable_bucket_index_t test_index_2_buckets_count_42 = test_key_2_hash % buckets_count_42;

#define HASHTABLE_DATA(buckets_count_v, ...) \
{ \
    hashtable_data_t* hashtable_data = hashtable_data_init(buckets_count_v); \
    __VA_ARGS__; \
    hashtable_data_free(hashtable_data); \
}

#define HASHTABLE_INIT(initial_size_v, can_auto_resize_v) \
    hashtable_config_t* hashtable_config = hashtable_config_init();  \
    hashtable_config->initial_size = initial_size_v; \
    hashtable_config->can_auto_resize = can_auto_resize_v; \
    \
    hashtable_t* hashtable = hashtable_init(hashtable_config); \

#define HASHTABLE_FREE() \
    hashtable_free(hashtable); \

#define HASHTABLE(initial_size_v, can_auto_resize_v, ...) \
{ \
    HASHTABLE_INIT(initial_size_v, can_auto_resize_v); \
    \
    __VA_ARGS__ \
    \
    HASHTABLE_FREE(); \
}

#define HASHTABLE_BUCKET(bucket_index) hashtable->ht_current->buckets[bucket_index]

#if HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0
#define HASHTABLE_BUCKET_KEYS_VALUES_NEW(bucket_index) \
{ \
    hashtable_bucket_key_value_t* keys_values; \
    keys_values = (hashtable_bucket_key_value_t*)xalloc_alloc(sizeof(hashtable_bucket_key_value_t) * HASHTABLE_BUCKET_SLOTS_COUNT); \
    memset(keys_values, 0, sizeof(hashtable_bucket_key_value_t)); \
    HASHTABLE_BUCKET(bucket_index).keys_values = keys_values; \
}
#else
#define HASHTABLE_BUCKET_KEYS_VALUES_NEW(bucket_index) \
{}
#endif // HASHTABLE_BUCKET_FEATURE_EMBED_KEYS_VALUES == 0

#define HASHTABLE_BUCKET_SET_INDEX_SHARED(bucket_index, bucket_slot_index, hash, value) \
    HASHTABLE_BUCKET(bucket_index).half_hashes[bucket_slot_index] = hash >> 32u; \
    HASHTABLE_BUCKET(bucket_index).keys_values[bucket_slot_index].data = value;

#define HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(bucket_index, bucket_slot_index, hash, key, key_size, value) \
    HASHTABLE_BUCKET_SET_INDEX_SHARED(bucket_index, bucket_slot_index, hash, value); \
    HASHTABLE_BUCKET(bucket_index).keys_values[bucket_slot_index].flags = \
        HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE; \
    strncpy((char*)&HASHTABLE_BUCKET(bucket_index).keys_values[bucket_slot_index].inline_key.data, key, HASHTABLE_KEY_INLINE_MAX_LENGTH);

#define HASHTABLE_BUCKET_SET_INDEX_KEY_EXTERNAL(bucket_index, bucket_slot_index, hash, key, key_size, value) \
    HASHTABLE_BUCKET_SET_INDEX_SHARED(bucket_index, bucket_slot_index, hash, value); \
    HASHTABLE_BUCKET(bucket_index).keys_values[bucket_slot_index].flags = \
        HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED; \
    HASHTABLE_BUCKET(bucket_index).keys_values[bucket_slot_index].external_key.data = key; \
    HASHTABLE_BUCKET(bucket_index).keys_values[bucket_slot_index].external_key.size = key_size; \
    HASHTABLE_BUCKET(bucket_index).keys_values[bucket_slot_index].prefix_key.size = key_size; \
    strncpy((char*)&HASHTABLE_BUCKET(bucket_index).keys_values[bucket_slot_index].prefix_key.data, key, HASHTABLE_KEY_PREFIX_SIZE);

#define HASHTABLE_BUCKET_NEW_KEY_INLINE(bucket_index, hash, key, key_size, value) \
    HASHTABLE_BUCKET_KEYS_VALUES_NEW(bucket_index); \
    HASHTABLE_BUCKET_SET_INDEX_KEY_INLINE(bucket_index, 0, hash, key, key_size, value); \

#define HASHTABLE_BUCKET_NEW_KEY_EXTERNAL(bucket_index, hash, key, key_size, value) \
    HASHTABLE_BUCKET_KEYS_VALUES_NEW(bucket_index); \
    HASHTABLE_BUCKET_SET_INDEX_KEY_EXTERNAL(bucket_index, 0, hash, key, key_size, value); \

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FIXTURES_HASHTABLE_H
