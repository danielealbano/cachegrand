#ifndef CACHEGRAND_HASHTABLE_H
#define CACHEGRAND_HASHTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HASHTABLE_USE_UINT64
#define HASHTABLE_USE_UINT64    1
#endif

// By default, don't inline keys, although it might improve memory consumption it will hit hard to the memory allocator
// which require currently some minimal locking wasting a lot of cycles just to save up to 32 bytes
#ifndef HASHTABLE_FLAG_ALLOW_KEY_INLINE
#define HASHTABLE_FLAG_ALLOW_KEY_INLINE 0
#endif

#define HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT    14
#define HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX          32

#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
#define HASHTABLE_KEY_INLINE_MAX_LENGTH                 22
#endif

#define HASHTABLE_TO_CHUNK_INDEX(bucket_index) \
    ((bucket_index) / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT)
#define HASHTABLE_TO_CHUNK_SLOT_INDEX(bucket_index) \
    ((bucket_index) % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT)
#define HASHTABLE_TO_BUCKET_INDEX(chunk_index, chunk_slot_index) \
    (((chunk_index) * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) + (chunk_slot_index))

typedef uint8_t hashtable_key_value_flags_t;
typedef uint64_t hashtable_hash_t;
typedef uint32_t hashtable_hash_half_t;
typedef uint16_t hashtable_hash_quarter_t;
#if HASHTABLE_USE_UINT64 == 1
typedef uint64_t hashtable_bucket_index_t;
typedef uint64_t hashtable_chunk_index_t;
#else
typedef uint32_t hashtable_bucket_index_t;
typedef uint32_t hashtable_chunk_index_t;
#endif
typedef uint8_t hashtable_chunk_slot_index_t;
typedef hashtable_bucket_index_t hashtable_bucket_count_t;
typedef hashtable_chunk_index_t hashtable_chunk_count_t;
typedef uint32_t hashtable_key_size_t;
typedef _Volatile(hashtable_key_size_t) hashtable_key_size_volatile_t;
typedef char hashtable_key_data_t;
typedef _Volatile(hashtable_key_data_t) hashtable_key_data_volatile_t;
typedef uintptr_t hashtable_value_data_t;

typedef _Volatile(uint32_t) hashtable_slot_id_volatile_t;
typedef _Volatile(hashtable_hash_half_t) hashtable_hash_half_volatile_t;
typedef _Volatile(hashtable_hash_quarter_t) hashtable_hash_quarter_volatile_t;

enum {
    HASHTABLE_KEY_VALUE_FLAG_DELETED         = 0x01u,
    HASHTABLE_KEY_VALUE_FLAG_FILLED          = 0x02u,
    HASHTABLE_KEY_VALUE_FLAG_KEY_INLINE      = 0x04u,
};

#define HASHTABLE_KEY_VALUE_HAS_FLAG(flags, flag) \
    (((flags) & (hashtable_key_value_flags_t)(flag)) == (hashtable_key_value_flags_t)(flag))
#define HASHTABLE_KEY_VALUE_SET_FLAG(flags, flag) \
    flags |= (hashtable_key_value_flags_t)flag
#define HASHTABLE_KEY_VALUE_IS_EMPTY(flags) \
    ((flags) == 0)


/**
 * Configuration of the hashtable
 */
typedef struct hashtable_config hashtable_config_t;
struct hashtable_config {
    hashtable_bucket_count_t initial_size;
    bool can_auto_resize;
    bool numa_aware;
    struct bitmask* numa_nodes_bitmask;
};

/**
 * Struct holding the information related to the key/value data
 *
 * The key can be stored inline-ed if short enough (there are 23 bytes for it) or it can entirely be stored externally
 * in ad-hoc allocated memory if needed.
 * The struct is aligned to 32 byte to ensure to fit the first half or the second half of a cache-line
 */
typedef struct hashtable_key_value hashtable_key_value_t;
typedef _Volatile(hashtable_key_value_t) hashtable_key_value_volatile_t;
struct hashtable_key_value {
    union {                                     // union 23 bytes (HASHTABLE_KEY_INLINE_MAX_LENGTH must match this size)
        struct {
            hashtable_key_size_volatile_t size;          // 4 bytes
            hashtable_key_data_volatile_t* data;         // 8 bytes
        } __attribute__((packed)) external_key;
#if HASHTABLE_FLAG_ALLOW_KEY_INLINE == 1
        struct {
            uint8_t size;
            hashtable_key_data_t data[HASHTABLE_KEY_INLINE_MAX_LENGTH];
        } __attribute__((packed)) inline_key;
#endif
    };

