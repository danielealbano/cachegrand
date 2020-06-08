#ifndef CACHEGRAND_HASHTABLE_H
#define CACHEGRAND_HASHTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#define _Atomic(T) T volatile

#define HASHTABLE_MEMORY_FENCE_LOAD() atomic_thread_fence(memory_order_acquire)
#define HASHTABLE_MEMORY_FENCE_STORE() atomic_thread_fence(memory_order_release)
#define HASHTABLE_MEMORY_FENCE_LOAD_STORE() atomic_thread_fence(memory_order_acq_rel)

#define HASHTABLE_BUCKET_CHAIN_RING_SLOTS_COUNT     8
#define HASHTABLE_KEY_INLINE_MAX_LENGTH             23
#define HASHTABLE_KEY_PREFIX_SIZE                   HASHTABLE_KEY_INLINE_MAX_LENGTH \
                                                    - sizeof(hashtable_key_size_t)
#define HASHTABLE_PRIMENUMBERS_COUNT                38
#define HASHTABLE_PRIMENUMBERS_MAX                  4294967291U

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

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

typedef uint8_t hashtable_bucket_key_value_flags_t;
typedef uint8_t hashtable_bucket_chain_ring_flags_t;
typedef uint64_t hashtable_bucket_hash_t;
typedef uint32_t hashtable_bucket_hash_half_t;
typedef uint32_t hashtable_bucket_index_t;
typedef hashtable_bucket_index_t hashtable_bucket_count_t;
typedef uint32_t hashtable_key_size_t;
typedef char hashtable_key_data_t;
typedef uintptr_t hashtable_value_data_t;
typedef uint8_t hashtable_bucket_chain_ring_index_t;

typedef _Atomic(hashtable_bucket_hash_t) hashtable_bucket_hash_atomic_t;
typedef _Atomic(hashtable_bucket_hash_half_t) hashtable_bucket_hash_half_atomic_t;

enum {
    HASHTABLE_BUCKET_KEY_VALUE_FLAG_DELETED         = 0x01u,
    HASHTABLE_BUCKET_KEY_VALUE_FLAG_FILLED          = 0x02u,
    HASHTABLE_BUCKET_KEY_VALUE_FLAG_KEY_INLINE      = 0x04u,
};

enum {
    HASHTABLE_BUCKET_CHAIN_RING_FLAG_FULL           = 0x01u
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
    union {                                     // union 23 bytes (HASHTABLE_KEY_INLINE_MAX_LENGTH must match this size)
        struct {
            hashtable_key_size_t size;          // 4 bytes
            hashtable_key_data_t* data;         // 8 bytes
        } __attribute__((packed)) external_key;
        struct {
            hashtable_key_size_t size;          // 4 bytes
            hashtable_key_data_t data[HASHTABLE_KEY_PREFIX_SIZE];
        } __attribute__((packed)) prefix_key;
        struct {
            hashtable_key_data_t data[HASHTABLE_KEY_INLINE_MAX_LENGTH];
        } __attribute__((packed)) inline_key;
    };

    hashtable_bucket_key_value_flags_t flags;

    hashtable_value_data_t data;                // 8 byte
} __attribute__((aligned(32)));

/**
 * Struct holding a ring of the hashes chain
 *
 * This structure is referenced, through a pointer, from the bucket list in the hashtable or from a pointer in the
 * bucket itself
 */
typedef struct hashtable_bucket_chain_ring hashtable_bucket_chain_ring_t;
struct hashtable_bucket_chain_ring {
    volatile hashtable_bucket_chain_ring_t* next_ring;
    hashtable_bucket_hash_half_atomic_t half_hashes[HASHTABLE_BUCKET_CHAIN_RING_SLOTS_COUNT];
    volatile hashtable_bucket_key_value_t keys_values[HASHTABLE_BUCKET_CHAIN_RING_SLOTS_COUNT];
    hashtable_bucket_chain_ring_flags_t flags;
} __attribute__((aligned(64)));

/**
 * Structure holding a bucket of hashtable
 *
 * It references the first ring of the chain, offer a write lock and a rings_count, the latter should be used only
 * when the write_lock is in use to avoid performing useless atomic operations.
 * To update the bucket in an atomic way, _internal_cmpandxcg is exposed on purpose.
 */

#define HASHTABLE_BUCKET_WRITE_LOCK_CLEAR(var)  var & ~((uint128_t)1 << 120u)
#define HASHTABLE_BUCKET_WRITE_LOCK_SET(var)    var | ((uint128_t)1 << 120u)

typedef struct hashtable_bucket hashtable_bucket_t;
struct hashtable_bucket {
    union {
        uint128_t _internal_cmpandxcg;
        struct {
            volatile hashtable_bucket_chain_ring_t* chain_first_ring;
            uint16_t reserved0;
            uint16_t reserved1;
            uint16_t reserved2;
            uint8_t reserved3;
            uint8_t write_lock;
        } __attribute__((packed));
    } __attribute__((aligned(16)));
};

/**
 * Struct holding the hashtable data
 **/
typedef struct hashtable_data hashtable_data_t;
struct hashtable_data {
    hashtable_bucket_count_t buckets_count;
    bool can_be_deleted;
    size_t buckets_size;
    volatile hashtable_bucket_t* buckets;
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
