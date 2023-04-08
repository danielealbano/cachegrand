#ifndef CACHEGRAND_STORAGE_DB_H
#define CACHEGRAND_STORAGE_DB_H

#ifdef __cplusplus
extern "C" {
#endif

//#define STORAGE_DB_SHARD_VERSION 1
//#define STORAGE_DB_SHARD_MAGIC_NUMBER_HIGH 0x4341434845475241
//#define STORAGE_DB_SHARD_MAGIC_NUMBER_LOW  0x5241000000000000

#define STORAGE_DB_SHARD_VERSION (1)
#define STORAGE_DB_CHUNK_MAX_SIZE ((64 * 1024) - 1)
#define STORAGE_DB_WORKERS_MAX (2048)
#define STORAGE_DB_KEYS_EVICTION_BITONIC_SORT_16_ELEMENTS_ARRAY_LENGTH (64)
#define STORAGE_DB_KEYS_EVICTION_EVICT_FIRST_N_KEYS (5)
#define STORAGE_DB_KEYS_EVICTION_ITER_MAX_DISTANCE (5000)
#define STORAGE_DB_KEYS_EVICTION_ITER_MAX_SEARCH_ATTEMPTS (100)

// This magic value defines the size of the ring buffer used to keep in memory data long enough to be sure they are not
// being in use anymore.
#define STORAGE_DB_WORKER_ENTRY_INDEX_RING_BUFFER_SIZE 512

#define STORAGE_DB_ENTRY_NO_EXPIRY (0)

typedef uint16_t storage_db_chunk_index_t;
typedef uint16_t storage_db_chunk_length_t;
typedef uint32_t storage_db_chunk_offset_t;
typedef uint32_t storage_db_shard_index_t;
typedef uint64_t storage_db_create_time_ms_t;
typedef uint64_t storage_db_last_access_time_ms_t;
typedef int64_t storage_db_expiry_time_ms_t;

struct storage_db_keys_eviction_kv_list_entry {
    uint64_t value;
    uint64_t key;
} __attribute__((__aligned__(16)));
typedef struct storage_db_keys_eviction_kv_list_entry storage_db_keys_eviction_kv_list_entry_t;

struct storage_db_limits {
    struct {
        uint64_t soft_limit;
        uint64_t hard_limit;
    } data_size;
    struct {
        hashtable_bucket_count_t soft_limit;
        hashtable_bucket_count_t hard_limit;
    } keys_count;
};
typedef struct storage_db_limits storage_db_limits_t;

typedef int (*storage_db_keys_eviction_list_sort_cb)(const void *, const void*);

struct storage_db_keys_eviction_list_entry {
    uint16_t key_size;
    uint32_t accesses_counters;
    char *key;
    storage_db_last_access_time_ms_t last_access_time_ms;
    storage_db_expiry_time_ms_t expiry_time_ms;
};
typedef struct storage_db_keys_eviction_list_entry storage_db_keys_eviction_list_entry_t;

struct storage_db_counters_slots_bitmap_and_index {
    slots_bitmap_mpmc_t *slots_bitmap;
    uint64_t index;
};
typedef struct storage_db_counters_slots_bitmap_and_index storage_db_counters_slots_bitmap_and_index_t;

enum storage_db_backend_type {
    STORAGE_DB_BACKEND_TYPE_UNKNOWN = 0,
    STORAGE_DB_BACKEND_TYPE_MEMORY = 1,
    STORAGE_DB_BACKEND_TYPE_FILE = 2,
};
typedef enum storage_db_backend_type storage_db_backend_type_t;

enum storage_db_entry_index_value_type {
    STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_UNKNOWN = 1,
    STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STRING = 2,
    STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_LIST = 3,
    STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_HASHSET = 4,
    STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_SORTEDSET = 5
};
typedef enum storage_db_entry_index_value_type storage_db_entry_index_value_type_t;

// general config parameters to initialize and use the internal storage db (e.g. storage backend, amount of memory for
// the hashtable, other optional stuff)
typedef struct storage_db_config storage_db_config_t;
struct storage_db_config {
    storage_db_backend_type_t backend_type;
    storage_db_limits_t limits;
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
    uint32_t version;
    timespec_t creation_time;
};

typedef struct storage_db_worker storage_db_worker_t;
struct storage_db_worker {
    storage_db_shard_t *active_shard;
    ring_bounded_queue_spsc_voidptr_t *deleted_entry_index_ring_buffer;
    double_linked_list_t *deleting_entry_index_list;
};

typedef struct storage_db_counters storage_db_counters_t;
struct storage_db_counters {
    int64_t keys_count;
    int64_t data_size;
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
    storage_db_worker_t *workers;
    uint16_t workers_count;
    storage_db_limits_t limits;
    slots_bitmap_mpmc_t *counters_slots_bitmap;
    storage_db_counters_t counters[STORAGE_DB_WORKERS_MAX];
};

typedef struct storage_db_chunk_info storage_db_chunk_info_t;
struct storage_db_chunk_info {
    union {
        struct {
            storage_channel_t *shard_storage_channel;
            storage_db_chunk_offset_t chunk_offset;
        } file;
        struct {
            void *chunk_data;
        } memory;
    };
    storage_db_chunk_length_t chunk_length;
};

typedef union storage_db_entry_index_status storage_db_entry_index_status_t;
union storage_db_entry_index_status {
    uint64_volatile_t _cas_wrapper;
    struct {
        uint32_volatile_t readers_counter:31;
        bool_volatile_t deleted:1;
        uint32_volatile_t accesses_counter;
    };
};

typedef struct storage_db_chunk_sequence storage_db_chunk_sequence_t;
struct storage_db_chunk_sequence {
    storage_db_chunk_index_t count;
    storage_db_chunk_info_t *sequence;
    size_t size;
};

typedef struct storage_db_entry_index storage_db_entry_index_t;
struct storage_db_entry_index {
    storage_db_entry_index_status_t status;
    storage_db_entry_index_value_type_t value_type:8;
    storage_db_create_time_ms_t created_time_ms;
    storage_db_expiry_time_ms_t expiry_time_ms;
    storage_db_last_access_time_ms_t last_access_time_ms;
    storage_db_chunk_sequence_t key;
    storage_db_chunk_sequence_t value;
};

typedef struct storage_db_op_rmw_transaction storage_db_op_rmw_status_t;
struct storage_db_op_rmw_transaction {
    hashtable_mcmp_op_rmw_status_t hashtable;
    transaction_t *transaction;
    storage_db_entry_index_t *current_entry_index;
    bool delete_entry_index_on_abort;
};

typedef struct storage_db_key_and_key_length storage_db_key_and_key_length_t;
struct storage_db_key_and_key_length {
    char *key;
    size_t key_size;
};

void storage_db_worker_counters_slot_key_init_once();

void storage_db_worker_counters_slot_key_ensure_init(
        storage_db_t *storage_db);

uint64_t storage_db_worker_counters_get_slot_index(
        storage_db_t *storage_db);

uint64_t storage_db_counters_get_current_thread_get_slot_index(
        storage_db_t *storage_db);

storage_db_counters_t* storage_db_counters_get_current_thread_data(
        storage_db_t *storage_db);

void storage_db_counters_sum(
        storage_db_t *storage_db,
        storage_db_counters_t *counters);

char *storage_db_shard_build_path(
        char *basedir_path,
        storage_db_shard_index_t shard_index);

storage_db_config_t* storage_db_config_new();

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
        storage_db_t *db,
        uint32_t workers_count);

