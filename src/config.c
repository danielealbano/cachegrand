/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <cyaml/cyaml.h>
#include <limits.h>

#include "exttypes.h"
#include "misc.h"
#include "spinlock.h"
#include "transaction.h"
#include "xalloc.h"
#include "log/log.h"
#include "fatal.h"
#include "utils_cpu.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "support/simple_file_io.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "data_structures/hashtable/mcmp/hashtable.h"

#include "config.h"
#include "config_cyaml_config.h"
#include "config_cyaml_schema.h"

#include "network/channel/network_channel.h"

#define TAG "config"

void config_internal_cyaml_log(
        cyaml_log_t level_cyaml,
        __attribute__((unused)) void *ctx,
        const char *fmt,
        va_list args) {
    log_level_t level;
    bool fmt_has_newline = false;
    char* fmt_to_print = (char*)fmt;
    size_t fmt_len = strlen(fmt);

    switch(level_cyaml) {
        case CYAML_LOG_DEBUG:
            level = LOG_LEVEL_DEBUG;
            break;
        case CYAML_LOG_NOTICE:
        case CYAML_LOG_WARNING:
            level = LOG_LEVEL_WARNING;
            break;
        case CYAML_LOG_ERROR:
            level = LOG_LEVEL_ERROR;
            break;

        default:
        case CYAML_LOG_INFO:
            level = LOG_LEVEL_INFO;
            break;
    }

    // Counts how many new lines / carriage return are at the end of the log message
    while(fmt[fmt_len - 1] == '\r' || fmt[fmt_len - 1] == '\n') {
        fmt_len -= 1;
        fmt_has_newline = true;
    }

    // If any, clone the message to skip them
    if (fmt_has_newline) {
        fmt_to_print = xalloc_alloc(fmt_len + 1);
        strncpy(fmt_to_print, fmt, fmt_len);
        fmt_to_print[fmt_len] = 0;
    }

    log_message_internal(TAG, level, fmt_to_print, args);

    // If the message has been cloned, free it
    if (fmt_has_newline) {
        xalloc_free(fmt_to_print);
    }
}

cyaml_err_t config_internal_cyaml_load(
        config_t** config,
        char* config_path,
        cyaml_config_t* cyaml_config,
        cyaml_schema_value_t* schema) {
    return cyaml_load_file(
            config_path,
            cyaml_config,
            schema,
            (cyaml_data_t **)config,
            NULL);
}

bool config_parse_string_time(
        char *string,
        size_t string_len,
        bool allow_negative,
        bool allow_zero,
        bool allow_time_suffix,
        int64_t *return_value) {
    bool result = false;
    char *string_end;
    size_t string_end_len;
    int64_t string_value;

    // Skip any leading space
    while (isspace(string[0]) && string_len > 0) {
        string++;
        string_len--;
    }

    // Skip any trailing space
    while (isspace(string[string_len - 1]) && string_len > 0) {
        string_len--;
    }

    // If there are only spaces, skip them
    if (string_len == 0) {
        return false;
    }

    // As strtoll doesn't support non-null terminated strings, duplicate the string and null terminate it using strndup
    string = strndup(string, string_len);
    if (string == NULL) {
        return false;
    }

    // Try to parse the number
    string_value = strtoll(string, &string_end, 10);
    string_end_len = strlen(string_end);

    // Skip any leading space after parsing string_end
    while (isspace(string_end[0]) && string_end_len > 0) {
        string_end++;
        string_end_len--;
    }

    // Check if the end of the string matches the beginning of the string, if's true then the string is not a number
    if (string_end == string) {
        goto end;
    }

    // Check if string_value is negative
    if (!allow_negative && string_value < 0) {
        goto end;
    }

    // Check if string_value is zero
    if (!allow_zero && string_value == 0) {
        goto end;
    }

    if (!allow_time_suffix && string_end_len == 0) {
        *return_value = string_value;
        result = true;
        goto end;
    }

    // Check if the string is a size suffix
    if (allow_time_suffix && string_end_len == 1) {
        switch (string_end[0]) {
            case 's':
                string_value *= 1;
                break;
            case 'm':
                string_value *= 60;
                break;
            case 'h':
                string_value *= 60 * 60;
                break;
            case 'd':
                string_value *= 60 * 60 * 24;
                break;
        }

        *return_value = string_value;
        result = true;
        goto end;
    }

    end:
    free(string);
    return result;
}

