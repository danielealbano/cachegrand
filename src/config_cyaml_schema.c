/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <cyaml/cyaml.h>

#include "config.h"

// Generic schema for string pointers
const cyaml_schema_value_t config_generic_string_schema = {
        CYAML_VALUE_STRING(CYAML_FLAG_POINTER, char, 0, CYAML_UNLIMITED),
};

/**
 * CONFIG -> NETWORK -> PROTOCOLS schemas
 */

// Schema for config -> network -> protocols -> protocol-> bindings -> binding
const cyaml_schema_field_t config_network_protocol_binding_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "host", CYAML_FLAG_POINTER,
                config_network_protocol_binding_t, host, 0, CYAML_UNLIMITED),
        CYAML_FIELD_UINT(
                "port", CYAML_FLAG_DEFAULT,
                config_network_protocol_binding_t, port),
        CYAML_FIELD_END
};

// Schema for config -> network -> protocols -> protocol-> timeout
const cyaml_schema_field_t config_network_protocol_timeout_schema[] = {
        CYAML_FIELD_INT(
                "read_ms", CYAML_FLAG_DEFAULT,
                config_network_protocol_timeout_t, read_ms),
        CYAML_FIELD_INT(
                "write_ms", CYAML_FLAG_DEFAULT,
                config_network_protocol_timeout_t, write_ms),
        CYAML_FIELD_END
};

// Schema for config -> network -> protocols -> protocol-> keepalive
const cyaml_schema_field_t config_network_protocol_keepalive_schema[] = {
        CYAML_FIELD_UINT(
                "time", CYAML_FLAG_DEFAULT,
                config_network_protocol_keepalive_t, time),
        CYAML_FIELD_UINT(
                "interval", CYAML_FLAG_DEFAULT,
                config_network_protocol_keepalive_t, interval),
        CYAML_FIELD_UINT(
                "probes", CYAML_FLAG_DEFAULT,
                config_network_protocol_keepalive_t, probes),
        CYAML_FIELD_END
};

// Schema for config -> network -> protocols -> protocol-> redis
const cyaml_schema_field_t config_network_protocol_redis_schema[] = {
        CYAML_FIELD_UINT(
                "max_key_length", CYAML_FLAG_DEFAULT,
                config_network_protocol_redis_t, max_key_length),
        CYAML_FIELD_UINT(
                "max_command_length", CYAML_FLAG_DEFAULT,
                config_network_protocol_redis_t, max_command_length),
        CYAML_FIELD_END
};

// Allowed strings for for config -> network -> protocols -> protocol-> type (config_network_protocol_type_t)
const cyaml_strval_t config_network_protocol_type_schema_strings[] = {
        { "redis", CONFIG_PROTOCOL_TYPE_REDIS },
};

typedef enum config_network_protocol_type config_network_protocol_type_t;

// Schema for config -> network -> protocols -> protocol-> bindings
const cyaml_schema_value_t config_protocol_binding_list_schema = {
        CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT,
                            config_network_protocol_binding_t, config_network_protocol_binding_schema),
};

// Schema for config -> network -> protocols -> protocol
const cyaml_schema_field_t config_network_protocol_fields_schema[] = {
        CYAML_FIELD_ENUM(
                "type", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT,
                config_network_protocol_t, type, config_network_protocol_type_schema_strings,
                CYAML_ARRAY_LEN(config_network_protocol_type_schema_strings)),
        CYAML_FIELD_MAPPING_PTR(
                "timeout", CYAML_FLAG_POINTER,
                config_network_protocol_t, timeout, config_network_protocol_timeout_schema),
        CYAML_FIELD_MAPPING_PTR(
                "keepalive", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_network_protocol_t, keepalive, config_network_protocol_keepalive_schema),
        CYAML_FIELD_MAPPING_PTR(
                "redis", CYAML_FLAG_POINTER,
                config_network_protocol_t, redis, config_network_protocol_redis_schema),
        CYAML_FIELD_SEQUENCE(
                "bindings", CYAML_FLAG_POINTER,
                config_network_protocol_t, bindings, &config_protocol_binding_list_schema, 0, CYAML_UNLIMITED),
        CYAML_FIELD_END
};

