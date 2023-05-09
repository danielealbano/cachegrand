/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <cyaml/cyaml.h>

#include "config.h"
#include "config_cyaml_schema.h"

// Generic schema for string pointers
const cyaml_schema_value_t config_generic_string_schema = {
        CYAML_VALUE_STRING(CYAML_FLAG_POINTER, char, 0, CYAML_UNLIMITED),
};

/**
 * CONFIG -> MODULES schemas
 */

// Schema for config -> modules -> module -> network -> bindings -> binding
const cyaml_schema_field_t config_module_binding_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "host", CYAML_FLAG_POINTER,
                config_module_network_binding_t, host, 0, CYAML_UNLIMITED),
        CYAML_FIELD_UINT(
                "port", CYAML_FLAG_DEFAULT,
                config_module_network_binding_t, port),
        CYAML_FIELD_BOOL(
                "tls", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_module_network_binding_t, tls),
        CYAML_FIELD_END
};

// Schema for config -> modules -> module -> network -> timeout
const cyaml_schema_field_t config_module_network_timeout_schema[] = {
        CYAML_FIELD_INT(
                "read_ms", CYAML_FLAG_DEFAULT,
                config_module_network_timeout_t, read_ms),
        CYAML_FIELD_INT(
                "write_ms", CYAML_FLAG_DEFAULT,
                config_module_network_timeout_t, write_ms),
        CYAML_FIELD_END
};

// Schema for config -> modules -> module -> network -> keepalive
const cyaml_schema_field_t config_module_network_keepalive_schema[] = {
        CYAML_FIELD_UINT(
                "time", CYAML_FLAG_DEFAULT,
                config_module_network_keepalive_t, time),
        CYAML_FIELD_UINT(
                "interval", CYAML_FLAG_DEFAULT,
                config_module_network_keepalive_t, interval),
        CYAML_FIELD_UINT(
                "probes", CYAML_FLAG_DEFAULT,
                config_module_network_keepalive_t, probes),
        CYAML_FIELD_END
};

// Schema for config -> modules -> module -> network -> tls
const cyaml_schema_field_t config_module_network_tls_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "certificate_path", CYAML_FLAG_DEFAULT,
                config_module_network_tls_t, certificate_path, 0, CYAML_UNLIMITED),
        CYAML_FIELD_STRING_PTR(
                "private_key_path", CYAML_FLAG_DEFAULT,
                config_module_network_tls_t, private_key_path, 0, CYAML_UNLIMITED),
        CYAML_FIELD_STRING_PTR(
                "ca_certificate_chain_path", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_module_network_tls_t, ca_certificate_chain_path, 0, CYAML_UNLIMITED),
        CYAML_FIELD_BOOL(
                "verify_client_certificate", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_module_network_tls_t, verify_client_certificate),
        CYAML_FIELD_SEQUENCE(
                "cipher_suites", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_module_network_tls_t, cipher_suites,
                &config_generic_string_schema, 0, CYAML_UNLIMITED),
        CYAML_FIELD_ENUM(
                "min_version", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT | CYAML_FLAG_OPTIONAL,
                config_module_network_tls_t, min_version, config_module_network_tls_min_version_schema_strings,
                CYAML_ARRAY_LEN(config_module_network_tls_min_version_schema_strings)),
        CYAML_FIELD_ENUM(
                "max_version", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT | CYAML_FLAG_OPTIONAL,
                config_module_network_tls_t, max_version, config_module_network_tls_max_version_schema_strings,
                CYAML_ARRAY_LEN(config_module_network_tls_max_version_schema_strings)),
        CYAML_FIELD_END
};

// Schema for config -> modules -> module -> redis
const cyaml_schema_field_t config_module_redis_schema[] = {
        CYAML_FIELD_UINT(
                "max_key_length", CYAML_FLAG_DEFAULT,
                config_module_redis_t, max_key_length),
        CYAML_FIELD_UINT(
                "max_command_length", CYAML_FLAG_DEFAULT,
                config_module_redis_t, max_command_length),
        CYAML_FIELD_UINT(
                "max_command_arguments", CYAML_FLAG_DEFAULT,
                config_module_redis_t, max_command_arguments),
        CYAML_FIELD_BOOL(
                "strict_parsing", CYAML_FLAG_DEFAULT,
                config_module_redis_t, strict_parsing),
        CYAML_FIELD_BOOL(
                "require_authentication", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_module_redis_t, require_authentication),
        CYAML_FIELD_STRING_PTR(
                "username", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_module_redis_t, username, 0, CYAML_UNLIMITED),
        CYAML_FIELD_STRING_PTR(
                "password", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_module_redis_t, password, 0, CYAML_UNLIMITED),
        CYAML_FIELD_END
};

