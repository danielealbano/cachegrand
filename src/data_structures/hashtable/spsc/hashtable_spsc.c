#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "xalloc.h"
#include "pow2.h"

#include "hashtable_spsc.h"

hashtable_spsc_t *hashtable_spsc_new(
        hashtable_spsc_bucket_count_t buckets_count,
        uint16_t max_range,
        bool stop_on_not_set) {
    hashtable_spsc_bucket_count_t buckets_count_pow2_next, buckets_to_allocate;

    assert(buckets_count > 0);
    assert(max_range > 0);
    assert(buckets_count < UINT16_MAX);

    buckets_count_pow2_next = (uint16_t)pow2_next(buckets_count);
    buckets_to_allocate = buckets_count_pow2_next + max_range;

    assert(buckets_count_pow2_next >= buckets_count);
    assert(buckets_to_allocate >= buckets_count_pow2_next);

    size_t hashtable_size =
            sizeof(hashtable_spsc_t) +
            (sizeof(hashtable_spsc_hash_t) * buckets_to_allocate) +
            (sizeof(hashtable_spsc_bucket_t) * buckets_to_allocate);
    hashtable_spsc_t *hashtable = xalloc_alloc_zero(hashtable_size);

    hashtable->buckets_count = buckets_count;
    hashtable->buckets_count_pow2 = buckets_count_pow2_next;
    hashtable->buckets_count_real = buckets_to_allocate;
    hashtable->max_range = max_range;
    hashtable->stop_on_not_set = stop_on_not_set;

    return hashtable;
}

void hashtable_spsc_free(
        hashtable_spsc_t *hashtable) {
    xalloc_free(hashtable);
}

bool hashtable_spsc_op_try_set(
        hashtable_spsc_t *hashtable,
        const char *key,
        hashtable_spsc_key_length_t key_length,
        void *value) {
    hashtable_spsc_bucket_index_t bucket_index;
    uint32_t hash = fnv_32_hash_ci((void *)key, key_length);

    // Search if there is already a bucket with the same hash and key
    bucket_index = hashtable_spsc_find_bucket_index(hashtable, hash, key, key_length);

    if (bucket_index == -1) {
        // If not search for an empty bucket within the allowed range
        bucket_index = hashtable_spsc_find_empty_bucket(hashtable, hash);

        if (bucket_index > -1) {
            // If an empty bucket was found, update the hash and mark it as in use
            hashtable->hashes[bucket_index].set = true;
            hashtable->hashes[bucket_index].cmp_hash = hash & 0x7fff;
        }
    }

    if (bucket_index == -1) {
        // If it can't find a bucket in the allowed range returns false as it's necessary to upsize the hashtable
        return false;
    }

    hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

    buckets[bucket_index].key = key;
    buckets[bucket_index].key_length = key_length;
    buckets[bucket_index].value = value;

    return true;
}

void *hashtable_spsc_op_get(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    uint32_t hash = fnv_32_hash_ci((void *)key, key_length);
    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index(hashtable, hash, key, key_length);

    if (bucket_index != -1) {
        return hashtable_spsc_get_buckets(hashtable)[bucket_index].value;
    }

    return NULL;
}

bool hashtable_spsc_op_delete(
        hashtable_spsc_t *hashtable,
        const char* key,
        hashtable_spsc_key_length_t key_length) {
    uint32_t hash = fnv_32_hash_ci((void *)key, key_length);
    hashtable_spsc_bucket_index_t bucket_index = hashtable_spsc_find_bucket_index(hashtable, hash, key, key_length);

    if (bucket_index != -1) {
        hashtable->hashes[bucket_index].set = false;
        return true;
    }

    return false;
}
