#ifndef CACHEGRAND_HASHTABLE_SPSC_H
#define CACHEGRAND_HASHTABLE_SPSC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include "hash/hash_fnv1.h"

#define HASHTABLE_SPSC_DEFAULT_MAX_RANGE 24

typedef int32_t hashtable_spsc_bucket_index_t;
typedef uint16_t hashtable_spsc_bucket_count_t;
typedef uint16_t hashtable_spsc_cmp_hash_t;
typedef uint16_t hashtable_spsc_key_length_t;

typedef struct hashtable_spsc_bucket hashtable_spsc_bucket_t;
struct hashtable_spsc_bucket {
    hashtable_spsc_key_length_t key_length;
    const char *key;
    void *value;
};

typedef struct hashtable_spsc_hash hashtable_spsc_hash_t;
struct hashtable_spsc_hash {
    bool set:1;
    hashtable_spsc_cmp_hash_t cmp_hash:15;
};

typedef struct hashtable_spsc hashtable_spsc_t;
struct hashtable_spsc {
    hashtable_spsc_bucket_count_t  buckets_count;
    hashtable_spsc_bucket_count_t  buckets_count_pow2;
    hashtable_spsc_bucket_count_t  buckets_count_real;
    uint16_t max_range;
    bool stop_on_not_set;
    hashtable_spsc_hash_t hashes[0];
    hashtable_spsc_bucket_t buckets[0];
};

static inline __attribute__((always_inline)) hashtable_spsc_bucket_t *hashtable_spsc_get_buckets(
        hashtable_spsc_t *hashtable) {
    return (hashtable_spsc_bucket_t *)(hashtable->hashes + hashtable->buckets_count_real);
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_find_empty_bucket(
        hashtable_spsc_t *hashtable,
        uint32_t hash) {
    hashtable_spsc_bucket_count_t bucket_start_index, bucket_index;

    bucket_index = bucket_start_index = hash & (hashtable->buckets_count_pow2 - 1);

    do {
        if (!hashtable->hashes[bucket_index].set) {
            return bucket_index;
        }
    } while(++bucket_index - bucket_start_index < hashtable->max_range);

    return -1;
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_find_bucket_index(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    hashtable_spsc_bucket_count_t bucket_start_index, bucket_index;

    assert(key_length < UINT16_MAX);

    hashtable_spsc_cmp_hash_t cmp_hash = hash & 0x7FFF;

    bucket_index = bucket_start_index = hash & (hashtable->buckets_count_pow2 - 1);

    do {
        if (!hashtable->hashes[bucket_index].set) {
            if (hashtable->stop_on_not_set) {
                break;
            }

            continue;
        }

        if (hashtable->hashes[bucket_index].cmp_hash != cmp_hash) {
            continue;
        }

        hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

        if (buckets[bucket_index].key_length != key_length) {
            continue;
        }

        if (strncasecmp(buckets[bucket_index].key, key, key_length) != 0) {
            continue;
        }

        return bucket_index;
    } while(++bucket_index - bucket_start_index < hashtable->max_range);

    return -1;
}

hashtable_spsc_t *hashtable_spsc_new(
        hashtable_spsc_bucket_count_t buckets_count,
        uint16_t max_range,
        bool stop_on_not_set);

void hashtable_spsc_free(
        hashtable_spsc_t *hashtable);

bool hashtable_spsc_op_try_set(
        hashtable_spsc_t *hashtable,
        const char *key,
        hashtable_spsc_key_length_t key_length,
        void* value);

void *hashtable_spsc_op_get(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length);

bool hashtable_spsc_op_delete(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length);

#ifdef __cplusplus
}
#endif

#endif // CACHEGRAND_HASHTABLE_SPSC_H