// Schema for config -> modules -> module -> bindings
const cyaml_schema_value_t config_module_network_protocol_binding_list_schema = {
        CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT,
                            config_module_network_binding_t, config_module_binding_schema),
};

// Schema for config -> modules -> module -> network
const cyaml_schema_field_t config_module_network_fields_schema[] = {
        CYAML_FIELD_MAPPING_PTR(
                "timeout", CYAML_FLAG_POINTER,
                config_module_network_t, timeout, config_module_network_timeout_schema),
        CYAML_FIELD_MAPPING_PTR(
                "keepalive", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_module_network_t, keepalive, config_module_network_keepalive_schema),
        CYAML_FIELD_MAPPING_PTR(
                "tls", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_module_network_t, tls, config_module_network_tls_schema),
        CYAML_FIELD_SEQUENCE(
                "bindings", CYAML_FLAG_POINTER,
                config_module_network_t, bindings, &config_module_network_protocol_binding_list_schema, 0, CYAML_UNLIMITED),
        CYAML_FIELD_END
};

// Schema for config -> modules -> module -> protocol
const cyaml_schema_field_t config_module_fields_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "type", CYAML_FLAG_DEFAULT,
                config_module_t, type, 0, 20),
        CYAML_FIELD_MAPPING_PTR(
                "network", CYAML_FLAG_POINTER,
                config_module_t, network, config_module_network_fields_schema),
        CYAML_FIELD_MAPPING_PTR(
                "redis", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_module_t, redis, config_module_redis_schema),
        CYAML_FIELD_END
};

// Schema for config -> modules
const cyaml_schema_value_t config_module_list_schema = {
        CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT,
                            config_module_t, config_module_fields_schema),
};

/**
 * CONFIG -> NETWORK schemas
 */

// Allowed strings for config -> network -> backend
const cyaml_strval_t config_network_backend_schema_strings[] = {
        { "io_uring", CONFIG_NETWORK_BACKEND_IO_URING },
};

// Schema for config -> network
const cyaml_schema_field_t config_network_schema[] = {
        CYAML_FIELD_ENUM(
                "backend", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT,
                config_network_t, backend, config_network_backend_schema_strings,
                CYAML_ARRAY_LEN(config_network_backend_schema_strings)),
        CYAML_FIELD_UINT(
                "max_clients", CYAML_FLAG_POINTER,
                config_network_t, max_clients),
        CYAML_FIELD_UINT(
                "listen_backlog", CYAML_FLAG_POINTER,
                config_network_t, listen_backlog),
        CYAML_FIELD_END
};

/**
 * CONFIG -> database schema
 */

// Schema for config -> database -> keys_eviction
const cyaml_schema_field_t config_database_keys_eviction_schema[] = {
        CYAML_FIELD_BOOL(
                "only_ttl", CYAML_FLAG_DEFAULT,
                config_database_keys_eviction_t, only_ttl),
        CYAML_FIELD_ENUM(
                "policy", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT,
                config_database_keys_eviction_t , policy, config_database_keys_eviction_policy_schema_strings,
                CYAML_ARRAY_LEN(config_database_keys_eviction_policy_schema_strings)),
        CYAML_FIELD_END
};

// Schema for config -> database -> file -> limits -> hard
const cyaml_schema_field_t config_database_file_limits_hard_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "max_disk_usage", CYAML_FLAG_DEFAULT,
                config_database_file_limits_hard_t, max_disk_usage_str, 0, 20),
        CYAML_FIELD_END
};

// Schema for config -> database -> file -> limits -> soft
const cyaml_schema_field_t config_database_file_limits_soft_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "max_disk_usage", CYAML_FLAG_DEFAULT,
                config_database_file_limits_soft_t, max_disk_usage_str, 0, 20),
        CYAML_FIELD_END
};

// Schema for config -> database -> limits
const cyaml_schema_field_t config_database_file_limits_schema[] = {
        CYAML_FIELD_MAPPING_PTR(
                "hard", CYAML_FLAG_POINTER,
                config_database_file_limits_t, hard, config_database_file_limits_hard_schema),
        CYAML_FIELD_MAPPING_PTR(
                "soft", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_file_limits_t, soft, config_database_file_limits_soft_schema),
        CYAML_FIELD_END
};

// Schema for config -> database -> keys_eviction
const cyaml_schema_field_t config_database_file_garbage_collector_schema[] = {
        CYAML_FIELD_UINT(
                "min_interval_s", CYAML_FLAG_DEFAULT,
                config_database_file_garbage_collector_t, min_interval_s),
        CYAML_FIELD_END
};

