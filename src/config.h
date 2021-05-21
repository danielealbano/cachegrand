#ifndef CACHEGRAND_CONFIG_H
#define CACHEGRAND_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

enum config_network_protocol_type {
    CONFIG_PROTOCOL_TYPE_REDIS,
};
typedef enum config_network_protocol_type config_network_protocol_type_t;

enum config_log_type {
    CONFIG_LOG_TYPE_CONSOLE,
    CONFIG_LOG_TYPE_FILE,
    CONFIG_LOG_TYPE_MAX,
};
typedef enum config_log_type config_log_type_t;

enum config_log_level {
    CONFIG_LOG_LEVEL_DEBUG = 0x0002,
    CONFIG_LOG_LEVEL_VERBOSE = 0x0004,
    CONFIG_LOG_LEVEL_INFO = 0x0008,
    CONFIG_LOG_LEVEL_WARNING = 0x0010,
    CONFIG_LOG_LEVEL_ERROR = 0x0020,
    CONFIG_LOG_LEVEL_MAX,

    // Extra log levels specific for the config as they are mapped directly into string to be used with cyaml
    CONFIG_LOG_LEVEL_ALL = 0x0080,

    CONFIG_LOG_LEVEL_DEBUG_NEGATE = 0x0200,
    CONFIG_LOG_LEVEL_VERBOSE_NEGATE = 0x0400,
    CONFIG_LOG_LEVEL_INFO_NEGATE = 0x0800,
    CONFIG_LOG_LEVEL_WARNING_NEGATE = 0x1000,
    CONFIG_LOG_LEVEL_ERROR_NEGATE = 0x2000,
};
typedef enum config_log_level config_log_level_t;

enum config_network_backend {
    CONFIG_NETWORK_BACKEND_IO_URING,
};
typedef enum config_network_backend config_network_backend_t;

typedef struct config_network_protocol_binding config_network_protocol_binding_t;
struct config_network_protocol_binding {
    char* host;
    uint16_t port;
};

typedef struct config_network_protocol_timeout config_network_protocol_timeout_t;
struct config_network_protocol_timeout {
    uint32_t read;
    uint32_t write;
    uint32_t inactivity;
};

typedef struct config_network_protocol_keepalive config_network_protocol_keepalive_t;
struct config_network_protocol_keepalive {
    uint32_t time;
    uint32_t interval;
    uint32_t probes;
};

typedef struct config_network_protocol_redis config_network_protocol_redis_t;
struct config_network_protocol_redis {
    uint32_t max_key_length;
    uint32_t max_command_length;
};

typedef struct config_network_protocol config_network_protocol_t;
struct config_network_protocol {
    config_network_protocol_type_t type;
    config_network_protocol_timeout_t* timeout;
    config_network_protocol_keepalive_t* keepalive;

    config_network_protocol_redis_t* redis;

    config_network_protocol_binding_t* bindings;
    unsigned bindings_count;
};

typedef struct config_log_file config_log_file_t;
struct config_log_file {
    char* path;
};

typedef struct config_log config_log_t;
struct config_log {
    config_log_type_t type;
    config_log_level_t level;
    config_log_file_t* file;
};

typedef struct config_sentry config_sentry_t;
struct config_sentry {
    bool enable;
    char* data_path;
    char* dsn;
};

typedef struct config_network config_network_t;
struct config_network {
    config_network_backend_t backend;
    uint32_t max_clients;
    uint32_t listen_backlog;

    config_network_protocol_t* protocols;
    uint8_t protocols_count;
};

enum config_storage_backend {
    CONFIG_STORAGE_BACKEND_MEMORY,
    CONFIG_STORAGE_BACKEND_IO_URING,
};
typedef enum config_storage_backend config_storage_backend_t;

typedef struct config_storage config_storage_t;
struct config_storage {
    config_storage_backend_t backend;
    uint32_t max_shard_size_mb;
    union {
        struct {
            char* path;
        } io_uring;
    };
};

typedef struct config_database config_database_t;
struct config_database {
    uint32_t max_keys;
};

typedef struct config config_t;
struct config {
    char** cpus;
    unsigned cpus_count;
    uint32_t workers_per_cpus;
    bool run_in_foreground;
    char* pidfile_path;
    bool* use_slab_allocator;

    config_network_t* network;
    config_storage_t* storage;
    config_database_t* database;
    config_sentry_t* sentry;

    config_log_t* logs;
    uint8_t logs_count;
};

enum config_cpus_validate_error {
    CONFIG_CPUS_VALIDATE_OK,
    CONFIG_CPUS_VALIDATE_ERROR_INVALID_CPU,
    CONFIG_CPUS_VALIDATE_ERROR_MULTIPLE_RANGES,
    CONFIG_CPUS_VALIDATE_ERROR_RANGE_TOO_SMALL,
    CONFIG_CPUS_VALIDATE_ERROR_UNEXPECTED_CHARACTER,

    CONFIG_CPUS_VALIDATE_MAX,
};
typedef enum config_cpus_validate_error config_cpus_validate_error_t;

config_t* config_load(
        char* config_path);

void config_free(
        config_t* config);

bool config_validate_after_load(
        config_t* config);

bool config_cpus_validate(
        uint16_t max_cpus_count,
        char** cpus,
        unsigned cpus_count,
        config_cpus_validate_error_t* config_cpus_validate_errors);

bool config_cpus_parse(
        uint16_t max_cpus_count,
        char** cpus,
        unsigned cpus_count,
        uint16_t** cpus_map,
        uint16_t* cpus_map_count);

void config_cpus_filter_duplicates(
        uint16_t* cpus,
        uint16_t cpus_count,
        uint16_t** unique_cpus,
        uint16_t* unique_cpus_count);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_CONFIG_H