bool config_parse_string_absolute_or_percent(
        char *string,
        size_t string_len,
        bool allow_negative,
        bool allow_zero,
        bool allow_percent,
        bool allow_absolute,
        bool allow_size_suffix,
        int64_t *return_value,
        config_parse_string_absolute_or_percent_return_value_t *return_value_type) {
    bool result = false;
    char *string_end;
    size_t string_end_len;
    int64_t string_value;

    // Skip any leading space
    while (isspace(string[0]) && string_len > 0) {
        string++;
        string_len--;
    }

    // Skip any trailing space
    while (isspace(string[string_len - 1]) && string_len > 0) {
        string_len--;
    }

    // If there are only spaces, skip them
    if (string_len == 0) {
        return false;
    }

    // As strtoll doesn't support non-null terminated strings, duplicate the string and null terminate it using strndup
    string = strndup(string, string_len);
    if (string == NULL) {
        return false;
    }

    // Try to parse the number
    string_value = strtoll(string, &string_end, 10);
    string_end_len = strlen(string_end);

    // Skip any leading space after parsing string_end
    while (isspace(string_end[0]) && string_end_len > 0) {
        string_end++;
        string_end_len--;
    }

    // Check if the end of the string matches the beginning of the string, if's true then the string is not a number
    if (string_end == string) {
        goto end;
    }

    // Check if string_value is negative
    if (!allow_negative && string_value < 0) {
        goto end;
    }

    // Check if string_value is zero
    if (!allow_zero && string_value == 0) {
        goto end;
    }

    // Check if the string is a percent
    if (allow_percent && string_end[0] == '%' && string_end_len == 1) {
        if (string_value > 100) {
            goto end;
        }

        *return_value = string_value;
        *return_value_type = CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_PERCENT;
        result = true;
        goto end;
    }

    if (!allow_absolute && !allow_size_suffix) {
        goto end;
    }

    // Check if string_end matches the end of the string, if's true and allow_absolute is true then the value
    // is absolute
    if (allow_absolute && string_end_len == 0) {
        *return_value = string_value;
        *return_value_type = CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE;
        result = true;
        goto end;
    }

    // Check if the string is followed by the b, kb, mb, gb, tb suffixes
    if (allow_size_suffix && string_end_len > 0) {
        int64_t string_value_multiplier;

        // Ensure that string_end is lowercase
        for (size_t i = 0; i < string_end_len; i++) {
            string_end[i] = (char)tolower((unsigned char)string_end[i]);
        }

        if (string_end[0] == 'b' && string_end_len == 1) {
            string_value_multiplier = 0;
        } else if (string_end[0] == 'k' && string_end[1] == 'b' && string_end_len == 2) {
            string_value_multiplier = 1;
        } else if (string_end[0] == 'm' && string_end[1] == 'b' && string_end_len == 2) {
            string_value_multiplier = 2;
        } else if (string_end[0] == 'g' && string_end[1] == 'b' && string_end_len == 2) {
            string_value_multiplier = 3;
        } else if (string_end[0] == 't' && string_end[1] == 'b' && string_end_len == 2) {
            string_value_multiplier = 4;
        } else {
            goto end;
        }

        for(int i = 0; i < string_value_multiplier; i++) {
            string_value *= 1024;
        }

        *return_value = string_value;
        *return_value_type = CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE;
        result = true;
        goto end;
    }

    end:
    free(string);
    return result;
}

bool config_validate_after_load_cpus(
        config_t* config) {
    bool return_result = true;
    int max_cpu_count = utils_cpu_count();

    // Validate that at least one CPU has been configured
    if (config->cpus_count == 0) {
        LOG_E(TAG, "No CPUs have been selected");
        return false;
    }

    // Allocate the errors array
    config_cpus_validate_error_t *errors = xalloc_alloc_zero(
            sizeof(config_cpus_validate_error_t) * config->cpus_count);

    // Validate the CPUs
    if (config_cpus_validate(
            max_cpu_count,
            config->cpus,
            config->cpus_count,
            errors) == false) {
        for(int cpu_index = 0; cpu_index < config->cpus_count; cpu_index++) {
            if (errors[cpu_index] == CONFIG_CPUS_VALIDATE_OK) {
                continue;
            }

            switch (errors[cpu_index]) {
                default:
                case CONFIG_CPUS_VALIDATE_ERROR_INVALID_CPU:
                    LOG_E(TAG, "CPU(s) selector <%d> is invalid", cpu_index);
                    break;

                case CONFIG_CPUS_VALIDATE_ERROR_MULTIPLE_RANGES:
                case CONFIG_CPUS_VALIDATE_ERROR_RANGE_TOO_SMALL:
                    LOG_E(TAG, "CPU(s) selector <%d> has an invalid range", cpu_index);
                    break;

                case CONFIG_CPUS_VALIDATE_ERROR_UNEXPECTED_CHARACTER:
                    LOG_E(TAG, "CPU(s) selector <%d> has an unexpected character", cpu_index);
                    break;

                case CONFIG_CPUS_VALIDATE_ERROR_NO_MULTI_CPUS_WITH_ALL:
                    LOG_E(TAG, "CPU(s) selector <%d> is invalid, all the CPUs have already been selected", cpu_index);
                    break;
            }
        }

        return_result = false;
    }

    xalloc_free(errors);

    return return_result;
}

