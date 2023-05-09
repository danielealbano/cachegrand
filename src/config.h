#ifndef CACHEGRAND_CONFIG_H
#define CACHEGRAND_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t module_id_t;

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

enum config_module_network_tls_min_version {
    CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY,
    CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_0,
    CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_1,
    CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2,
    CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3,
};
typedef enum config_module_network_tls_min_version config_module_network_tls_min_version_t;

enum config_module_network_tls_max_version {
    CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY,
    CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_0,
    CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_1,
    CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2,
    CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_3,
};
typedef enum config_module_network_tls_max_version config_module_network_tls_max_version_t;

enum config_parse_string_absolute_or_percent_return_value {
    CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE,
    CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_PERCENT,
};
typedef enum config_parse_string_absolute_or_percent_return_value
        config_parse_string_absolute_or_percent_return_value_t;

typedef struct config_module_network_binding config_module_network_binding_t;
struct config_module_network_binding {
    char *host;
    uint16_t port;
    bool tls;
};

typedef struct config_module_network_timeout config_module_network_timeout_t;
struct config_module_network_timeout {
    int32_t read_ms;
    int32_t write_ms;
};

typedef struct config_module_network_keepalive config_module_network_keepalive_t;
struct config_module_network_keepalive {
    uint32_t time;
    uint32_t interval;
    uint32_t probes;
};

typedef struct config_module_network_tls config_module_network_tls_t;
struct config_module_network_tls {
    char *certificate_path;
    char *private_key_path;
    char *ca_certificate_chain_path;
    char **cipher_suites;
    unsigned cipher_suites_count;
    config_module_network_tls_min_version_t min_version;
    config_module_network_tls_max_version_t max_version;
    bool verify_client_certificate;
};

typedef struct config_module_redis config_module_redis_t;
struct config_module_redis {
    uint32_t max_key_length;
    uint32_t max_command_length;
    uint32_t max_command_arguments;
    bool strict_parsing;
    bool require_authentication;
    char *username;
    char *password;
};

typedef struct config_module_network config_module_network_t;
struct config_module_network {
    config_module_network_timeout_t *timeout;
    config_module_network_keepalive_t *keepalive;
    config_module_network_tls_t *tls;

    config_module_network_binding_t *bindings;
    unsigned bindings_count;
};

typedef struct config_module config_module_t;
struct config_module {
    char *type;
    module_id_t module_id;

    config_module_network_t *network;
    config_module_redis_t *redis;
};

typedef struct config_log_file config_log_file_t;
struct config_log_file {
    char *path;
};

typedef struct config_log config_log_t;
struct config_log {
    config_log_type_t type;
    config_log_level_t level;
    config_log_file_t *file;
};

typedef struct config_sentry config_sentry_t;
struct config_sentry {
    bool enable;
    char *data_path;
    char *dsn;
};

typedef struct config_network config_network_t;
struct config_network {
    config_network_backend_t backend;
    uint32_t max_clients;
    uint32_t listen_backlog;
};
enum config_database_keys_eviction_policy {
    CONFIG_DATABASE_KEYS_EVICTION_POLICY_LRU,
    CONFIG_DATABASE_KEYS_EVICTION_POLICY_LFU,
    CONFIG_DATABASE_KEYS_EVICTION_POLICY_RANDOM,
    CONFIG_DATABASE_KEYS_EVICTION_POLICY_TTL
};
typedef enum config_database_keys_eviction_policy config_database_keys_eviction_policy_t;

enum config_database_backend {
    CONFIG_DATABASE_BACKEND_MEMORY,
    CONFIG_DATABASE_BACKEND_FILE
};
typedef enum config_database_backend config_database_backend_t;

struct config_database_file_limits_hard {
    char *max_disk_usage_str;
    int64_t max_disk_usage;
};
typedef struct config_database_file_limits_hard config_database_file_limits_hard_t;

struct config_database_file_limits_soft {
    char *max_disk_usage_str;
    int64_t max_disk_usage;
};
typedef struct config_database_file_limits_soft config_database_file_limits_soft_t;

struct config_database_file_limits {
    config_database_file_limits_hard_t *hard;
    config_database_file_limits_soft_t *soft;
};
typedef struct config_database_file_limits config_database_file_limits_t;

struct config_database_file_garbage_collector {
    uint32_t min_interval_s;
};
typedef struct config_database_file_garbage_collector config_database_file_garbage_collector_t;

struct config_database_file {
    char *path;
    uint32_t max_opened_shards;
    uint32_t shard_size_mb;
    config_database_file_garbage_collector_t *garbage_collector;
    config_database_file_limits_t *limits;
};
typedef struct config_database_file config_database_file_t;

struct config_database_memory_limits_hard {
    char *max_memory_usage_str;
    int64_t max_memory_usage;
};
typedef struct config_database_memory_limits_hard config_database_memory_limits_hard_t;

struct config_database_memory_limits_soft {
    char *max_memory_usage_str;
    int64_t max_memory_usage;
};
typedef struct config_database_memory_limits_soft config_database_memory_limits_soft_t;

