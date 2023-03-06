/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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
#include <errno.h>
#include <arpa/inet.h>
#include <cyaml/cyaml.h>

#include "exttypes.h"
#include "misc.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "xalloc.h"
#include "log/log.h"
#include "fatal.h"
#include "utils_cpu.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
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
        && config->database->keys_eviction->ignore_ttl == true) {
        LOG_E(TAG, "The keys eviction policy can't be set to <ttl> if <ignore_ttl> is set to <true>");
        return_result = false;
    }

    if (config->database->keys_eviction->batch_size < 1) {
        LOG_E(TAG, "The keys eviction batch size must be greater than 0");
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
                config_module_type_schema_strings[module->type].str);
        return_result = false;
    }

    if (module->network->timeout->write_ms < -1 || module->network->timeout->write_ms == 0) {
        LOG_E(
                TAG,
                "In module <%s>, read_ms timeout can only be <-1> or a value greater than <0>",
                config_module_type_schema_strings[module->type].str);
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
                    config_module_type_schema_strings[module->type].str,
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
                config_module_type_schema_strings[module->type].str,
                module->network->tls->certificate_path);
        return_result = false;
    }

    if (!simple_file_io_exists(module->network->tls->private_key_path)) {
        LOG_E(
                TAG,
                "In module <%s>, the private key <%s> doesn't exist",
                config_module_type_schema_strings[module->type].str,
                module->network->tls->private_key_path);
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_modules_redis(
        config_module_t *module) {
    bool return_result = true;

    if (module->type != CONFIG_MODULE_TYPE_REDIS) {
        return true;
    }

    if (module->redis->max_key_length > FFMA_OBJECT_SIZE_MAX - 1) {
        LOG_E(
                TAG,
                "In module <%s>, the allowed maximum value of max_key_length is <%u>",
                config_module_type_schema_strings[module->type].str,
                FFMA_OBJECT_SIZE_MAX - 1);
        return_result = false;
    }

    return return_result;
}

bool config_validate_after_load_modules(
        config_t* config) {
    bool return_result = true;

    for(int module_index = 0; module_index < config->modules_count; module_index++) {
        config_module_t *module = &config->modules[module_index];

        if (config_validate_after_load_modules_network_timeout(module) == false
            || config_validate_after_load_modules_network_tls(module) == false
            || config_validate_after_load_modules_network_bindings(module) == false
            || config_validate_after_load_modules_redis(module) == false) {
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
        || config_validate_after_load_database_limits(config) == false
        || config_validate_after_load_database_keys_eviction(config) == false
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

config_t* config_load(
        char* config_path) {
    config_t* config = NULL;

    LOG_I(TAG, "Loading the configuration from %s", config_path);

    cyaml_err_t err = config_internal_cyaml_load(
            &config,
            config_path,
            config_cyaml_config_get_global(),
            (cyaml_schema_value_t*)config_cyaml_schema_get_top_schema());
    if (err != CYAML_OK) {
        LOG_E(TAG, "Failed to load the configuration: %s", cyaml_strerror(err));
        config = NULL;
    }

    if (config) {
        if (config_validate_after_load(config) == false) {
            LOG_E(TAG, "Failed to validate the configuration");
            config_free(config);
            config = NULL;
        }
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