bool config_validate_after_load_database_backend_file(
        config_t* config) {
    bool return_result = true;

    if (config->database->backend != CONFIG_DATABASE_BACKEND_FILE) {
        return return_result;
    }

    if (config->database->file == NULL) {
        LOG_E(TAG, "The database backend is set to <file> but the <file> settings are not present");
        return_result = false;
    }

    if (config->database->memory != NULL) {
        LOG_E(TAG, "The database backend is set to <file> but the <memory> settings are present");
        return_result = false;
    }

    if (config->database->file->limits && config->database->file->limits->soft
        && config->database->file->limits->soft->max_disk_usage >=
           config->database->file->limits->hard->max_disk_usage) {
        LOG_E(TAG, "The soft limit for the maximum disk usage must be smaller than the hard limit");
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_database_backend_memory(
        config_t* config) {
    bool return_result = true;

    if (config->database->backend != CONFIG_DATABASE_BACKEND_MEMORY) {
        return return_result;
    }

    if (config->database->memory == NULL) {
        LOG_E(TAG, "The database backend is set to <memory> but the <memory> settings are not present");
        return_result = false;
    }

    if (config->database->file != NULL) {
        LOG_E(TAG, "The database backend is set to <memory> but the <file> settings are present");
        return_result = false;
    }

    if (config->database->memory->limits && config->database->memory->limits->soft
        && config->database->memory->limits->soft->max_memory_usage >=
           config->database->memory->limits->hard->max_memory_usage) {
        LOG_E(TAG, "The soft limit for the maximum disk usage must be smaller than the hard limit");
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_database_snapshots(
        config_t* config) {
    bool return_result = true;

    if (!config->database->snapshots) {
        return return_result;
    }

    // Ensure that the path is not longer than PATH_MAX
    if (strlen(config->database->snapshots->path) > PATH_MAX) {
        LOG_E(TAG, "The path for the snapshots is too long");
        return_result = false;
    }

    // Check that the maximum allowed interval is 7 days
    if (config->database->snapshots->interval_ms > 7 * 24 * 60 * 60 * 1000) {
        LOG_E(TAG, "The maximum allowed interval for the snapshots is <7> days");
        return_result = false;
    }

    // Ensure that if rotation is enabled, the maximum number of max_files is equal or greater than 0 and smaller than
    // uint16_t
    if (config->database->snapshots->rotation && (
            config->database->snapshots->rotation->max_files < 2 ||
            config->database->snapshots->rotation->max_files > 65535)) {
        LOG_E(TAG, "The maximum number of files for the snapshots rotation must be between <2> and <65535>");
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_database_limits(
        config_t* config) {
    bool return_result = true;

    if (!config->database->limits) {
        return return_result;
    }

    if (config->database->limits->soft
        && config->database->limits->soft->max_keys >= config->database->limits->hard->max_keys) {
        LOG_E(TAG, "The soft limit for the maximum number of keys must be smaller than the hard limit");
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_database_keys_eviction(
        config_t* config) {
    bool return_result = true;

    if (!config->database->keys_eviction) {
        return return_result;
    }

    if (config->database->keys_eviction->policy == CONFIG_DATABASE_KEYS_EVICTION_POLICY_TTL
        && config->database->keys_eviction->only_ttl == false) {
        LOG_E(TAG, "The keys eviction policy <ttl> requires <only_ttl> set to <true>");
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_database(
        config_t* config) {
    bool return_result = true;

    if (config->database->max_user_databases < 1) {
        LOG_E(TAG, "The maximum number of user databases must be greater than <0>");
        return_result = false;
    } else if (config->database->max_user_databases > UINT8_MAX) {
        LOG_E(TAG, "The maximum number of user databases must be smaller than <%d>", UINT8_MAX);
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_modules_network_timeout(
        config_module_t *module) {
    bool return_result = true;

    if (module->network->timeout->read_ms < -1 || module->network->timeout->read_ms == 0) {
        LOG_E(
                TAG,
                "In module <%s>, read_ms timeout can only be <-1> or a value greater than <0>",
                module->type);
        return_result = false;
    }

    if (module->network->timeout->write_ms < -1 || module->network->timeout->write_ms == 0) {
        LOG_E(
                TAG,
                "In module <%s>, read_ms timeout can only be <-1> or a value greater than <0>",
                module->type);
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_modules_network_bindings(
        config_module_t *module) {
    bool return_result = true;
    bool tls_enabled = module->network->tls != NULL;

    for(int binding_index = 0; binding_index < module->network->bindings_count; binding_index++) {
        config_module_network_binding_t *binding = &module->network->bindings[binding_index];

        // Ensure that if the binding requires tls than tls is enabled
        if (binding->tls && tls_enabled == false) {
            LOG_E(
                    TAG,
                    "In module <%s>, the binding <%s:%d> requires tls but tls is not configured",
                    module->type,
                    binding->host,
                    binding->port);
            return_result = false;
        }
    }

    return return_result;
}

bool config_validate_after_load_modules_network_tls(
        config_module_t *module) {
    bool return_result = true;

    if (module->network->tls == NULL) {
        return true;
    }

    if (!simple_file_io_exists(module->network->tls->certificate_path)) {
        LOG_E(
                TAG,
                "In module <%s>, the certificate <%s> doesn't exist",
                module->type,
                module->network->tls->certificate_path);
        return_result = false;
    }

    if (!simple_file_io_exists(module->network->tls->private_key_path)) {
        LOG_E(
                TAG,
                "In module <%s>, the private key <%s> doesn't exist",
                module->type,
                module->network->tls->private_key_path);
        return_result = false;
    }

    if (module->network->tls->ca_certificate_chain_path &&
        !simple_file_io_exists(module->network->tls->ca_certificate_chain_path)) {
        LOG_E(
                TAG,
                "In module <%s>, the ca certificate chain <%s> doesn't exist",
                module->type,
                module->network->tls->ca_certificate_chain_path);
        return_result = false;
    }

    if (module->network->tls->verify_client_certificate && !module->network->tls->ca_certificate_chain_path) {
        LOG_E(
                TAG,
                "In module <%s>, the client certificate verification requires a ca certificate chain",
                module->type);
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_modules_network_keepalive(
        config_module_t *module) {
    bool return_result = true;

    if (module->network->keepalive == NULL) {
        return true;
    }

    if (module->network->keepalive->time == 0) {
        LOG_E(
                TAG,
                "In module <%s>, the keepalive time has to be greater than zero",
                module->type);
        return_result = false;
    }

    if (module->network->keepalive->interval == 0) {
        LOG_E(
                TAG,
                "In module <%s>, the keepalive interval has to be greater than zero",
                module->type);
        return_result = false;
    }

    if (module->network->keepalive->probes == 0) {
        LOG_E(
                TAG,
                "In module <%s>, the keepalive probes has to be greater than zero",
                module->type);
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_modules_config_valid(
        config_t *config,
        config_module_t *config_module) {
    bool return_result = true;

    // Don't rely on config_module->module_id as it can be zero because there is no module matching the type, try
    // instead to search for a module that has the same name
    module_t *module = module_get_by_name(config_module->type);

    // If the module was not found, log an error
    if (!module) {
        LOG_E(
                TAG,
                "Module type <%s> unsupported",
                config_module->type);
        return_result = false;
    } else {
        // If the module exports config_validate_after_load, use it to validate the config
        if (module->config_validate_after_load && module->config_validate_after_load(config, config_module) == false) {
            return_result = false;
        }
    }

    return return_result;
}

bool config_validate_after_load_modules(
        config_t* config) {
    bool return_result = true;

    for(int module_index = 0; module_index < config->modules_count; module_index++) {
        config_module_t *config_module = &config->modules[module_index];

        if (config_validate_after_load_modules_config_valid(config, config_module) == false
            || config_validate_after_load_modules_network_timeout(config_module) == false
            || config_validate_after_load_modules_network_keepalive(config_module) == false
            || config_validate_after_load_modules_network_tls(config_module) == false
            || config_validate_after_load_modules_network_bindings(config_module) == false) {
            return_result = false;
        }
    }

    return return_result;
}

bool config_validate_after_load_log_file(
        config_log_t *log) {
    bool return_result = true;

    if (log->type != CONFIG_LOG_TYPE_FILE) {
        return true;
    }

    if (log->file == NULL) {
        LOG_E(
                TAG,
                "For log type <%s>, the <file> settings must be present",
                config_log_type_schema_strings[log->type].str);
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_logs(
        config_t* config) {
    bool return_result = true;

    for(int log_index = 0; log_index < config->logs_count; log_index++) {
        config_log_t *log = &config->logs[log_index];

        if (config_validate_after_load_log_file(log) == false) {
            return_result = false;
        }
    }

    return return_result;
}

bool config_validate_after_load(
        config_t* config) {
    bool return_result = true;

    if (config_validate_after_load_cpus(config) == false
        || config_validate_after_load_database_backend_file(config) == false
        || config_validate_after_load_database_backend_memory(config) == false
        || config_validate_after_load_database_snapshots(config) == false
        || config_validate_after_load_database_limits(config) == false
        || config_validate_after_load_database_keys_eviction(config) == false
        || config_validate_after_load_database(config) == false
        || config_validate_after_load_modules(config) == false
        || config_validate_after_load_logs(config) == false) {
        return_result = false;
    }

    return return_result;
}

void config_internal_cyaml_free(
        config_t* config,
        cyaml_config_t* cyaml_config,
        cyaml_schema_value_t* schema) {
    cyaml_free(cyaml_config, schema, config, 0);
}

bool config_cpus_validate(
        uint16_t max_cpus_count,
        char** cpus,
        unsigned cpus_count,
        config_cpus_validate_error_t* config_cpus_validate_errors) {
    bool error = false;

    bool has_all = false;
    for(uint32_t cpu_index = 0; cpu_index < cpus_count; cpu_index++) {
        bool cpu_is_range = false;
        long cpu_number;
        long cpu_number_range_start = -1;
        char* cpu = cpus[cpu_index];
        char* cpu_end = NULL;

        if (has_all) {
            config_cpus_validate_errors[cpu_index] = CONFIG_CPUS_VALIDATE_ERROR_NO_MULTI_CPUS_WITH_ALL;
            continue;
        }

        if (strncasecmp(cpu, "all", 3) == 0) {
            if (strlen(cpu) > 3) {
                config_cpus_validate_errors[cpu_index] = CONFIG_CPUS_VALIDATE_ERROR_INVALID_CPU;
            } else if (cpu_index > 0) {
                config_cpus_validate_errors[cpu_index] = CONFIG_CPUS_VALIDATE_ERROR_NO_MULTI_CPUS_WITH_ALL;
            } else {
                has_all = true;
            }

            continue;
        }

        do {
            cpu_number = strtol(cpu, &cpu_end, 10);

            // If the cpu_number failed to be parsed, the cpu_number is greater than ULONG_MAX or if it is greater than
            // the max cpus (the linux kernel currently supports up to 4096 cpus but better to do not hard-code the
            // check) report an error, better to make the user aware that something may be wrong
            if (
                    cpu_end == cpu || errno == ERANGE || cpu_number > UINT16_MAX || cpu_number > max_cpus_count ||
                    cpu_number < 0) {
                error = true;
                config_cpus_validate_errors[cpu_index] = CONFIG_CPUS_VALIDATE_ERROR_INVALID_CPU;
                break;
            }

            if (*cpu_end == '-') {
                // There can't be more than one range
                if (cpu_is_range) {
                    error = true;
                    config_cpus_validate_errors[cpu_index] = CONFIG_CPUS_VALIDATE_ERROR_MULTIPLE_RANGES;
                    break;
                }

                cpu = ++cpu_end;
                cpu_number_range_start = cpu_number;
                cpu_is_range = true;

                continue;
            } else if (*cpu_end == 0) {
                if (cpu_is_range) {
                    if (cpu_number - cpu_number_range_start + 1 < 2) {
                        config_cpus_validate_errors[cpu_index] = CONFIG_CPUS_VALIDATE_ERROR_RANGE_TOO_SMALL;
                        error = true;
                        break;
                    }
                }
            } else {
                config_cpus_validate_errors[cpu_index] = CONFIG_CPUS_VALIDATE_ERROR_UNEXPECTED_CHARACTER;
                error = true;
                break;
            }
        } while(*cpu_end != 0);
    }

    return !error;
}

bool config_cpus_parse(
        uint16_t max_cpus_count,
        char** cpus,
        unsigned cpus_count,
        uint16_t** cpus_map,
        uint16_t* cpus_map_count) {
    bool error = false;
    bool select_all_cpus = false;

    uint16_t* int_cpus_map = NULL;
    uint16_t int_cpus_map_count = 0;

    for(uint32_t cpu_index = 0; cpu_index < cpus_count; cpu_index++) {
        bool cpu_is_range = false;
        long cpu_number;
        long cpu_number_range_start = -1;
        long cpu_number_range_end = -1;
        long cpu_number_range_len = -1;
        char* cpu = cpus[cpu_index];
        char* cpu_end = NULL;

        if (strncasecmp(cpu, "all", 3) == 0) {
            select_all_cpus = true;
            continue;
        }

        do {
            cpu_number = strtol(cpu, &cpu_end, 10);

            // If the cpu_number failed to be parsed, the cpu_number is greater than ULONG_MAX or if it is greater than
            // the max cpus (the linux kernel currently supports up to 4096 cpus but better to do not hard-code the
            // check) report an error, better to make the user aware that something may be wrong
            if (
                    cpu_end == cpu || errno == ERANGE || cpu_number > UINT16_MAX || cpu_number > max_cpus_count ||
                    cpu_number < 0) {
                error = true;
                break;
            }

            if (*cpu_end == '-') {
                // There can't be more than one range
                if (cpu_is_range) {
                    error = true;
                    break;
                }

                cpu = ++cpu_end;
                cpu_number_range_start = cpu_number;
                cpu_is_range = true;

                continue;
            } else if (*cpu_end == 0) {
                if (cpu_is_range) {
                    cpu_number_range_end = cpu_number;
                    cpu_number_range_len = cpu_number_range_end - cpu_number_range_start + 1;
                    if (cpu_number_range_len < 2) {
                        error = true;
                        break;
                    }

                    int_cpus_map_count += cpu_number_range_len;
                } else {
                    int_cpus_map_count++;
                }
            } else {
                // Unexpected char, error
                error = true;
                break;
            }
        } while(*cpu_end != 0);

        if (error) {
            break;
        }

        int_cpus_map = xalloc_realloc(int_cpus_map, sizeof(uint16_t) * int_cpus_map_count);

        if (!cpu_is_range) {
            int_cpus_map[int_cpus_map_count - 1] = cpu_number;
        } else if (cpu_number_range_len > -1) {
            uint16_t int_cpus_map_initial_index = int_cpus_map_count - cpu_number_range_len;
            for(cpu_number = cpu_number_range_start; cpu_number <= cpu_number_range_end; cpu_number++) {
                uint16_t int_cpus_map_index = int_cpus_map_initial_index + (cpu_number - cpu_number_range_start);
                int_cpus_map[int_cpus_map_index] = cpu_number;
            }
        }
    }

    if (error) {
        return false;
    }

    if (cpus_count == 0 || select_all_cpus) {
        if (int_cpus_map) {
            xalloc_free(int_cpus_map);
        }
        int_cpus_map_count = max_cpus_count;
        int_cpus_map = xalloc_alloc(sizeof(uint16_t) * int_cpus_map_count);

        for(long cpu_number = 0; cpu_number < max_cpus_count; cpu_number++) {
            int_cpus_map[cpu_number] = cpu_number;
        }
    }

    *cpus_map = int_cpus_map;
    *cpus_map_count = int_cpus_map_count;

    return true;
}

void config_cpus_filter_duplicates(
        const uint16_t* cpus,
        uint16_t cpus_count,
        uint16_t** unique_cpus,
        uint16_t* unique_cpus_count) {
    char* int_unique_cpu_set;
    uint16_t* int_unique_cpus;
    uint16_t int_unique_cpus_count = 0;

    for(uint16_t cpu_index = 0; cpu_index < cpus_count; cpu_index++) {
        uint16_t cpu_number = cpus[cpu_index];
        int_unique_cpus_count = cpu_number > int_unique_cpus_count ? cpu_number : int_unique_cpus_count;
    }

    if (int_unique_cpus_count == 0) {
        *unique_cpus = NULL;
        *unique_cpus_count = int_unique_cpus_count;
        return;
    }

    int_unique_cpus = xalloc_alloc_zero(sizeof(uint16_t) * int_unique_cpus_count);
    int_unique_cpu_set = xalloc_alloc_zero(sizeof(char) * int_unique_cpus_count + 1);

    uint16_t int_unique_cpu_index = 0;
    for(uint16_t cpu_index = 0; cpu_index < cpus_count; cpu_index++) {
        uint16_t cpu_number = cpus[cpu_index];

        if (int_unique_cpu_set[cpu_number] == 0) {
            int_unique_cpus[int_unique_cpu_index] = cpu_number;
            int_unique_cpu_index++;
        }

        int_unique_cpu_set[cpu_number] = 1;
    }

    xalloc_free(int_unique_cpu_set);

    *unique_cpus = int_unique_cpus;
    *unique_cpus_count = int_unique_cpus_count;
}

bool config_process_string_values(
        config_t *config) {
    config_parse_string_absolute_or_percent_return_value_t return_value_type;

    // Check if snapshots are enabled
    if (config->database->snapshots) {
        config->database->snapshots->min_data_changed = 0;

        bool result = config_parse_string_time(
                config->database->snapshots->interval_str,
                strlen(config->database->snapshots->interval_str),
                false,
                false,
                true,
                &config->database->snapshots->interval_ms);

        // The returned time is in seconds, so multiply by 1000 to convert to ms
        config->database->snapshots->interval_ms *= 1000;

        if (!result) {
            LOG_E(TAG, "Failed to parse the snapshot interval");
            config_free(config);
            return NULL;
        }

        if (config->database->snapshots->min_data_changed_str) {
            result = config_parse_string_absolute_or_percent(
                    config->database->snapshots->min_data_changed_str,
                    strlen(config->database->snapshots->min_data_changed_str),
                    false,
                    true,
                    false,
                    true,
                    true,
                    &config->database->snapshots->min_data_changed,
                    &return_value_type);

            if (!result) {
                LOG_E(TAG, "Failed to parse the snapshot minimum data changed");
                config_free(config);
                return NULL;
            }
        }
    }

    // Check if the memory backend for the database is defined in the config, if yes process the hard and soft memory
    // limits, the latter is optional
    if (config->database->memory) {
        // Get the total system memory
        struct sysinfo sys_info;

        // Get the system memory information
        if (sysinfo(&sys_info) < 0) {
            LOG_E(TAG, "Failed to get the system memory information");
            return false;
        }

        // Convert the string value of the hard memory limit into a numeric value, it allows absolute values,
        // percentages and size suffixes (e.g. GB, MB, KB, etc.)
        bool result = config_parse_string_absolute_or_percent(
                config->database->memory->limits->hard->max_memory_usage_str,
                strlen(config->database->memory->limits->hard->max_memory_usage_str),
                false,
                false,
                true,
                true,
                true,
                &config->database->memory->limits->hard->max_memory_usage,
                &return_value_type);

        if (!result) {
            LOG_E(TAG, "Failed to parse the hard memory limit");
            return false;
        }

        // If the value is a percentage, convert it to an absolute value by multiplying it with the total system memory
        if (return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_PERCENT) {
            config->database->memory->limits->hard->max_memory_usage = (int64_t)(
                    (double)sys_info.totalram *
                    ((double)config->database->memory->limits->hard->max_memory_usage / 100.0));
        }

        // Check if the soft limit is defined, if yes process it
        if (config->database->memory->limits->soft) {
            // Convert the string value of the hard memory limit into a numeric value, it allows absolute values,
            // percentages and size suffixes (e.g. GB, MB, KB, etc.)
            result = config_parse_string_absolute_or_percent(
                    config->database->memory->limits->soft->max_memory_usage_str,
                    strlen(config->database->memory->limits->soft->max_memory_usage_str),
                    false,
                    false,
                    true,
                    true,
                    true,
                    &config->database->memory->limits->soft->max_memory_usage,
                    &return_value_type);

            if (!result) {
                LOG_E(TAG, "Failed to parse the soft memory limit");
                return false;
            }

            // If the value is a percentage, convert it to an absolute value by multiplying it with the total system
            // memory
            if (return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_PERCENT) {
                config->database->memory->limits->soft->max_memory_usage =
                        (int64_t)((double)sys_info.totalram *
                                  ((double)config->database->memory->limits->soft->max_memory_usage / 100.0));
            }
        }
    }

    // Check if the file backend for the database is defined in the config, if yes process the hard and soft disk
    // limits, the latter is optional
    if (config->database->file) {
        size_t total_disk_size;
        struct statvfs vfs;

        // Read the total disk size for the path where the database file is supposed to be stored
        if (statvfs(config->database->file->path, &vfs) == 0) {
            total_disk_size = vfs.f_blocks * vfs.f_frsize;
        } else {
            LOG_E(TAG, "Failed to get the disk information");
            return false;
        }

        // Convert the string value of the hard disk usage limit into a numeric value, it allows absolute values,
        bool result = config_parse_string_absolute_or_percent(
                config->database->file->limits->hard->max_disk_usage_str,
                strlen(config->database->file->limits->hard->max_disk_usage_str),
                false,
                false,
                true,
                true,
                true,
                &config->database->file->limits->hard->max_disk_usage,
                &return_value_type);

        if (!result) {
            LOG_E(TAG, "Failed to parse the hard disk usage limit");
            return false;
        }

        // If the value is a percentage, convert it to an absolute value by multiplying it with the total disk size
        if (return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_PERCENT) {
            config->database->file->limits->hard->max_disk_usage = (int64_t)(
                    (double)total_disk_size *
                    ((double)config->database->file->limits->hard->max_disk_usage / 100.0));
        }

        // Check if the soft limit is defined, if yes process it
        if (config->database->file->limits->soft) {
            // Convert the string value of the hard disk usage limit into a numeric value, it allows absolute values,
            // percentages and size suffixes (e.g. GB, MB, KB, etc.)
            result = config_parse_string_absolute_or_percent(
                    config->database->file->limits->soft->max_disk_usage_str,
                    strlen(config->database->file->limits->soft->max_disk_usage_str),
                    false,
                    false,
                    true,
                    true,
                    true,
                    &config->database->file->limits->soft->max_disk_usage,
                    &return_value_type);

            if (!result) {
                LOG_E(TAG, "Failed to parse the soft disk usage limit");
                return false;
            }

            // If the value is a percentage, convert it to an absolute value by multiplying it with the total disk size
            if (return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_PERCENT) {
                config->database->file->limits->soft->max_disk_usage =
                        (int64_t)((double)total_disk_size *
                                  ((double)config->database->file->limits->soft->max_disk_usage / 100.0));
            }
        }
    }

    if (config->database->enforced_ttl) {
        if (config->database->enforced_ttl->default_ttl_str) {
            bool result = config_parse_string_time(
                    config->database->enforced_ttl->default_ttl_str,
                    strlen(config->database->enforced_ttl->default_ttl_str),
                    false,
                    false,
                    true,
                    &config->database->enforced_ttl->default_ttl_ms);

            if (!result) {
                LOG_E(TAG, "Failed to parse the default ttl");
                return false;
            }

            config->database->enforced_ttl->default_ttl_ms *= 1000;
        }

        if (config->database->enforced_ttl->max_ttl_str) {
            bool result = config_parse_string_time(
                    config->database->enforced_ttl->max_ttl_str,
                    strlen(config->database->enforced_ttl->max_ttl_str),
                    false,
                    false,
                    true,
                    &config->database->enforced_ttl->max_ttl_ms);

            if (!result) {
                LOG_E(TAG, "Failed to parse the max ttl");
                return false;
            }

            config->database->enforced_ttl->max_ttl_ms *= 1000;
        }
    }

    return true;
}

config_t* config_load(
        char* config_path) {
    cyaml_err_t err;
    config_t* config = NULL;

    LOG_I(TAG, "Loading the configuration from %s", config_path);

    // Load the configuration from the yaml file
    if ((err = config_internal_cyaml_load(
            &config,
            config_path,
            config_cyaml_config_get_global(),
            (cyaml_schema_value_t*)config_cyaml_schema_get_top_schema()))!= CYAML_OK) {
        // The operation failed, log the error
        LOG_E(TAG, "Failed to load the configuration: %s", cyaml_strerror(err));
        config = NULL;
    }

    // Potentially config_internal_cyaml_load can return NULL even when parsing the yaml file was successful, so we
    // need to check for that
    if (!config) {
        return NULL;
    }

    // Set the module_id in the config and invoke the config prepare
    for(int module_index = 0; module_index < config->modules_count; module_index++) {
        config_module_t *config_module = &config->modules[module_index];
        module_t *module = module_get_by_name(config_module->type);

        // If the module was not found, skip it
        if (!module) {
            continue;
        }

        config_module->module_id = module->id;

        if (module->config_prepare && module->config_prepare(config_module) == false) {
            LOG_E(TAG, "Failed to prepare the configuration for module <%s>", config_module->type);
            config_free(config);
            return NULL;
        }
    }

    // Before we start to validate the configuration it's necessary to convert the values that are stored as strings
    // into the appropriate types, e.g. to convert the string "1G" into the uint64_t value 1073741824 or to convert the
    // string "50%" for the memory limits to 50% of the total system memory, etc.
    if (config_process_string_values(config) == false) {
        config_free(config);
        return NULL;
    }

    // Tries to validate the configuration
    if (config_validate_after_load(config) == false) {
        LOG_E(TAG, "Failed to validate the configuration");
        config_free(config);
        return NULL;
    }

    return config;
}

void config_free(
        config_t* config) {
    config_internal_cyaml_free(
            config,
            config_cyaml_config_get_global(),
            (cyaml_schema_value_t*)config_cyaml_schema_get_top_schema());
}