storage_db_shard_t *storage_db_worker_active_shard(
        storage_db_t *db);

ring_bounded_queue_spsc_voidptr_t *storage_db_worker_deleted_entry_index_ring_buffer(
        storage_db_t *db);

void storage_db_worker_garbage_collect_deleting_entry_index_when_no_readers(
        storage_db_t *db);

double_linked_list_t *storage_db_worker_deleting_entry_index_list(
        storage_db_t *db);

bool storage_db_shard_new_is_needed(
        storage_db_shard_t *shard,
        size_t chunk_length);

bool storage_db_chunk_data_pre_allocate(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        size_t chunk_length);

void storage_db_chunk_data_free(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info);

storage_db_entry_index_t *storage_db_entry_index_new();

void storage_db_entry_index_chunks_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index);

void storage_db_entry_index_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index);

void storage_db_entry_index_touch(
        storage_db_entry_index_t *entry_index);

storage_db_entry_index_t *storage_db_entry_index_ring_buffer_new(
        storage_db_t *db);

void storage_db_entry_index_ring_buffer_free(
        storage_db_t *db,
        storage_db_entry_index_t *entry_index);

bool storage_db_entry_chunk_can_read_from_memory(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info);

char* storage_db_entry_chunk_read_fast_from_memory(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info);