    hashtable_key_value_flags_t flags;

    hashtable_value_data_t data;                // 8 byte
} __attribute__((aligned(32)));

/**
 * Struct holding the chunks used to store the half hashes end the metadata. The overflowed_keys_count has been
 * borrowed from F14, as well the number of slots in the half hashes chunk
 */
typedef union {
    hashtable_slot_id_volatile_t slot_id;
    struct {
        bool filled;
        uint8_t distance;
        hashtable_hash_quarter_volatile_t quarter_hash;
    };
} hashtable_slot_id_wrapper_t;

typedef struct hashtable_half_hashes_chunk hashtable_half_hashes_chunk_t;
typedef _Volatile(hashtable_half_hashes_chunk_t) hashtable_half_hashes_chunk_volatile_t;
struct hashtable_half_hashes_chunk {
    transaction_spinlock_lock_volatile_t write_lock;
    union {
        uint32_t padding;
        struct {
            uint8_volatile_t overflowed_chunks_counter;
            uint8_volatile_t slots_occupied;
            uint8_volatile_t is_full;
        };
    } metadata;
    hashtable_slot_id_wrapper_t half_hashes[HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT];
} __attribute__((aligned(64)));

typedef struct hashtable_counters hashtable_counters_t;
typedef _Volatile(hashtable_counters_t) hashtable_counters_volatile_t;
struct hashtable_counters {
    int64_t size;
};

/**
 * Struct holding the hashtable data
 **/
typedef struct hashtable_data hashtable_data_t;
typedef _Volatile(hashtable_data_t) hashtable_data_volatile_t;
struct hashtable_data {
    hashtable_bucket_count_t buckets_count;
    hashtable_bucket_count_t buckets_count_real;
    hashtable_chunk_count_t chunks_count;
    bool can_be_deleted;
    size_t half_hashes_chunk_size;
    size_t keys_values_size;
    struct {
        spinlock_lock_volatile_t lock;
        uint32_volatile_t size;
        hashtable_counters_volatile_t **list;
    } thread_counters;
    hashtable_half_hashes_chunk_volatile_t* half_hashes_chunk;
    hashtable_key_value_volatile_t* keys_values;
};

/**
 * Struct holding the hashtable
 *
 * This has to be initialized with a call to hashtable_mcmp_init.
 *
 * During the normal operations only ht_1 or ht_2 actually contains the hashtable data and ht_current points to the
 * current one.
 * During the resize both are in use, the not-used one is initialized with a new hashtable, ht_old updated
 * to point to the same address in ht_current and ht_current updated to point to the newly initialized hashtable. At
 * the end of the resize, the is_resizing is updated to false, then the system waits for all the threads to finish their
 * work on the buckets in that hashtable and then ht_old is updated to point to null and all the data structures
 * associated are freed.
 **/
typedef struct hashtable hashtable_t;
struct hashtable {
    hashtable_config_t* config;
    hashtable_data_volatile_t* ht_current;
    hashtable_data_volatile_t* ht_old;
    bool is_resizing;
};

typedef struct hashtable_mcmp_op_rmw_transaction hashtable_mcmp_op_rmw_status_t;
struct hashtable_mcmp_op_rmw_transaction {
    hashtable_hash_t hash;
    hashtable_t *hashtable;
    transaction_t *transaction;
    hashtable_half_hashes_chunk_volatile_t *half_hashes_chunk;
    hashtable_key_value_volatile_t *key_value;
    hashtable_key_data_t *key;
    hashtable_key_size_t key_size;
    hashtable_chunk_index_t chunk_index;
    hashtable_chunk_slot_index_t chunk_slot_index;
    bool created_new;
    uintptr_t current_value;
};

hashtable_t* hashtable_mcmp_init(hashtable_config_t* hashtable_config);
void hashtable_mcmp_free(hashtable_t* hashtable);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_H