// Schema for config -> database -> file (with config.database.backend == file)
const cyaml_schema_field_t config_database_backend_file_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "path", CYAML_FLAG_POINTER,
                config_database_file_t, path, 0, CYAML_UNLIMITED),
        CYAML_FIELD_UINT(
                "max_opened_shards", CYAML_FLAG_POINTER,
                config_database_file_t, max_opened_shards),
        CYAML_FIELD_UINT(
                "shard_size_mb", CYAML_FLAG_POINTER,
                config_database_file_t, shard_size_mb),
        CYAML_FIELD_MAPPING_PTR(
                "garbage_collector", CYAML_FLAG_POINTER,
                config_database_file_t, garbage_collector, config_database_file_garbage_collector_schema),
        CYAML_FIELD_MAPPING_PTR(
                "limits", CYAML_FLAG_POINTER,
                config_database_file_t, limits, config_database_file_limits_schema),
        CYAML_FIELD_END
};

// Schema for config -> database -> memory -> limits -> hard
const cyaml_schema_field_t config_database_memory_limits_hard_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "max_memory_usage", CYAML_FLAG_DEFAULT,
                config_database_memory_limits_hard_t, max_memory_usage_str, 0, 20),
        CYAML_FIELD_END
};

// Schema for config -> database -> memory -> limits -> soft
const cyaml_schema_field_t config_database_memory_limits_soft_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "max_memory_usage", CYAML_FLAG_DEFAULT,
                config_database_memory_limits_soft_t, max_memory_usage_str, 0, 20),
        CYAML_FIELD_END
};

// Schema for config -> database -> limits
const cyaml_schema_field_t config_database_memory_limits_schema[] = {
        CYAML_FIELD_MAPPING_PTR(
                "hard", CYAML_FLAG_POINTER,
                config_database_memory_limits_t, hard, config_database_memory_limits_hard_schema),
        CYAML_FIELD_MAPPING_PTR(
                "soft", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_memory_limits_t, soft, config_database_memory_limits_soft_schema),
        CYAML_FIELD_END
};

// Schema for config -> database -> memory (with config.database.backend == memory)
const cyaml_schema_field_t config_database_backend_memory_schema[] = {
        CYAML_FIELD_MAPPING_PTR(
                "limits", CYAML_FLAG_POINTER,
                config_database_memory_t, limits, config_database_memory_limits_schema),
        CYAML_FIELD_END
};

// Schema for config -> database -> limits -> hard
const cyaml_schema_field_t config_database_limits_hard_schema[] = {
        CYAML_FIELD_UINT(
                "max_keys", CYAML_FLAG_DEFAULT,
                config_database_limits_hard_t, max_keys),
        CYAML_FIELD_END
};

// Schema for config -> database -> limits -> soft
const cyaml_schema_field_t config_database_limits_soft_schema[] = {
        CYAML_FIELD_UINT(
                "max_keys", CYAML_FLAG_DEFAULT,
                config_database_limits_soft_t, max_keys),
        CYAML_FIELD_END
};

// Schema for config -> database -> limits
const cyaml_schema_field_t config_database_limits_schema[] = {
        CYAML_FIELD_MAPPING_PTR(
                "hard", CYAML_FLAG_POINTER,
                config_database_limits_t, hard, config_database_limits_hard_schema),
        CYAML_FIELD_MAPPING_PTR(
                "soft", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_limits_t, soft, config_database_limits_soft_schema),
        CYAML_FIELD_END
};

// Schema for config -> database -> snapshots -> rotation
const cyaml_schema_field_t config_database_snapshots_rotation_schema[] = {
        CYAML_FIELD_UINT(
                "max_files", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_database_snapshots_rotation_t, max_files),
        CYAML_FIELD_END
};

// Schema for config -> database -> snapshots
const cyaml_schema_field_t config_database_snapshots_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "path", CYAML_FLAG_DEFAULT,
                config_database_snapshots_t, path, 0, CYAML_UNLIMITED),
        CYAML_FIELD_STRING_PTR(
                "interval", CYAML_FLAG_DEFAULT,
                config_database_snapshots_t, interval_str, 0, 20),
        CYAML_FIELD_BOOL(
                "snapshot_at_shutdown", CYAML_FLAG_DEFAULT,
                config_database_snapshots_t, snapshot_at_shutdown),
        CYAML_FIELD_UINT(
                "min_keys_changed", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_database_snapshots_t, min_keys_changed),
        CYAML_FIELD_STRING_PTR(
                "min_data_changed", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_database_snapshots_t, min_data_changed_str, 0, 20),
        CYAML_FIELD_MAPPING_PTR(
                "rotation", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_snapshots_t, rotation, config_database_snapshots_rotation_schema),
        CYAML_FIELD_END
};

