/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "misc.h"
#include "log/log.h"
#include "xalloc.h"

#include "log_sink.h"

#include "log/sink/log_sink_console.h"
#include "log/sink/log_sink_file.h"

log_sink_t** log_sink_registered = NULL;
uint8_t log_sink_registered_count = 0;

bool log_sink_register(
        log_sink_t* sink) {
    if (sink == NULL) {
        return false;
    }

    log_sink_registered_count++;

    size_t size = sizeof(log_sink_t*) * log_sink_registered_count;
    if (log_sink_registered == NULL) {
        log_sink_registered = xalloc_alloc(size);
    } else {
        log_sink_registered = xalloc_realloc(log_sink_registered, size);
    }

    log_sink_registered[log_sink_registered_count - 1] = sink;

    return true;
}

log_sink_t *log_sink_init(
        log_sink_type_t type,
        log_level_t levels,
        log_sink_settings_t* settings,
        log_sink_printer_fn_t printer_fn,
        log_sink_free_fn_t free_fn) {
    if (!printer_fn) {
        return NULL;
    }

    log_sink_t* log_sink = xalloc_alloc_zero(sizeof(log_sink_t));

    log_sink->type = type;
    log_sink->levels = levels;
    log_sink->printer_fn = printer_fn;
    log_sink->free_fn = free_fn;

    memcpy(&log_sink->settings, settings, sizeof(log_sink_settings_t));

    return log_sink;
}

void log_sink_free(
        log_sink_t* log_sink) {
    if (log_sink->free_fn) {
        log_sink->free_fn(&log_sink->settings);
    }

    xalloc_free(log_sink);
}

log_sink_t* log_sink_factory(
        log_sink_type_t type,
        log_level_t levels,
        log_sink_settings_t* settings) {
    switch (type) {
        case LOG_SINK_TYPE_CONSOLE:
            return log_sink_console_init(levels, settings);

        case LOG_SINK_TYPE_FILE:
            return log_sink_file_init(levels, settings);

        default:
            return NULL;
    }
}

log_sink_t** log_sink_registered_get() {
    return log_sink_registered;
}

uint8_t log_sink_registered_count_get() {
    return log_sink_registered_count;
}

void log_sink_registered_free() {
    for(
            uint8_t log_sink_registered_index = 0;
            log_sink_registered_index < log_sink_registered_count && log_sink_registered_index < LOG_SINK_REGISTERED_MAX;
            log_sink_registered_index++) {
        log_sink_t* log_sink = log_sink_registered[log_sink_registered_index];
        log_sink_free(log_sink);
        log_sink_registered[log_sink_registered_index] = NULL;
    }

    log_sink_registered_count = 0;
    xalloc_free(log_sink_registered);
    log_sink_registered = NULL;
}
