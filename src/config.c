#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <arpa/inet.h>
#include <cyaml/cyaml.h>

#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "log.h"
#include "fatal.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "worker/worker.h"

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
            level = LOG_LEVEL_INFO;
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

bool config_internal_validate_after_load(
        config_t* config) {
    // TODO: validate the cpus list if present, if all is in the list anything else can be there

    // TODO: if type == file in log sink, the file struct must be present

    // TODO: if keepalive struct is present, values must be allowed
    return true;
}

void config_internal_cyaml_free(
        config_t* config,
        cyaml_config_t* cyaml_config,
        cyaml_schema_value_t* schema) {
    cyaml_free(cyaml_config, schema, config, 0);
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

    if (config_internal_validate_after_load(config) == false) {
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
