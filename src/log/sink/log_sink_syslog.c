/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syslog.h>

#include "misc.h"
#include "clock.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_support.h"
#include "xalloc.h"

#include "log_sink_syslog.h"

#define TAG "log_sink_syslog"

static bool log_sink_syslog_log_level_map_initialized = false;
static int log_sink_syslog_log_level_map[LOG_LEVEL_MAX] = { 0 };
void log_sink_syslog_log_level_map_init() {
    log_sink_syslog_log_level_map[LOG_LEVEL_ERROR] = LOG_ERR;
    log_sink_syslog_log_level_map[LOG_LEVEL_WARNING] = LOG_WARNING;
    log_sink_syslog_log_level_map[LOG_LEVEL_INFO] = LOG_NOTICE;
    log_sink_syslog_log_level_map[LOG_LEVEL_VERBOSE] = LOG_INFO;
    log_sink_syslog_log_level_map[LOG_LEVEL_DEBUG] = LOG_DEBUG;

    log_sink_syslog_log_level_map_initialized = true;
}

log_sink_t *log_sink_syslog_init(
        log_level_t levels,
        log_sink_settings_t* settings) {
    // Initialize the log level map if not already initialized
    if (!log_sink_syslog_log_level_map_initialized) {
        log_sink_syslog_log_level_map_init();
        log_sink_syslog_log_level_map_initialized = true;
    }

    // Open syslog
    openlog(CACHEGRAND_CMAKE_CONFIG_NAME, LOG_CONS | LOG_NDELAY | LOG_PID, LOG_USER);

    // Mark the syslog as opened
    settings->syslog.internal.opened = true;

    return log_sink_init(
            LOG_SINK_TYPE_SYSLOG,
            levels,
            settings,
            log_sink_syslog_printer,
            log_sink_syslog_free);
}

void log_sink_syslog_free(
        log_sink_settings_t* settings) {
    if (settings->syslog.internal.opened == true) {
        closelog();
    }
}

void log_sink_syslog_printer(
        log_sink_settings_t* settings,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        size_t message_len) {
    char* log_message;
    char log_message_static_buffer[256] = { 0 };
    bool log_message_static_buffer_selected = false;

    size_t log_message_size = log_sink_support_printer_str_len(
            tag,
            early_prefix_thread,
            message_len);

    log_message = log_buffer_static_or_alloc_new(
            log_message_static_buffer,
            sizeof(log_message_static_buffer),
            log_message_size,
            &log_message_static_buffer_selected);

    // Write the log message
    log_sink_support_printer_simple_str(
            log_message,
            log_message_size,
            tag,
            early_prefix_thread,
            message,
            message_len);

    // Write the log message to syslog
    syslog(LOG_USER | log_sink_syslog_log_level_map[level], "%s", log_message);

    if (!log_message_static_buffer_selected) {
        xalloc_free(log_message);
    }
}
