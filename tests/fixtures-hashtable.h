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

typedef struct test_key_same_bucket test_key_same_bucket_t;
struct test_key_same_bucket {
    const char* key;
    hashtable_key_size_t key_len;
    hashtable_bucket_hash_t key_hash;
    hashtable_bucket_hash_half_t key_hash_half;
};

test_key_same_bucket_t test_key_1_same_bucket[] = {
    { "test key 1_same_bucket_42_0036", 30, 0x3a1a6793c5984390, 0x3a1a6793 }, // match n. 1
    { "test key 1_same_bucket_42_0092", 30, 0x03887e20e6eadae0, 0x03887e20 }, // match n. 2
    { "test key 1_same_bucket_42_0118", 30, 0x1c51dba0c77bbe38, 0x1c51dba0 }, // match n. 3
    { "test key 1_same_bucket_42_0197", 30, 0xa369dfa4e169ab2c, 0xa369dfa4 }, // match n. 4
    { "test key 1_same_bucket_42_0201", 30, 0x57e289ab7575a73a, 0x57e289ab }, // match n. 5
    { "test key 1_same_bucket_42_0207", 30, 0xdbae2432f8ed753e, 0xdbae2432 }, // match n. 6
    { "test key 1_same_bucket_42_0218", 30, 0x0c0ed27de374bc7a, 0x0c0ed27d }, // match n. 7
    { "test key 1_same_bucket_42_0341", 30, 0x91a0132bf462567e, 0x91a0132b }, // match n. 8
    { "test key 1_same_bucket_42_0349", 30, 0xe56c946e6b9813b4, 0xe56c946e }, // match n. 9
    { "test key 1_same_bucket_42_0398", 30, 0xe41de88bd413c22a, 0xe41de88b }, // match n. 10
    { "test key 1_same_bucket_42_0429", 30, 0x4aee1521c64a17e0, 0x4aee1521 }, // match n. 11
    { "test key 1_same_bucket_42_0470", 30, 0x3277baf71196d346, 0x3277baf7 }, // match n. 12
    { "test key 1_same_bucket_42_0530", 30, 0xe66714bdf2aafcb4, 0xe66714bd }, // match n. 13
    { "test key 1_same_bucket_42_0556", 30, 0xebac069200629f06, 0xebac0692 }, // match n. 14
    { "test key 1_same_bucket_42_0557", 30, 0xe494be9a586d6d7e, 0xe494be9a }, // match n. 15
    { "test key 1_same_bucket_42_0561", 30, 0xcfc203062c2954c0, 0xcfc20306 }, // match n. 16
    { "test key 1_same_bucket_42_0579", 30, 0x288a94b8695698fe, 0x288a94b8 }, // match n. 17
    { "test key 1_same_bucket_42_0652", 30, 0xba123c897ad07e2e, 0xba123c89 }, // match n. 18
    { "test key 1_same_bucket_42_0663", 30, 0x390f6330d912acca, 0x390f6330 }, // match n. 19
    { "test key 1_same_bucket_42_0669", 30, 0xfc14fa325168f272, 0xfc14fa32 }, // match n. 20
    { nullptr, 0, 0, 0 }
};

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

#define HASHTABLE_BUCKET_CHAIN_RING_NEW() \
({ \
    hashtable_bucket_chain_ring_t* var; \
    var = (hashtable_bucket_chain_ring_t*)xalloc_alloc(sizeof(hashtable_bucket_chain_ring_t)); \
    memset(var, 0, sizeof(hashtable_bucket_chain_ring_t)); \
    var; \
})

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
    chain_ring->keys_values[chain_ring_index].prefix_key.size = key_size; \
    strncpy((char*)&chain_ring->keys_values[chain_ring_index].prefix_key.data, key, HASHTABLE_KEY_PREFIX_SIZE);

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
