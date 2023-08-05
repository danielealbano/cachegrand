#ifndef CACHEGRAND_STORAGE_DB_HASHSET_H
#define CACHEGRAND_STORAGE_DB_HASHSET_H

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_DB_HASHSET_DEFAULT_MAX_RANGE (24)
#define STORAGE_DB_HASHSET_HASH_MASK (0x7FFFFFFF)

#define STORAGE_DB_HASHSET_HASH(hash) ((hash) & STORAGE_DB_HASHSET_HASH_MASK)

typedef int64_t storage_db_hashset_bucket_index_t;
typedef uint32_t storage_db_hashset_bucket_count_t;
typedef uint32_t storage_db_hashset_cmp_hash_t;
typedef uint16_t storage_db_hashset_key_length_t;

struct storage_db_hashset_bucket {
    storage_db_hashset_key_length_t key_length;
    union {
        const char *key;
        uint32_t key_uint32;
        uint64_t key_uint64;
    };
    void *value;
};
typedef struct storage_db_hashset_key_entry storage_db_hashset_key_entry_t;

struct storage_db_hashset_hash_entry {
    bool set:1;
    storage_db_hashset_cmp_hash_t cmp_hash:31;
};
typedef struct storage_db_hashset_hash_entry storage_db_hashset_hash_entry_t;

struct storage_db_hashset_metadata {
    bool initialized;
    uint32_t buckets_count;
    uint32_t buckets_count_pow2;
    off_t hashes_offset;
    off_t keys_offset;
};
typedef struct storage_db_hashset_metadata storage_db_hashset_metadata_t;

struct storage_db_hashset {
    storage_db_chunk_sequence_t *current_chunk_sequence;
    storage_db_chunk_sequence_t *initial_chunk_sequence;
    storage_db_chunk_sequence_t *new_chunk_sequence;

    storage_db_hashset_metadata_t metadata;
};
typedef struct storage_db_hashset storage_db_hashset_t;

static inline __attribute__((always_inline)) void storage_db_hashset_load_metadata(
        storage_db_t *storage_db,
        storage_db_hashset_t *storage_db_hashset) {
    bool allocated_new_buffer = false;
    storage_db_hashset->metadata.initialized = false;

    // If there are no chunks, there can't be metadata, so we can just terminate the execution right away
    if (storage_db_hashset->current_chunk_sequence->count == 0) {
        return;
    }

    // The metadata are always stored at the very beginning of the first chunk, so we can just load it and use it
    // right away
    storage_db_chunk_info_t *chunk = storage_db_chunk_sequence_get(
            storage_db_hashset->current_chunk_sequence,
            0);

    char *data = storage_db_get_chunk_data(storage_db, chunk, &allocated_new_buffer);

    storage_db_hashset_metadata_t *metadata_ptr = (storage_db_hashset_metadata_t *)data;
    storage_db_hashset->metadata.initialized = true;
    storage_db_hashset->metadata.buckets_count = metadata_ptr->buckets_count;
    storage_db_hashset->metadata.buckets_count_pow2 = metadata_ptr->buckets_count_pow2;
    storage_db_hashset->metadata.hashes_offset = metadata_ptr->hashes_offset;
    storage_db_hashset->metadata.keys_offset = metadata_ptr->keys_offset;

    if (allocated_new_buffer) {
        ffma_mem_free(data);
    }
}

static inline __attribute__((always_inline)) void storage_db_hashset_new_from_entry_index(
        storage_db_t *storage_db,
        storage_db_chunk_sequence_t *chunk_sequence,
        storage_db_hashset_t *storage_db_hashset) {
    assert(chunk_sequence != NULL);

    storage_db_hashset->current_chunk_sequence = chunk_sequence;
    storage_db_hashset->initial_chunk_sequence = chunk_sequence;
    storage_db_hashset->new_chunk_sequence = NULL;

    storage_db_hashset_load_metadata(storage_db, storage_db_hashset);
}

static inline __attribute__((always_inline)) void storage_db_hashset_new_empty(
        storage_db_hashset_t *storage_db_hashset) {
    storage_db_hashset->current_chunk_sequence = NULL;
    storage_db_hashset->initial_chunk_sequence = NULL;
    storage_db_hashset->new_chunk_sequence = NULL;
}

static inline __attribute__((always_inline)) storage_db_hashset_hash_entry_t *storage_db_hashset_get_buckets(
        storage_db_hashset_t *storage_db_hashset) {
    return storage_db_hashset->metadata.initialized
           ? (storage_db_hashset_hash_entry_t*)storage_db_hashset->metadata.hashes_offset
           : NULL;
}

