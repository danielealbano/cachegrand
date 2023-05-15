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
#include <time.h>
#include <assert.h>
#include <unistd.h>

#include "misc.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_support.h"
#include "xalloc.h"

#include "log_sink_console.h"

static const char* log_sink_console_colors_fg[] = {
        "\x1B[0m", // reset
        "\x1B[91m", // light red
        "\x1B[93m", // light yellow
        "\x1B[37m", // light gray
        "\x1B[90m", // dark gray
        "\x1B[97m", // white
};

static const char* log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_MAX] = { 0 };
void log_sink_console_log_level_color_fg_lookup_init() {
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_ERROR] = log_sink_console_colors_fg[1];
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_WARNING] = log_sink_console_colors_fg[2];
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_VERBOSE] = log_sink_console_colors_fg[3];
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_DEBUG] = log_sink_console_colors_fg[4];
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_INFO] = log_sink_console_colors_fg[5];
    log_sink_console_log_level_color_fg_lookup[0] = log_sink_console_colors_fg[0];
}

log_sink_t *log_sink_console_init(
        log_level_t levels,
        log_sink_settings_t* settings) {
    if (log_sink_console_log_level_color_fg_lookup[0] == NULL) {
        log_sink_console_log_level_color_fg_lookup_init();
    }

    return log_sink_init(
            LOG_SINK_TYPE_CONSOLE,
            levels,
            settings,
            log_sink_console_printer,
            NULL);
}

void log_sink_console_printer(
        log_sink_settings_t* settings,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        size_t message_len) {
    char* log_message;
    char* log_message_beginning;
    char log_message_static_buffer[256] = { 0 };
    bool log_message_static_buffer_selected = false;
    size_t color_fg_desired_len = 0;
    size_t color_fg_reset_len = 0;
    bool level_has_color = log_sink_console_log_level_color_fg_lookup[level] != NULL;
    FILE* out = level == LOG_LEVEL_ERROR && !settings->console.use_stdout_for_errors
            ? stderr
            : stdout;

    // Disable color if the output is not a terminal
    if (!isatty(fileno(out))) {
        level_has_color = false;
    }

    if (level_has_color) {
        color_fg_desired_len = strlen(log_sink_console_log_level_color_fg_lookup[level]);
        color_fg_reset_len = strlen(log_sink_console_log_level_color_fg_lookup[0]);
    }

    size_t log_message_size = log_sink_support_printer_str_len(
            tag,
            early_prefix_thread,
            message_len);
    log_message_size += color_fg_desired_len + color_fg_reset_len;

    // Use the static buffer or allocate a new one
    log_message = log_buffer_static_or_alloc_new(
            log_message_static_buffer,
            sizeof(log_message_static_buffer),
            log_message_size,
            &log_message_static_buffer_selected);
    log_message_beginning = log_message;
    log_message_beginning[log_message_size] = 0;

    // Switch to the desired foreground color
    if (level_has_color) {
        strncpy(log_message, log_sink_console_log_level_color_fg_lookup[level], color_fg_desired_len);
        log_message += color_fg_desired_len;
    }

    // Write the log message
    log_message += log_sink_support_printer_str(
            log_message,
            log_message_size - (color_fg_desired_len + color_fg_reset_len),
            tag,
            timestamp,
            level,
            early_prefix_thread,
            message,
            message_len);

    // Restore the foreground color
    if (level_has_color) {
        strncpy(log_message, log_sink_console_log_level_color_fg_lookup[0], color_fg_reset_len);
        log_message += color_fg_reset_len;
    }

    fwrite(log_message_beginning, log_message_size, 1, out);
    fflush(out);

    assert(log_message == log_message_beginning + log_message_size);

    if (!log_message_static_buffer_selected) {
        xalloc_free(log_message_beginning);
    }
}