struct config_database_memory_limits {
    config_database_memory_limits_hard_t *hard;
    config_database_memory_limits_soft_t *soft;
};
typedef struct config_database_memory_limits config_database_memory_limits_t;

struct config_database_memory {
    config_database_memory_limits_t *limits;
};
typedef struct config_database_memory config_database_memory_t;

typedef struct config_database_keys_eviction config_database_keys_eviction_t;
struct config_database_keys_eviction {
    bool only_ttl;
    config_database_keys_eviction_policy_t policy;
};

struct config_database_limits_hard {
    uint64_t max_keys;
};
typedef struct config_database_limits_hard config_database_limits_hard_t;

struct config_database_limits_soft {
    uint64_t max_keys;
};
typedef struct config_database_limits_soft config_database_limits_soft_t;

struct config_database_limits {
    config_database_limits_hard_t *hard;
    config_database_limits_soft_t *soft;
};
typedef struct config_database_limits config_database_limits_t;

struct config_database_snapshots_rotation {
    int64_t max_files;
};
typedef struct config_database_snapshots_rotation config_database_snapshots_rotation_t;

struct config_database_snapshots {
    char *path;
    char *interval_str;
    char *min_data_changed_str;
    bool snapshot_at_shutdown;
    int64_t interval_ms;
    int64_t min_keys_changed;
    int64_t min_data_changed;
    config_database_snapshots_rotation_t *rotation;
};
typedef struct config_database_snapshots config_database_snapshots_t;

struct config_database {
    config_database_limits_t *limits;
    config_database_keys_eviction_t *keys_eviction;
    config_database_backend_t backend;
    config_database_snapshots_t *snapshots;
    config_database_file_t *file;
    config_database_memory_t *memory;
    int64_t max_user_databases;
};
typedef struct config_database config_database_t;

struct config {
    char **cpus;
    unsigned cpus_count;
    uint32_t workers_per_cpus;
    bool run_in_foreground;
    char *pidfile_path;
    bool *use_hugepages;

    config_network_t *network;
    config_module_t *modules;
    uint8_t modules_count;
    config_database_t *database;
    config_sentry_t *sentry;

    config_log_t *logs;
    uint8_t logs_count;
};
typedef struct config config_t;

enum config_cpus_validate_error {
    CONFIG_CPUS_VALIDATE_OK,
    CONFIG_CPUS_VALIDATE_ERROR_INVALID_CPU,
    CONFIG_CPUS_VALIDATE_ERROR_MULTIPLE_RANGES,
    CONFIG_CPUS_VALIDATE_ERROR_RANGE_TOO_SMALL,
    CONFIG_CPUS_VALIDATE_ERROR_UNEXPECTED_CHARACTER,
    CONFIG_CPUS_VALIDATE_ERROR_NO_MULTI_CPUS_WITH_ALL,
};
typedef enum config_cpus_validate_error config_cpus_validate_error_t;

bool config_parse_string_time(
        char *string,
        size_t string_len,
        bool allow_negative,
        bool allow_zero,
        bool allow_time_suffix,
        int64_t *return_value);

bool config_parse_string_absolute_or_percent(
        char *string,
        size_t string_len,
        bool allow_negative,
        bool allow_zero,
        bool allow_percent,
        bool allow_absolute,
        bool allow_size_suffix,
        int64_t *return_value,
        config_parse_string_absolute_or_percent_return_value_t *return_value_type);

config_t *config_load(
        char *config_path);

void config_free(
        config_t *config);

bool config_process_string_values(
        config_t *config);

bool config_validate_after_load_cpus(
        config_t* config);

bool config_validate_after_load_database_snapshots(
        config_t* config);

bool config_validate_after_load_database_backend_file(
        config_t* config);

bool config_validate_after_load_database_backend_memory(
        config_t* config);

bool config_validate_after_load_database_keys_eviction(
        config_t* config);

bool config_validate_after_load_modules_network_timeout(
        config_module_t *module);

bool config_validate_after_load_modules_network_bindings(
        config_module_t *module);

bool config_validate_after_load_modules_network_tls(
        config_module_t *module);

bool config_validate_after_load_modules(
        config_t* config);

bool config_validate_after_load_log_file(
        config_log_t *log);

bool config_validate_after_load_logs(
        config_t* config);

bool config_validate_after_load(
        config_t *config);

bool config_cpus_validate(
        uint16_t max_cpus_count,
        char **cpus,
        unsigned cpus_count,
        config_cpus_validate_error_t *config_cpus_validate_errors);

bool config_cpus_parse(
        uint16_t max_cpus_count,
        char **cpus,
        unsigned cpus_count,
        uint16_t **cpus_map,
        uint16_t *cpus_map_count);

void config_cpus_filter_duplicates(
        const uint16_t *cpus,
        uint16_t cpus_count,
        uint16_t **unique_cpus,
        uint16_t *unique_cpus_count);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_CONFIG_H
