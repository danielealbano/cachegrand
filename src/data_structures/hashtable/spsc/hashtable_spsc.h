#ifndef CACHEGRAND_HASHTABLE_SPSC_H
#define CACHEGRAND_HASHTABLE_SPSC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <misc.h>
#include "hash/hash_fnv1.h"

#define HASHTABLE_SPSC_DEFAULT_MAX_RANGE (24)
#define HASHTABLE_SPSC_HASH_MASK (0x7FFFFFFF)

#define HASHTABLE_SPSC_HASH(hash) ((hash) & HASHTABLE_SPSC_HASH_MASK)

typedef int64_t hashtable_spsc_bucket_index_t;
typedef uint32_t hashtable_spsc_bucket_count_t;
typedef uint32_t hashtable_spsc_cmp_hash_t;
typedef uint16_t hashtable_spsc_key_length_t;

typedef struct hashtable_spsc_bucket hashtable_spsc_bucket_t;
struct hashtable_spsc_bucket {
    hashtable_spsc_key_length_t key_length;
    union {
        const char *key;
        uint32_t key_uint32;
        uint64_t key_uint64;
    };
    void *value;
};

typedef struct hashtable_spsc_hash hashtable_spsc_hash_t;
struct hashtable_spsc_hash {
    bool set:1;
    hashtable_spsc_cmp_hash_t cmp_hash:31;
};

typedef struct hashtable_spsc hashtable_spsc_t;
struct hashtable_spsc {
    hashtable_spsc_bucket_count_t  buckets_count;
    hashtable_spsc_bucket_count_t  buckets_count_pow2;
    hashtable_spsc_bucket_count_t  buckets_count_real;
    uint16_t max_range;
    bool free_keys_on_deallocation;
    hashtable_spsc_hash_t hashes[0];
    hashtable_spsc_bucket_t buckets[0];
};

