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

hashtable_bucket_hash_t test_hash_zero = 0;

char test_key_1[] = "test key 1";
hashtable_key_size_t test_key_1_len = 10;
hashtable_bucket_hash_t test_key_1_hash = (hashtable_bucket_hash_t)0xf1bdcc8aaccb614c;
hashtable_bucket_index_t test_index_1_buckets_count_42 = test_key_1_hash % buckets_count_42;

char test_key_1_kv_1[] = "test key 1_same_bucket_42_0036";
hashtable_key_size_t test_key_1_kv_1_len = 30;
hashtable_bucket_hash_t test_key_1_kv_1_hash = (hashtable_bucket_hash_t)4186772687967241104u;

char test_key_1_kv_2[] = "test key 1_same_bucket_42_0092";
hashtable_key_size_t test_key_1_kv_2_len = 30;
hashtable_bucket_hash_t test_key_1_kv_2_hash = (hashtable_bucket_hash_t)254592058724637408u;

char test_key_1_kv_3[] = "test key 1_same_bucket_42_0118";
hashtable_key_size_t test_key_1_kv_3_len = 30;
hashtable_bucket_hash_t test_key_1_kv_3_hash = (hashtable_bucket_hash_t)2040653589763571256u;

char test_key_1_kv_4[] = "test key 1_same_bucket_42_0197";
hashtable_key_size_t test_key_1_kv_4_len = 30;
hashtable_bucket_hash_t test_key_1_kv_4_hash = (hashtable_bucket_hash_t)11775188599986301740u;

char test_key_1_kv_5[] = "test key 1_same_bucket_42_0201";
hashtable_key_size_t test_key_1_kv_5_len = 30;
hashtable_bucket_hash_t test_key_1_kv_5_hash = (hashtable_bucket_hash_t)6332775395539396410u;

char test_key_2[] = "test key 2";
hashtable_key_size_t test_key_2_len = 10;
hashtable_bucket_hash_t test_key_2_hash = (hashtable_bucket_hash_t)0x8c8b1b670da1324d;
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

#define HASHTABLE_BUCKET_CHAIN_RING_NEW() \
({ \
    hashtable_bucket_chain_ring_t* var; \
    var = (hashtable_bucket_chain_ring_t*)xalloc_alloc(sizeof(hashtable_bucket_chain_ring_t)); \
    memset(var, 0, sizeof(hashtable_bucket_chain_ring_t)); \
    var; \
})

#define HASHTABLE_BUCKET_CHAIN_RING_FREE(chain_ring) \
    xalloc_free(chain_ring);

#define HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_SHARED(chain_ring, chain_ring_index, hash, value) \
    chain_ring->half_hashes[chain_ring_index] = hash >> 32u; \
    chain_ring->keys_values[chain_ring_index].data = value;

#define HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(chain_ring, chain_ring_index, hash, key, key_size, value) \
    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_SHARED(chain_ring, chain_ring_index, hash, value); \
    chain_ring->keys_values[chain_ring_index].flags = \
        HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED | HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE; \
    strncpy((char*)&chain_ring->keys_values[chain_ring_index].inline_key.data, key, HASHTABLE_KEY_INLINE_MAX_LENGTH);

#define HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_EXTERNAL(chain_ring, chain_ring_index, hash, key, key_size, value) \
    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_SHARED(chain_ring, chain_ring_index, hash, value); \
    chain_ring->keys_values[chain_ring_index].flags = \
        HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED; \
    chain_ring->keys_values[chain_ring_index].external_key.data = key; \
    chain_ring->keys_values[chain_ring_index].external_key.size = key_size; \
    strncpy((char*)&chain_ring->keys_values[chain_ring_index].external_key.key_prefix, key, HASHTABLE_KEY_EXTERNAL_PREFIX_SIZE);

#define HASHTABLE_BUCKET_NEW_KEY_INLINE(bucket_index, hash, key, key_size, value) \
    hashtable_bucket_chain_ring_t* chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW(); \
    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_INLINE(chain_ring, 1, hash, key, key_size, value); \
    hashtable->ht_current->buckets[bucket_index].chain_first_ring = chain_ring;

#define HASHTABLE_BUCKET_NEW_KEY_EXTERNAL(bucket_index, hash, key, key_size, value) \
    hashtable_bucket_chain_ring_t* chain_ring = HASHTABLE_BUCKET_CHAIN_RING_NEW(); \
    HASHTABLE_BUCKET_CHAIN_RING_SET_INDEX_KEY_EXTERNAL(chain_ring, 1, hash, key, key_size, value); \
    hashtable->ht_current->buckets[bucket_index].chain_first_ring = chain_ring;

#define HASHTABLE_BUCKET_NEW_CLEANUP(bucket_index) \
    HASHTABLE_BUCKET_CHAIN_RING_FREE(hashtable->ht_current->buckets[bucket_index]->chain_first_ring);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FIXTURES_HASHTABLE_H