bool storage_db_chunk_read(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        char *buffer,
        off_t offset,
        size_t length);

bool storage_db_chunk_write(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        off_t chunk_offset,
        char *buffer,
        size_t buffer_length);

size_t storage_db_chunk_sequence_calculate_chunk_count(
        size_t size);

size_t storage_db_chunk_sequence_allowed_max_size();

bool storage_db_chunk_sequence_is_size_allowed(
        size_t size);

bool storage_db_chunk_sequence_allocate(
        storage_db_t *db,
        storage_db_chunk_sequence_t *chunk_sequence,
        size_t size);

storage_db_chunk_info_t *storage_db_chunk_sequence_get(
        storage_db_chunk_sequence_t *chunk_sequence,
        storage_db_chunk_index_t chunk_index);

char *storage_db_get_chunk_data(
        storage_db_t *db,
        storage_db_chunk_info_t *chunk_info,
        bool *allocated_new_buffer);

void storage_db_chunk_sequence_free_chunks(
        storage_db_t *db,
        storage_db_chunk_sequence_t *sequence);

void storage_db_entry_index_status_increase_readers_counter(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *old_status);

void storage_db_entry_index_status_decrease_readers_counter(
        storage_db_entry_index_t* entry_index,
        storage_db_entry_index_status_t *old_status);

void storage_db_entry_index_status_set_deleted(
        storage_db_entry_index_t* entry_index,
        bool deleted,
        storage_db_entry_index_status_t *old_status);

storage_db_entry_index_t *storage_db_get_entry_index(
        storage_db_t *db,
        char *key,
        size_t key_length);

bool storage_db_entry_index_is_expired(
        storage_db_entry_index_t *entry_index);

int64_t storage_db_entry_index_ttl_ms(
        storage_db_entry_index_t *entry_index);

storage_db_entry_index_t *storage_db_get_entry_index_for_read_prep(
        storage_db_t *db,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index);

storage_db_entry_index_t *storage_db_get_entry_index_for_read(
        storage_db_t *db,
        char *key,
        size_t key_length);

bool storage_db_set_entry_index(
        storage_db_t *db,
        char *key,
        size_t key_length,
        storage_db_entry_index_t *entry_index);

bool storage_db_op_set(
        storage_db_t *db,
        char *key,
        size_t key_length,
        storage_db_entry_index_value_type_t value_type,
        storage_db_chunk_sequence_t *value_chunk_sequence,
        storage_db_expiry_time_ms_t expiry_time_ms);

bool storage_db_op_rmw_begin(
        storage_db_t *db,
        transaction_t *transaction,
        char *key,
        size_t key_length,
        storage_db_op_rmw_status_t *rmw_status,
        storage_db_entry_index_t **current_entry_index);

storage_db_entry_index_t *storage_db_op_rmw_current_entry_index_prep_for_read(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status,
        storage_db_entry_index_t *entry_index);

bool storage_db_op_rmw_commit_metadata(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status);

bool storage_db_op_rmw_commit_update(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status,
        storage_db_entry_index_value_type_t value_type,
        storage_db_chunk_sequence_t *value_chunk_sequence,
        storage_db_expiry_time_ms_t expiry_time_ms);