static inline __attribute__((always_inline)) storage_db_hashset_bucket_index_t storage_db_hashset_bucket_index_from_hash(
        storage_db_hashset_t *storage_db_hashset,
        uint32_t hash) {
    assert(storage_db_hashset->metadata.initialized);

    return hash & (storage_db_hashset->metadata.buckets_count_pow2 - 1);
}

static inline __attribute__((always_inline)) storage_db_hashset_bucket_index_t storage_db_hashset_find_empty_bucket(
        storage_db_hashset_t *hashtable,
        storage_db_hashset_bucket_count_t bucket_index,
        storage_db_hashset_bucket_count_t bucket_index_max) {
    do {
        if (!hashtable->hashes[bucket_index].set) {
            return bucket_index;
        }
    } while(++bucket_index < bucket_index_max);

    return -1;
}

static inline __attribute__((always_inline)) storage_db_hashset_bucket_index_t storage_db_hashset_find_set_bucket(
        storage_db_hashset_t *hashtable,
        storage_db_hashset_bucket_count_t bucket_index,
        storage_db_hashset_bucket_count_t bucket_index_max) {
    do {
        if (hashtable->hashes[bucket_index].set) {
            return bucket_index;
        }
    } while(++bucket_index < bucket_index_max);

    return -1;
}

static inline __attribute__((always_inline)) storage_db_hashset_bucket_index_t storage_db_hashset_find_bucket_index_by_key_cs(
        storage_db_hashset_t *hashtable,
        uint32_t hash,
        const char* key,
        storage_db_hashset_key_length_t key_length) {
    storage_db_hashset_bucket_index_t bucket_index;
    storage_db_hashset_bucket_count_t bucket_index_max;

    storage_db_hashset_cmp_hash_t cmp_hash = STORAGE_DB_HASHSET_HASH(hash);

    bucket_index = storage_db_hashset_bucket_index_from_hash(hashtable, hash);
    bucket_index_max = bucket_index + hashtable->max_range;

    do {
        bucket_index = storage_db_hashset_find_set_bucket(
                hashtable,
                bucket_index,
                bucket_index_max);

        if (unlikely(bucket_index == -1)) {
            break;
        }

        if (likely(hashtable->hashes[bucket_index].cmp_hash != cmp_hash)) {
            continue;
        }

        storage_db_hashset_bucket_t *buckets = storage_db_hashset_get_buckets(hashtable);

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

static inline __attribute__((always_inline)) void *storage_db_hashset_op_get_by_hash(
        storage_db_hashset_t *hashtable,
        uint32_t hash,
        const char* key_cs,
        storage_db_hashset_key_length_t key_cs_length) {
    storage_db_hashset_bucket_index_t bucket_index = storage_db_hashset_find_bucket_index_by_key_cs(
            hashtable,
            hash,
            key_cs,
            key_cs_length);

    if (unlikely(bucket_index == -1)) {
        return NULL;
    }

    return storage_db_hashset_get_buckets(hashtable)[bucket_index].value;
}

static inline __attribute__((always_inline)) void *storage_db_hashset_op_get_cs(
        storage_db_hashset_t *hashtable,
        const char* key,
        storage_db_hashset_key_length_t key_length) {
    uint32_t hash = fnv_32_hash((void *)key, key_length);
    return storage_db_hashset_op_get_by_hash(hashtable, hash, key, key_length);
}

storage_db_hashset_t *storage_db_hashset_new(
        storage_db_hashset_bucket_count_t buckets_count,
        uint16_t max_range,
        bool free_keys_on_deallocation);

void storage_db_hashset_free(
        storage_db_hashset_t *hashtable);

storage_db_hashset_t* storage_db_hashset_upsize(
        storage_db_hashset_t *hashtable_current);

bool storage_db_hashset_op_try_set_ci(
        storage_db_hashset_t *hashtable,
        const char *key,
        storage_db_hashset_key_length_t key_length,
        void* value);

bool storage_db_hashset_op_delete_ci(
        storage_db_hashset_t *hashtable,
        const char* key,
        storage_db_hashset_key_length_t key_length);

bool storage_db_hashset_op_try_set_cs(
        storage_db_hashset_t *hashtable,
        const char *key,
        storage_db_hashset_key_length_t key_length,
        void* value);

bool storage_db_hashset_op_delete_cs(
        storage_db_hashset_t *hashtable,
        const char* key,
        storage_db_hashset_key_length_t key_length);

bool storage_db_hashset_op_try_set_by_hash_and_key_uint32(
        storage_db_hashset_t *hashtable,
        uint32_t hash,
        uint32_t key_uint32,
        void *value);

void *storage_db_hashset_op_iter(
        storage_db_hashset_t *hashtable,
        storage_db_hashset_bucket_index_t *bucket_index);




#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_DB_HASHSET_H