// Schema for config -> database
const cyaml_schema_field_t config_database_schema[] = {
        CYAML_FIELD_MAPPING_PTR(
                "limits", CYAML_FLAG_POINTER,
                config_database_t, limits, config_database_limits_schema),
        CYAML_FIELD_MAPPING_PTR(
                "snapshots", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_t, snapshots, config_database_snapshots_schema),
        CYAML_FIELD_MAPPING_PTR(
                "keys_eviction", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_t, keys_eviction, config_database_keys_eviction_schema),
        CYAML_FIELD_ENUM(
                "backend", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT,
                config_database_t, backend, config_database_backend_schema_strings,
                CYAML_ARRAY_LEN(config_database_backend_schema_strings)),
        CYAML_FIELD_MAPPING_PTR(
                "file", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_t, file, config_database_backend_file_schema),
        CYAML_FIELD_MAPPING_PTR(
                "memory", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_t, memory, config_database_backend_memory_schema),
        CYAML_FIELD_UINT(
                "max_user_databases", CYAML_FLAG_DEFAULT,
                config_database_t, max_user_databases),
        CYAML_FIELD_END
};

/**
 * CONFIG -> LOGS schema
 */

// Schema for config -> logs -> log -> file
const cyaml_schema_field_t config_log_file_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "path", CYAML_FLAG_POINTER,
                config_log_file_t, path, 0, CYAML_UNLIMITED),
        CYAML_FIELD_END
};

// Schema for config -> logs -> log
const cyaml_schema_field_t config_log_fields_schema[] = {
        CYAML_FIELD_ENUM(
                "type", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT,
                config_log_t, type, config_log_type_schema_strings,
                CYAML_ARRAY_LEN(config_log_type_schema_strings)),
        CYAML_FIELD_FLAGS(
                "level", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT,
                config_log_t, level, config_log_level_schema_strings,
                CYAML_ARRAY_LEN(config_log_level_schema_strings)),
        CYAML_FIELD_MAPPING_PTR(
                "file", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_log_t, file, config_log_file_schema),
        CYAML_FIELD_END
};

// Schema for config -> logs
const cyaml_schema_value_t config_log_list_schema = {
        CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT,
                            config_log_t, config_log_fields_schema),
};

/**
 * CONFIG SENTRY schema
 */

// Schema for config -> sentry
const cyaml_schema_field_t config_sentry_schema[] = {
        CYAML_FIELD_BOOL(
                "enable", CYAML_FLAG_DEFAULT,
                config_sentry_t, enable),
        CYAML_FIELD_STRING_PTR(
                "data_path", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_sentry_t, data_path, 0, CYAML_UNLIMITED),
        CYAML_FIELD_END
};

/**
 * CONFIG schema
 */

// Schema for config
const cyaml_schema_field_t config_fields_schema[] = {
        CYAML_FIELD_SEQUENCE(
                "cpus", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_t, cpus,
                &config_generic_string_schema, 0, CYAML_UNLIMITED),
        CYAML_FIELD_UINT(
                "workers_per_cpus", CYAML_FLAG_DEFAULT,
                config_t, workers_per_cpus),

        CYAML_FIELD_BOOL(
                "run_in_foreground", CYAML_FLAG_DEFAULT,
                config_t, run_in_foreground),
        CYAML_FIELD_STRING_PTR(
                "pidfile_path", CYAML_FLAG_POINTER,
                config_t, pidfile_path, 0, CYAML_UNLIMITED | CYAML_FLAG_OPTIONAL),
        CYAML_FIELD_BOOL_PTR(
                "use_hugepages", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_t, use_hugepages),

        CYAML_FIELD_MAPPING_PTR(
                "network", CYAML_FLAG_POINTER,
                config_t, network, config_network_schema),
        CYAML_FIELD_SEQUENCE(
                "modules", CYAML_FLAG_POINTER,
                config_t, modules,
                &config_module_list_schema, 1, CYAML_UNLIMITED),
        CYAML_FIELD_MAPPING_PTR(
                "database", CYAML_FLAG_POINTER,
                config_t, database, config_database_schema),
        CYAML_FIELD_MAPPING_PTR(
                "sentry", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_t, sentry, config_sentry_schema),

        CYAML_FIELD_SEQUENCE(
                "logs", CYAML_FLAG_POINTER,
                config_t, logs,
                &config_log_list_schema, 1, CYAML_UNLIMITED),

        CYAML_FIELD_END
};

const cyaml_schema_value_t config_top_schema = {
        CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER,
                            config_t, config_fields_schema),
};

const cyaml_schema_value_t* config_cyaml_schema_get_top_schema() {
    return &config_top_schema;
}
