#ifndef CACHEGRAND_HASHTABLE_MPMC_H
#define CACHEGRAND_HASHTABLE_MPMC_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_MPMC_HASH_SEED 42U
#define HASHTABLE_MPMC_LINEAR_SEARCH_RANGE 256

typedef char hashtable_mpmc_key_t;
typedef uint16_t hashtable_mpmc_key_length_t;

typedef uint64_t hashtable_mpmc_bucket_index_t;

typedef uint64_t hashtable_mpmc_hash_t;
typedef uint32_t hashtable_mpmc_hash_half_t;
typedef uint16_t hashtable_mpmc_hash_quarter_t;

typedef _Volatile(hashtable_mpmc_hash_t) hashtable_mpmc_hash_volatile_t;
typedef _Volatile(hashtable_mpmc_hash_half_t) hashtable_mpmc_hash_half_volatile_t;
typedef _Volatile(hashtable_mpmc_hash_quarter_t) hashtable_mpmc_hash_quarter_volatile_t;

typedef struct hashtable_mpmc_data_key_value hashtable_mpmc_data_key_value_t;
typedef _Volatile(hashtable_mpmc_data_key_value_t) hashtable_mpmc_data_key_value_volatile_t;
struct hashtable_mpmc_data_key_value {
    union {
        struct {
            char key[15];
            uint8_t key_length;
        } embedded;
        struct {
            char *key;
            hashtable_mpmc_key_length_t key_length;
            // The compiler is going to pad this space anyway, add 2 reserved field to get to 16 bytes to ensure that
            // we can always safely assume the total is 16 bytes so the embedded key can be 15 chars without the need
            // of the null terminator.
            uint32_t reserved_1;
            uint16_t reserved_2;
        } external;
    } key;
    uintptr_t value;
    uint64_t hash;
    bool key_is_embedded;
};

typedef union hashtable_mpmc_data_bucket hashtable_mpmc_data_bucket_t;
union hashtable_mpmc_data_bucket {
    uint128_volatile_t _packed;
    struct {
        transaction_id_t transaction_id;
        hashtable_mpmc_hash_half_volatile_t hash_half;
        hashtable_mpmc_data_key_value_volatile_t *key_value;
    } data;
};

typedef struct hashtable_mpmc_data hashtable_mpmc_data_t;
struct hashtable_mpmc_data {
    uint64_t buckets_count;
    uint64_t buckets_count_real;
    uint64_t buckets_count_mask;
    hashtable_mpmc_data_bucket_t buckets[];
};

typedef struct hashtable_mpmc hashtable_mpmc_t;
struct hashtable_mpmc {
    hashtable_mpmc_data_t *data;
};

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_MPMC_H
