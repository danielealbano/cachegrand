#ifndef CACHEGRAND_HASHTABLE_H
#define CACHEGRAND_HASHTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) && defined(__has_feature)
#if !__has_feature(c_atomic)

#endif
#else
#define _Atomic(T) T volatile
#endif

#define HASHTABLE_MEMORY_FENCE_LOAD() atomic_thread_fence(memory_order_acquire)
#define HASHTABLE_MEMORY_FENCE_STORE() atomic_thread_fence(memory_order_release)
#define HASHTABLE_MEMORY_FENCE_LOAD_STORE() atomic_thread_fence(memory_order_acq_rel)

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define HASHTABLE_INLINE_KEY_MAX_SIZE                       23
#define HASHTABLE_PRIMENUMBERS_COUNT                        38
#define HASHTABLE_PRIMENUMBERS_MAX                          4294967291U

#define HASHTABLE_PRIMENUMBERS_LIST \
    42U, /* not a prime number, but it's the answer! */ \
    101U, \
    307U, \
    677U, \
    1523U, \
    3389U, \
    7639U, \
    17203U, \
    26813U, \
    40213U, \
    60353U, \
    90529U, \
    135799U, \
    203669U, \
    305581U, \
    458377U, \
    687581U, \
    1031399U, \
    1547101U, \
    2320651U, \
    5221501U, \
    7832021U, \
    11748391U, \
    17622551U, \
    26433887U, \
    39650833U, \
    59476253U, \
    89214403U, \
    133821599U, \
    200732527U, \
    301099033U, \
    451649113U, \
    677472127U, \
    1016208581U, \
    1524312899U, \
    2286469357U, \
    3429704039U, \
    4294967291U


typedef uint8_t hashtable_bucket_key_value_flags_t;
typedef uint32_t hashtable_bucket_hash_t;
typedef uint32_t hashtable_bucket_index_t;
typedef hashtable_bucket_index_t hashtable_bucket_count_t;
typedef uint32_t hashtable_key_size_t;
typedef char hashtable_key_data_t;
typedef uintptr_t hashtable_value_data_t;
typedef uint8_t hashtable_search_key_or_create_new_ret_t;

typedef _Atomic(hashtable_bucket_hash_t) hashtable_bucket_hash_atomic_t;

enum {
    HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED         = 0x01u,
    HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED          = 0x02u,
    HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE      = 0x04u,
};

enum {
    HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_NO_FREE,
    HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_FOUND,
    HASHTABLE_SEARCH_KEY_OR_CREATE_NEW_RET_EMPTY_OR_DELETED,
};

#define HASHTABLE_BUCKET_KEY_VALUE_HAS_FLAG(flags, flag) \
    ((flags & (hashtable_bucket_key_value_flags_t)flag) == (hashtable_bucket_key_value_flags_t)flag)
#define HASHTABLE_BUCKET_KEY_VALUE_SET_FLAG(flags, flag) \
    flags |= (hashtable_bucket_key_value_flags_t)flag
#define HASHTABLE_BUCKET_KEY_VALUE_IS_EMPTY(flags) \
    (flags == 0)

/**
 * Configuration of the hashtable
 */
typedef struct hashtable_config hashtable_config_t;
struct hashtable_config {
    uint64_t initial_size;
    bool can_auto_resize;
};

/**
 * Struct holding the information related to a bucket (flags, key, value).
 *
 * The key can be stored inlined (there are 23 bytes for it) or stored externally in ad-hoc allocated memory if needed.
 * The struct is aligned to 32 byte to ensure to fit the first half or the second half of a cacheline
 */
typedef struct hashtable_bucket_key_value hashtable_bucket_key_value_t;
struct hashtable_bucket_key_value {
    hashtable_bucket_key_value_flags_t flags;

    union {                                     // union 23 bytes (HASHTABLE_INLINE_KEY_MAX_SIZE must match this size)
        struct {
            hashtable_key_size_t size;          // 4 bytes
            hashtable_key_data_t* data;         // 8 bytes
        } __attribute__((packed)) external_key;
        struct {
            hashtable_key_data_t data[HASHTABLE_INLINE_KEY_MAX_SIZE];
        } inline_key;
    };

    hashtable_value_data_t data;                // 8 byte
} __attribute__((aligned(32)));

/**
 * Struct holding the hashtable data
 **/
typedef struct hashtable_data hashtable_data_t;
struct hashtable_data {
    hashtable_bucket_count_t buckets_count;
    hashtable_bucket_count_t buckets_count_real;
    uint64_t t1ha2_seed;
    bool can_be_deleted;
    size_t hashes_size;
    size_t keys_values_size;
    hashtable_bucket_hash_atomic_t* hashes;
    hashtable_bucket_key_value_t* volatile keys_values;
};

/**
 * Struct holding the hashtable
 *
 * This has to be initialized with a call to hashtable_init.
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
    bool is_shutdowning;
    bool is_resizing;
    volatile hashtable_data_t* ht_current;
    volatile hashtable_data_t* ht_old;
};

hashtable_t* hashtable_init(hashtable_config_t* hashtable_config);
void hashtable_free(hashtable_t* hashtable);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_H