static inline __attribute__((always_inline)) hashtable_spsc_bucket_t *hashtable_spsc_get_buckets(
        hashtable_spsc_t *hashtable) {
    return (hashtable_spsc_bucket_t *)(hashtable->hashes + hashtable->buckets_count_real);
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_bucket_index_from_hash(
        hashtable_spsc_t *hashtable,
        uint32_t hash) {
    return hash & (hashtable->buckets_count_pow2 - 1);
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_find_empty_bucket(
        hashtable_spsc_t *hashtable,
        hashtable_spsc_bucket_count_t bucket_index,
        hashtable_spsc_bucket_count_t bucket_index_max) {
    do {
        if (!hashtable->hashes[bucket_index].set) {
            return bucket_index;
        }
    } while(++bucket_index < bucket_index_max);

    return -1;
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_find_set_bucket(
        hashtable_spsc_t *hashtable,
        hashtable_spsc_bucket_count_t bucket_index,
        hashtable_spsc_bucket_count_t bucket_index_max) {
    do {
        if (hashtable->hashes[bucket_index].set) {
            return bucket_index;
        }
    } while(++bucket_index < bucket_index_max);

    return -1;
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_find_bucket_index_by_key_ci(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    hashtable_spsc_bucket_index_t bucket_index;
    hashtable_spsc_bucket_count_t bucket_index_max;

    hashtable_spsc_cmp_hash_t cmp_hash = HASHTABLE_SPSC_HASH(hash);

    bucket_index = hashtable_spsc_bucket_index_from_hash(hashtable, hash);
    bucket_index_max = bucket_index + hashtable->max_range;

    do {
        bucket_index = hashtable_spsc_find_set_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (unlikely(bucket_index == -1)) {
            break;
        }

        if (likely(hashtable->hashes[bucket_index].cmp_hash != cmp_hash)) {
            continue;
        }

        hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

        if (unlikely(buckets[bucket_index].key_length != key_length)) {
            continue;
        }

        if (unlikely(strncasecmp(buckets[bucket_index].key, key, key_length) != 0)) {
            continue;
        }

        return bucket_index;
    } while(++bucket_index);

    return -1;
}

static inline __attribute__((always_inline)) void *hashtable_spsc_op_get_ci(
        hashtable_spsc_t *hashtable,
        const char* key_ci,
        hashtable_spsc_key_length_t key_ci_length) {
    uint32_t hash = fnv_32_hash_ci((void *)key_ci, key_ci_length);

    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index_by_key_ci(
            hashtable,
            hash,
            key_ci,
            key_ci_length);

    if (unlikely(bucket_index == -1)) {
        return NULL;
    }

    return hashtable_spsc_get_buckets(hashtable)[bucket_index].value;
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_find_bucket_index_by_key_cs(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    hashtable_spsc_bucket_index_t bucket_index;
    hashtable_spsc_bucket_count_t bucket_index_max;

    hashtable_spsc_cmp_hash_t cmp_hash = HASHTABLE_SPSC_HASH(hash);

    bucket_index = hashtable_spsc_bucket_index_from_hash(hashtable, hash);
    bucket_index_max = bucket_index + hashtable->max_range;

    do {
        bucket_index = hashtable_spsc_find_set_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (unlikely(bucket_index == -1)) {
            break;
        }

        if (likely(hashtable->hashes[bucket_index].cmp_hash != cmp_hash)) {
            continue;
        }

        hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

        if (unlikely(buckets[bucket_index].key_length != key_length)) {
            continue;
        }

        if (unlikely(strncmp(buckets[bucket_index].key, key, key_length) != 0)) {
            continue;
        }

        return bucket_index;
    } while(++bucket_index);

    return -1;
}

static inline __attribute__((always_inline)) void *hashtable_spsc_op_get_by_hash(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        const char* key_cs,
        hashtable_spsc_key_length_t key_cs_length) {
    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index_by_key_cs(
            hashtable,
            hash,
            key_cs,
            key_cs_length);

    if (unlikely(bucket_index == -1)) {
        return NULL;
    }

    return hashtable_spsc_get_buckets(hashtable)[bucket_index].value;
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_find_bucket_index_by_key_uint32(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        uint32_t key_uint32) {
    hashtable_spsc_bucket_index_t bucket_index;
    hashtable_spsc_bucket_count_t bucket_index_max;
    hashtable_spsc_cmp_hash_t cmp_hash = HASHTABLE_SPSC_HASH(hash);
    hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

    assert(hashtable->free_keys_on_deallocation == false);

    bucket_index = hashtable_spsc_bucket_index_from_hash(hashtable, hash);
    bucket_index_max = bucket_index + hashtable->max_range;

    do {
        bucket_index = hashtable_spsc_find_set_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (unlikely(bucket_index == -1)) {
            break;
        }

        if (likely(hashtable->hashes[bucket_index].cmp_hash != cmp_hash)) {
            continue;
        }


        if (unlikely(key_uint32 != buckets[bucket_index].key_uint32)) {
            continue;
        }

        return bucket_index;
    } while(++bucket_index);

    return -1;
}

static inline __attribute__((always_inline)) void *hashtable_spsc_op_get_by_hash_and_key_uint32(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        uint32_t key_uint32) {
    assert(hashtable->free_keys_on_deallocation == false);

    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index_by_key_uint32(
            hashtable,
            hash,
            key_uint32);

    if (unlikely(bucket_index == -1)) {
        return NULL;
    }

    return hashtable_spsc_get_buckets(hashtable)[bucket_index].value;
}

static inline __attribute__((always_inline)) hashtable_spsc_bucket_index_t hashtable_spsc_find_bucket_index_by_key_uint64(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        uint64_t key_uint64) {
    hashtable_spsc_bucket_index_t bucket_index;
    hashtable_spsc_bucket_count_t bucket_index_max;
    hashtable_spsc_cmp_hash_t cmp_hash = HASHTABLE_SPSC_HASH(hash);
    hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

    assert(hashtable->free_keys_on_deallocation == false);

    bucket_index = hashtable_spsc_bucket_index_from_hash(hashtable, hash);
    bucket_index_max = bucket_index + hashtable->max_range;

    do {
        bucket_index = hashtable_spsc_find_set_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (unlikely(bucket_index == -1)) {
            break;
        }

        if (likely(hashtable->hashes[bucket_index].cmp_hash != cmp_hash)) {
            continue;
        }


        if (unlikely(key_uint64 != buckets[bucket_index].key_uint64)) {
            continue;
        }

        return bucket_index;
    } while(++bucket_index);

    return -1;
}

static inline __attribute__((always_inline)) void *hashtable_spsc_op_get_by_hash_and_key_uint64(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        uint64_t key_uint64) {
    assert(hashtable->free_keys_on_deallocation == false);

    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index_by_key_uint64(
            hashtable,
            hash,
            key_uint64);

    if (unlikely(bucket_index == -1)) {
        return NULL;
    }

    return hashtable_spsc_get_buckets(hashtable)[bucket_index].value;
}

static inline __attribute__((always_inline)) void *hashtable_spsc_op_get_cs(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    uint32_t hash = fnv_32_hash((void *)key, key_length);
    return hashtable_spsc_op_get_by_hash(hashtable, hash, key, key_length);
}

hashtable_spsc_t *hashtable_spsc_new(
        hashtable_spsc_bucket_count_t buckets_count,
        uint16_t max_range,
        bool free_keys_on_deallocation);

void hashtable_spsc_free(
        hashtable_spsc_t *hashtable);

hashtable_spsc_t* hashtable_spsc_upsize(
        hashtable_spsc_t *hashtable_current);

bool hashtable_spsc_op_try_set_ci(
        hashtable_spsc_t *hashtable,
        const char *key,
        hashtable_spsc_key_length_t key_length,
        void* value);

bool hashtable_spsc_op_delete_ci(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length);

bool hashtable_spsc_op_try_set_cs(
        hashtable_spsc_t *hashtable,
        const char *key,
        hashtable_spsc_key_length_t key_length,
        void* value);

bool hashtable_spsc_op_delete_cs(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length);

bool hashtable_spsc_op_try_set_by_hash_and_key_uint32(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        uint32_t key_uint32,
        void *value);

bool hashtable_spsc_op_try_set_by_hash_and_key_uint64(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        uint64_t key_uint64,
        void *value);

bool hashtable_spsc_op_delete_by_hash_and_key_uint32(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        uint32_t key_uint32);

bool hashtable_spsc_op_delete_by_hash_and_key_uint64(
        hashtable_spsc_t *hashtable,
        uint32_t hash,
        uint64_t key_uint64);

void *hashtable_spsc_op_iter(
        hashtable_spsc_t *hashtable,
        hashtable_spsc_bucket_index_t *bucket_index);

#ifdef __cplusplus
}
#endif

#endif // CACHEGRAND_HASHTABLE_SPSC_H
