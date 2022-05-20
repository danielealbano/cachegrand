#ifndef CACHEGRAND_STORAGE_DB_H
#define CACHEGRAND_STORAGE_DB_H

#ifdef __cplusplus
extern "C" {
#endif

//#define STORAGE_DB_SHARD_BLOCK_MAX_SIZE (64 * 1024)
//#define STORAGE_DB_SHARD_VERSION 1
//#define STORAGE_DB_SHARD_MAGIC_NUMBER_HIGH 0x4341434845475241
//#define STORAGE_DB_SHARD_MAGIC_NUMBER_LOW  0x5241000000000000


#define STORAGE_DB_CHUNK_MAX_SIZE (64 * 1024)

typedef uint32_t storage_db_chunk_index_t;
typedef uint16_t storage_db_chunk_length_t;
typedef uint32_t storage_db_chunk_offset_t;
typedef uint32_t storage_db_shard_index_t;

//// contains the necessary information to manage the db, holds a pointer to storage_db_config required during the
//// the initialization
//typedef struct storage_db_shard storage_db_shard_t;
//struct storage_db_shard {
//    uint16_t version;
//    uint32_t index;
//    size_t size;
//    struct {
//        uint64_t sec;
//        uint64_t nsec;
//    } creation_time;
//};

enum storage_db_backend_type {
    STORAGE_DB_BACKEND_TYPE_UNKNOWN = 0,
    STORAGE_DB_BACKEND_TYPE_MEMORY = 1,
    STORAGE_DB_BACKEND_TYPE_FILE = 2,
};
typedef enum storage_db_backend_type storage_db_backend_type_t;

// general config parameters to initialize and use the internal storage db (e.g. storage backend, amount of memory for
// the hashtable, other optional stuff)
typedef struct storage_db_config storage_db_config_t;
struct storage_db_config {
    storage_db_backend_type_t backend_type;
    hashtable_bucket_count_t max_keys;
    union {
        struct {
            char *basedir_path;
            size_t shard_size_mb;
        } file;
    } backend;
};

typedef struct storage_db_shard storage_db_shard_t;
struct storage_db_shard {
    storage_db_shard_index_t index;
    size_t offset;
    size_t size;
    storage_channel_t *storage_channel;
    char* path;
};

// contains the necessary information to manage the db, holds a pointer to storage_db_config required during the
// the initialization
typedef struct storage_db storage_db_t;
struct storage_db {
    struct {
        storage_db_shard_t **active_per_worker;
        double_linked_list_t *opened_shards;
        storage_db_shard_index_t new_index;
        spinlock_lock_volatile_t write_spinlock;
    } shards;
    hashtable_t *hashtable;
    storage_db_config_t *config;
};

// For performance reasons shards should never be bigger than a few hundred of MBs, having an uin32 allows to have
// offsets up to 4 GB
typedef struct storage_db_chunk_info storage_db_chunk_info_t;
struct storage_db_chunk_info {
    storage_channel_t *shard_storage_channel;
    storage_db_chunk_offset_t chunk_offset;
    storage_db_chunk_length_t chunk_length;
};

typedef struct storage_db_entry_index storage_db_entry_index_t;
struct storage_db_entry_index {
    size_t key_length;
    size_t value_length;
    storage_db_chunk_index_t key_chunks_count;
    storage_db_chunk_index_t value_chunks_count;
    storage_db_chunk_info_t *key_chunks_info;
    storage_db_chunk_info_t *value_chunks_info;
};

char *storage_db_shard_build_path(
        char *basedir_path,
        storage_db_shard_index_t shard_index);

storage_db_config_t* storage_db_config_new() ;

void storage_db_config_free(
        storage_db_config_t* config);

storage_db_t* storage_db_new(
        storage_db_config_t *config,
        uint32_t workers_count);

storage_db_shard_t *storage_db_new_active_shard(
        storage_db_t *db,
        uint32_t worker_index);

storage_db_shard_t *storage_db_new_active_shard_per_current_worker(
        storage_db_t *db);

storage_db_shard_t* storage_db_shard_new(
        storage_db_shard_index_t index,
        char *path,
        uint32_t shard_size_mb);

        storage_channel_t *storage_db_shard_open_or_create_file(
        char *path,
        bool create);

bool storage_db_open(
        storage_db_t *db);
bool storage_db_close(
        storage_db_t *db);

void storage_db_free(
        storage_db_t *db);

storage_db_shard_t *storage_db_shard_get_active_per_current_worker(
        storage_db_t *db);

bool storage_db_shard_new_is_needed(
        storage_db_shard_t *shard,
        size_t chunk_length);

bool storage_db_shard_allocate_chunk(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        size_t chunk_length);

storage_db_entry_index_t *storage_db_entry_index_new();

storage_db_entry_index_t *storage_db_entry_index_allocate_key_chunks(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index,
        size_t key_length);

storage_db_entry_index_t *storage_db_entry_index_allocate_value_chunks(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index,
        size_t value_length);

storage_db_entry_index_t *storage_db_entry_index_free(
        storage_db_entry_index_t *entry_index);

bool storage_db_entry_chunk_read(
        storage_db_chunk_info_t *chunk_info,
        char *buffer);

bool storage_db_entry_chunk_write(
        storage_db_chunk_info_t *chunk_info,
        char *buffer);

storage_db_chunk_info_t *storage_db_entry_key_chunk_get(
        storage_db_entry_index_t* entry_index,
        storage_db_chunk_index_t chunk_index) ;

storage_db_chunk_info_t *storage_db_entry_value_chunk_get(
        storage_db_entry_index_t* entry_index,
        storage_db_chunk_index_t chunk_index);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_DB_H