void storage_db_op_rmw_commit_rename(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status_source,
        storage_db_op_rmw_status_t *rmw_status_destination);

void storage_db_op_rmw_commit_delete(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status);

void storage_db_op_rmw_abort(
        storage_db_t *db,
        storage_db_op_rmw_status_t *rmw_status);

bool storage_db_op_delete(
        storage_db_t *db,
        char *key,
        size_t key_length);

bool storage_db_op_delete_by_index(
        storage_db_t *db,
        hashtable_bucket_index_t bucket_index);

char *storage_db_op_random_key(
        storage_db_t *db,
        hashtable_key_size_t *key_size);

int64_t storage_db_op_get_keys_count(
        storage_db_t *db);

int64_t storage_db_op_get_data_size(
        storage_db_t *db);

bool storage_db_op_flush_sync(
        storage_db_t *db);

storage_db_key_and_key_length_t *storage_db_op_get_keys(
        storage_db_t *db,
        uint64_t cursor,
        uint64_t count,
        char *pattern,
        size_t pattern_length,
        uint64_t *keys_count,
        uint64_t *cursor_next);

void storage_db_free_key_and_key_length_list(
        storage_db_key_and_key_length_t *keys,
        uint64_t keys_count);

void storage_db_keys_eviction_bitonic_sort_16_elements(
        storage_db_keys_eviction_kv_list_entry_t *kv);

uint8_t storage_db_keys_eviction_run_worker(
        storage_db_t *db,
        bool only_ttl,
        config_database_keys_eviction_policy_t policy);

static inline bool storage_db_keys_eviction_should_run(
        storage_db_t *db) {
    uint64_t keys_count = storage_db_op_get_keys_count(db);
    uint64_t data_size = storage_db_op_get_data_size(db);

    if (keys_count == 0) {
        return false;
    }

    if (db->limits.keys_count.soft_limit > 0 && keys_count > db->limits.keys_count.soft_limit) {
        return true;
    }

    if (db->limits.data_size.soft_limit > 0 && data_size > db->limits.data_size.soft_limit) {
        return true;
    }

    return false;
}

static inline double storage_db_keys_eviction_calculate_close_to_hard_limit_percentage(
        storage_db_t *db) {
    double keys_count_close_to_hard_limit_percentage = 0;
    double data_size_close_to_hard_limit_percentage = 0;
    uint64_t keys_count = storage_db_op_get_keys_count(db);
    uint64_t data_size = storage_db_op_get_data_size(db);

    if (db->limits.keys_count.soft_limit == 0 && db->limits.data_size.soft_limit == 0) {
        return 0;
    }

    if (db->limits.keys_count.soft_limit > 0 && keys_count > db->limits.keys_count.soft_limit) {
        keys_count_close_to_hard_limit_percentage =
                (double)(keys_count - db->limits.keys_count.soft_limit) /
                (double)(db->limits.keys_count.hard_limit - db->limits.keys_count.soft_limit);
    }

    if (db->limits.data_size.soft_limit > 0 && data_size > db->limits.data_size.soft_limit) {
        data_size_close_to_hard_limit_percentage =
                (double)(data_size - db->limits.data_size.soft_limit) /
                (double)(db->limits.data_size.hard_limit - db->limits.data_size.soft_limit);
    }

    return keys_count_close_to_hard_limit_percentage > data_size_close_to_hard_limit_percentage ?
            keys_count_close_to_hard_limit_percentage : data_size_close_to_hard_limit_percentage;
}

static inline bool storage_db_will_new_entry_hit_hard_limit(
        storage_db_t *db,
        uint64_t new_entry_size) {
    uint64_t keys_count = storage_db_op_get_keys_count(db);
    uint64_t data_size = storage_db_op_get_data_size(db);

    if (db->limits.keys_count.hard_limit > 0 && keys_count + 1 > db->limits.keys_count.hard_limit) {
        return true;
    }

    if (db->limits.data_size.hard_limit > 0 && data_size + new_entry_size > db->limits.data_size.hard_limit) {
        return true;
    }

    return false;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_STORAGE_DB_H
