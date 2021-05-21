#ifndef CACHEGRAND_STORAGE_H
#define CACHEGRAND_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

enum storage_kval_record_type {
    STORAGE_KVAL_RECORD_TYPE_BLOB,
    STORAGE_KVAL_RECORD_TYPE_KEYVALUE,
};
typedef enum storage_kval_record_type storage_kval_record_type_t;

enum storage_kval_record_flag {
    STORAGE_KVAL_RECORD_FLAG_DATA_BEGIN,
    STORAGE_KVAL_RECORD_FLAG_DATA_END
};
typedef enum storage_kval_record_flag storage_kval_record_flag_t;

typedef struct storage_kval_record_address storage_kval_record_address_t;
struct storage_kval_record_address {
    uint32_t shard_index;
    uint32_t shard_offset;
};

typedef struct storage_kval_record_timestamp storage_kval_record_timestamp_t;
struct storage_kval_record_timestamp {
    uint64_t seconds;
    uint64_t nanoseconds;
};

typedef struct storage_kval_record storage_kval_record_header_t;
struct storage_kval_record_header {
    storage_kval_record_type_t type;
    storage_kval_record_flag_t flags;
    storage_kval_record_timestamp_t timestamp;
    union {
        struct {
            storage_kval_record_address_t previous_record;
            size_t data_length;
            char* data;
        } blob;
        struct {
            hashtable_hash_t hash;
            uint32_t key_record_addresses_count;
            uint32_t value_record_addresses_count;
        } keyvalue;
    };
};

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_H
