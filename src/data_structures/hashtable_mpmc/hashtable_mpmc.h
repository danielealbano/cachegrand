#ifndef CACHEGRAND_HASHTABLE_MPMC_H
#define CACHEGRAND_HASHTABLE_MPMC_H

#ifdef __cplusplus
extern "C" {
#endif

#define HASHTABLE_MPMC_HASH_SEED 42U
#define HASHTABLE_MPMC_LINEAR_SEARCH_RANGE 256

#define HASHTABLE_MPMC_POINTER_TAG_TEMPORARY (0x01)
#define HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE (0x02)
#define HASHTABLE_MPMC_POINTER_TAG_MASK (0x07)
#define HASHTABLE_MPMC_POINTER_TAG_MASK_INVERTED (~HASHTABLE_MPMC_POINTER_TAG_MASK)

#define HASHTABLE_MPMC_BUCKET_IS_TEMPORARY(BUCKET) \
    ((((uintptr_t)(BUCKET).data.key_value & HASHTABLE_MPMC_POINTER_TAG_TEMPORARY) > 0))
#define HASHTABLE_MPMC_BUCKET_IS_TOMBSTONE(BUCKET) \
    ((((uintptr_t)(BUCKET).data.key_value & HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE) > 0))
#define HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(BUCKET) \
    ((hashtable_mpmc_data_key_value_volatile_t*)(((uintptr_t)(BUCKET).data.key_value & HASHTABLE_MPMC_POINTER_TAG_MASK_INVERTED)))

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
    size_t struct_size;
    hashtable_mpmc_data_bucket_t buckets[];
};

typedef struct hashtable_mpmc hashtable_mpmc_t;
struct hashtable_mpmc {
    hashtable_mpmc_data_t *data;
};

enum hashtable_mpmc_result {
    HASHTABLE_MPMC_RESULT_FALSE,
    HASHTABLE_MPMC_RESULT_TRUE,
    HASHTABLE_MPMC_RESULT_TRY_AGAIN,
};
typedef enum hashtable_mpmc_result hashtable_mpmc_result_t;

void hashtable_mpmc_epoch_gc_object_type_hashtable_key_value_destructor_cb(
        uint8_t staged_objects_count,
        epoch_gc_staged_object_t staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]);

void hashtable_mpmc_thread_operation_queue_init();

void hashtable_mpmc_thread_operation_queue_free();

hashtable_mpmc_hash_t hashtable_mcmp_support_hash_calculate(
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length);

hashtable_mpmc_bucket_index_t hashtable_mpmc_support_bucket_index_from_hash(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash);

hashtable_mpmc_data_t *hashtable_mpmc_data_init(
        uint64_t buckets_count);

void hashtable_mpmc_data_free(
        hashtable_mpmc_data_t *hashtable_mpmc_data);

hashtable_mpmc_t *hashtable_mpmc_init(
        uint64_t buckets_count);

void hashtable_mpmc_free(
        hashtable_mpmc_t *hashtable_mpmc);

hashtable_mpmc_hash_half_t hashtable_mpmc_support_hash_half(
        hashtable_mpmc_hash_t hash);

hashtable_mpmc_result_t hashtable_mpmc_support_find_bucket_and_key_value(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash,
        hashtable_mpmc_hash_half_t hash_half,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        bool allow_temporary,
        hashtable_mpmc_data_bucket_t *return_bucket,
        hashtable_mpmc_bucket_index_t *return_bucket_index);

hashtable_mpmc_result_t hashtable_mpmc_support_acquire_empty_bucket_for_insert(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash,
        hashtable_mpmc_hash_half_t hash_half,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t value,
        hashtable_mpmc_data_key_value_t **new_key_value,
        hashtable_mpmc_data_bucket_t *overridden_bucket,
        hashtable_mpmc_bucket_index_t *new_bucket_index);

hashtable_mpmc_result_t hashtable_mpmc_op_get(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t *return_value);

hashtable_mpmc_result_t hashtable_mpmc_op_delete(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length);

hashtable_mpmc_result_t hashtable_mpmc_op_set(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t value,
        bool *return_created_new,
        bool *return_value_updated,
        uintptr_t *return_previous_value);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_MPMC_H
