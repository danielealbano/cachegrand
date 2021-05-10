#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <cyaml/cyaml.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "log/log.h"
#include "fatal.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"

#include "config.h"
#include "config_cyaml_config.h"
#include "config_cyaml_schema.h"

#define TAG "config"

void config_internal_cyaml_log(
        cyaml_log_t level_cyaml,
        void *ctx,
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
            level = LOG_LEVEL_WARNING;
            break;
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
    while(fmt[fmt_len - 1] == 0x0a || fmt[fmt_len - 1] == 0x0c) {
        fmt_len -= 1;
        fmt_has_newline = true;
    }

    // If any, clone the message to skip them
    if (fmt_has_newline) {
        fmt_to_print = malloc(fmt_len + 1);
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
    return cyaml_load_file(config_path, cyaml_config, schema, (cyaml_data_t **)config, NULL);
}

bool config_validate_after_load(
        config_t* config) {
    // TODO: validate the cpus list if present, if all is in the list anything else can be there

    // TODO: if type == file in log sink, the file struct must be present (path will be present because of schema
    //       validation)

    // TODO: for log sinks, if all is set only negate flags can be set

    // TODO: if keepalive struct is present, values must be allowed
    return true;
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

    for(uint32_t cpu_index = 0; cpu_index < cpus_count; cpu_index++) {
        bool cpu_is_range = false;
        long cpu_number = -1;
        long cpu_number_range_start = -1;
        char* cpu = cpus[cpu_index];
        char* cpu_end = NULL;

        if (strncasecmp(cpu, "all", 3) == 0) {
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
        long cpu_number = -1;
        long cpu_number_range_start = -1;
        long cpu_number_range_end = -1;
        long cpu_number_range_len = -1;
        char* cpu = cpus[cpu_index];
        char* cpu_end = NULL;

        if (strncasecmp(cpu, "all", 3) == 0) {
            select_all_cpus = true;
            break;
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
        uint16_t* cpus,
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
        LOG_E(TAG, "Failed loading the configuration: %s", cyaml_strerror(err));
    }

    if (config_validate_after_load(config) == false) {
        config_free(config);
        config = NULL;
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
