#ifndef CACHEGRAND_CONFIG_CYAML_SCHEMA_H
#define CACHEGRAND_CONFIG_CYAML_SCHEMA_H

#ifdef __cplusplus
extern "C" {
#endif

// Allowed strings for config -> modules -> module -> network -> tls -> min_version (config_module_network_tls_min_version_t)
static cyaml_strval_t config_module_network_tls_min_version_schema_strings[] = {
        { "tls1.0", CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_0 },
        { "tls1.1", CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_1 },
        { "tls1.2", CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_2 },
        { "tls1.3", CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_TLS_1_3 },
        { "any", CONFIG_MODULE_NETWORK_TLS_MIN_VERSION_ANY },
};

// Allowed strings for config -> modules -> module -> network -> tls -> max_version (config_module_network_tls_max_version_t)
static cyaml_strval_t config_module_network_tls_max_version_schema_strings[] = {
        { "tls1.0", CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_0 },
        { "tls1.1", CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_1 },
        { "tls1.2", CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_2 },
        { "tls1.3", CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_TLS_1_3 },
        { "any", CONFIG_MODULE_NETWORK_TLS_MAX_VERSION_ANY },
};

// Allowed strings for for config -> modules -> module -> type (config_module_type_t)
static cyaml_strval_t config_module_type_schema_strings[] = {
        { "redis",      CONFIG_MODULE_TYPE_REDIS },
        { "prometheus", CONFIG_MODULE_TYPE_PROMETHEUS },
};

// Allowed strings for for config -> database -> backend
static cyaml_strval_t config_database_backend_schema_strings[] = {
        { "memory", CONFIG_DATABASE_BACKEND_MEMORY },
        { "file", CONFIG_DATABASE_BACKEND_FILE }
};

// Allowed strings for for config -> logs -> log -> type (config_log_type_t)
static cyaml_strval_t config_log_type_schema_strings[] = {
        { "console", CONFIG_LOG_TYPE_CONSOLE },
        { "file",    CONFIG_LOG_TYPE_FILE },
};

// Allowed strings for for config -> logs -> log -> level (config_log_level_t)
static cyaml_strval_t config_log_level_schema_strings[] = {
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

const cyaml_schema_value_t* config_cyaml_schema_get_top_schema();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_CONFIG_CYAML_SCHEMA_H
