#ifndef CACHEGRAND_TEST_HASHTABLE_H
#define CACHEGRAND_TEST_HASHTABLE_H

#ifdef __cplusplus
namespace
{
#endif

// Fixtures
char test_key_1[] = "test key 1";
hashtable_key_size_t test_key_1_len = 10;
hashtable_bucket_hash_t test_key_1_hash = 0xf1bdcc8aaccb614c;

char test_key_2[] = "test key 2";
hashtable_key_size_t test_key_2_len = 10;
hashtable_bucket_hash_t test_key_2_hash = 0x8c8b1b670da1324d;

uintptr_t test_value_1 = 12345;
uintptr_t test_value_2 = 54321;

hashtable_bucket_index_t test_index_53 = 51;
hashtable_bucket_index_t test_index_101 = 77;
hashtable_bucket_index_t test_index_307 = 20;

uint64_t buckets_initial_count_5 = 5;
uint64_t buckets_initial_count_100 = 100;
uint64_t buckets_initial_count_305 = 305;

uint64_t buckets_count_53 = 53;
uint64_t buckets_count_101 = 101;
uint64_t buckets_count_307 = 307;

uint64_t buckets_count_real_64 = 64;
uint64_t buckets_count_real_112 = 112;
uint64_t buckets_count_real_320 = 320;

#define HASHTABLE_DATA_INIT(buckets_count_v, ...) \
{ \
    hashtable_data_t* hashtable_data = hashtable_data_init(buckets_count_v); \
    __VA_ARGS__; \
    hashtable_data_free(hashtable_data); \
}

#define HASHTABLE_INIT(initial_size_v, can_auto_resize_v, ...) \
{ \
    hashtable_config_t* hashtable_config = hashtable_config_init();  \
    hashtable_config->initial_size = initial_size_v; \
    hashtable_config->can_auto_resize = can_auto_resize_v; \
    \
    hashtable_t* hashtable = hashtable_init(hashtable_config); \
    \
    __VA_ARGS__ \
    \
    hashtable_free(hashtable); \
}

#define HASHTABLE_BUCKET_HASH_KEY_VALUE_SET_KEY_INLINE(index, hash, key, key_size, value) \
{ \
    hashtable->ht_current->hashes[index] = hash; \
    hashtable->ht_current->keys_values[index].flags = \
        HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE; \
    strncpy((char*)&hashtable->ht_current->keys_values[index].inline_key.data, key, HASHTABLE_INLINE_KEY_MAX_SIZE); \
    hashtable->ht_current->keys_values[index].data = value; \
}

#define HASHTABLE_BUCKET_HASH_KEY_VALUE_SET_KEY_EXTERNAL(index, hash, key, key_size, value) \
{ \
    hashtable->ht_current->hashes[index] = hash; \
    hashtable->ht_current->keys_values[index].flags = \
        HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED; \
    hashtable->ht_current->keys_values[index].external_key.data = key; \
    hashtable->ht_current->keys_values[index].external_key.size = key_size; \
    hashtable->ht_current->keys_values[index].data = value; \
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_TEST_HASHTABLE_H
