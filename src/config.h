#ifndef CACHEGRAND_CONFIG_H
#define CACHEGRAND_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

enum config_protocol_type {
    CONFIG_PROTOCOL_TYPE_REDIS,
};
typedef enum config_protocol_type config_protocol_type_t;

enum config_log_type {
    CONFIG_LOG_TYPE_CONSOLE,
    CONFIG_LOG_TYPE_FILE,
};
typedef enum config_log_type config_log_type_t;

enum config_log_level {
#if DEBUG==1
    CONFIG_LOG_LEVEL_DEBUG = 0x0002,
#endif
    CONFIG_LOG_LEVEL_VERBOSE = 0x0004,
    CONFIG_LOG_LEVEL_INFO = 0x0008,
    CONFIG_LOG_LEVEL_WARNING = 0x0010,
    CONFIG_LOG_LEVEL_RECOVERABLE = 0x0020,
    CONFIG_LOG_LEVEL_ERROR = 0x0040,
    CONFIG_LOG_LEVEL_ALL = 0x0080,

#if DEBUG==1
    CONFIG_LOG_LEVEL_DEBUG_NEGATE = 0x0200,
#endif
    CONFIG_LOG_LEVEL_VERBOSE_NEGATE = 0x0400,
    CONFIG_LOG_LEVEL_INFO_NEGATE = 0x0800,
    CONFIG_LOG_LEVEL_WARNING_NEGATE = 0x1000,
    CONFIG_LOG_LEVEL_RECOVERABLE_NEGATE = 0x2000,
    CONFIG_LOG_LEVEL_ERROR_NEGATE = 0x4000,
};
typedef enum config_log_level config_log_level_t;

enum config_worker_type {
    CONFIG_WORKER_TYPE_IO_URING,
};
typedef enum config_worker_type config_worker_type_t;

typedef struct config_protocol_binding config_protocol_binding_t;
struct config_protocol_binding {
    char* host;
    char* port;
};

typedef struct config_protocol_timeout config_protocol_timeout_t;
struct config_protocol_timeout {
    uint32_t connection;
    uint32_t read;
    uint32_t write;
    uint32_t inactivity;
};

typedef struct config_protocol_keepalive config_protocol_keepalive_t;
struct config_protocol_keepalive {
    uint32_t time;
    uint32_t interval;
    uint32_t probes;
};

typedef struct config_protocol_redis config_protocol_redis_t;
struct config_protocol_redis {
    uint32_t max_key_length;
    uint32_t max_command_length;
};

typedef struct config_protocol config_protocol_t;
struct config_protocol {
    config_protocol_type_t type;
    config_protocol_timeout_t* timeout;
    config_protocol_keepalive_t* keepalive;

    config_protocol_redis_t* redis;

    config_protocol_binding_t* bindings;
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

// Do not remove the macro below as they are use to "tag" the struct in a special so that the build process can identify
// the settings and produce a dynamic loader for the config.
typedef struct config config_t;
struct config {
    config_worker_type_t worker_type;
    char** cpus;
    unsigned cpus_count;
    bool run_in_foreground;
    char* pidfile_path;
    bool* use_slab_allocator;

    uint32_t network_max_clients;
    uint32_t network_listen_backlog;
    uint32_t storage_max_partition_size_mb;
    uint32_t memory_max_keys;

    config_protocol_t* protocols;
    uint8_t protocols_count;

    config_log_t* logs;
    uint8_t logs_count;
};

config_t* config_load(
        char* config_path);

void config_free(
        config_t* config);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_CONFIG_H