// Schema for config -> network -> protocols
const cyaml_schema_value_t config_network_protocol_list_schema = {
        CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT,
                            config_network_protocol_t, config_network_protocol_fields_schema),
};

// Allowed strings for for config -> network -> backend
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
        CYAML_FIELD_SEQUENCE(
                "protocols", CYAML_FLAG_POINTER,
                config_network_t, protocols,
                &config_network_protocol_list_schema, 1, CYAML_UNLIMITED),
        CYAML_FIELD_END
};

/**
 * CONFIG -> database schema
 */

// Allowed strings for for config -> database -> backend
const cyaml_strval_t config_database_backend_schema_strings[] = {
        { "memory", CONFIG_DATABASE_BACKEND_MEMORY },
        { "file", CONFIG_DATABASE_BACKEND_FILE }
};

// Schema for config -> database -> file (with config.database.backend == file)
const cyaml_schema_field_t config_storage_file_schema[] = {
        CYAML_FIELD_STRING_PTR(
                "path", CYAML_FLAG_POINTER,
                config_database_file_t, path, 0, CYAML_UNLIMITED),
        CYAML_FIELD_UINT(
                "max_opened_shards", CYAML_FLAG_POINTER,
                config_database_file_t, max_opened_shards),
        CYAML_FIELD_UINT(
                "shard_size_mb", CYAML_FLAG_POINTER,
                config_database_file_t, shard_size_mb),
        CYAML_FIELD_END
};
// Schema for config -> storage
const cyaml_schema_field_t config_database_schema[] = {
        CYAML_FIELD_UINT(
                "max_keys", CYAML_FLAG_POINTER,
                config_database_t, max_keys),
        CYAML_FIELD_ENUM(
                "backend", CYAML_FLAG_DEFAULT | CYAML_FLAG_STRICT,
                config_database_t, backend, config_database_backend_schema_strings,
                CYAML_ARRAY_LEN(config_database_backend_schema_strings)),
        CYAML_FIELD_MAPPING_PTR(
                "file", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_database_t, file, config_storage_file_schema),
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

// Allowed strings for for config -> logs -> log -> type (config_log_type_t)
const cyaml_strval_t config_log_type_schema_strings[] = {
        { "console", CONFIG_LOG_TYPE_CONSOLE },
        { "file",    CONFIG_LOG_TYPE_FILE },
};

// Allowed strings for for config -> logs -> log -> level (config_log_level_t)
const cyaml_strval_t config_log_level_schema_strings[] = {
        { "error", CONFIG_LOG_LEVEL_ERROR },
        { "warning", CONFIG_LOG_LEVEL_WARNING },
        { "info", CONFIG_LOG_LEVEL_INFO },
        { "verbose", CONFIG_LOG_LEVEL_VERBOSE },
        { "debug", CONFIG_LOG_LEVEL_DEBUG },
        { "all", CONFIG_LOG_LEVEL_ALL },
        { "no-error", CONFIG_LOG_LEVEL_ERROR_NEGATE },
        { "no-warning", CONFIG_LOG_LEVEL_WARNING_NEGATE },
        { "no-info", CONFIG_LOG_LEVEL_INFO_NEGATE },
        { "no-verbose", CONFIG_LOG_LEVEL_VERBOSE_NEGATE },
        { "no-debug", CONFIG_LOG_LEVEL_DEBUG_NEGATE },
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
        CYAML_FIELD_STRING_PTR(
                "dsn", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
                config_sentry_t, dsn, 0, CYAML_UNLIMITED),
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
                "use_slab_allocator", CYAML_FLAG_DEFAULT | CYAML_FLAG_OPTIONAL,
                config_t, use_slab_allocator),

        CYAML_FIELD_MAPPING_PTR(
                "network", CYAML_FLAG_POINTER,
                config_t, network, config_network_schema),
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
