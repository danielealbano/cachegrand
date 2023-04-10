/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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

#include "misc.h"
#include "clock.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_support.h"
#include "xalloc.h"

#include "log_sink_file.h"

#define TAG "log_sink_file"

log_sink_t *log_sink_file_init(
        log_level_t levels,
        log_sink_settings_t* settings) {

    int fd = open(
            settings->file.path,
            O_CLOEXEC | O_CREAT | O_APPEND | O_WRONLY,
            S_IRUSR | S_IWUSR | S_IRGRP);

    if (fd < 0) {
        LOG_W(TAG, "Unable to open <%s> for logging", settings->file.path);
        LOG_E_OS_ERROR(TAG);
        return NULL;
    }

    settings->file.internal.fd = fd;

    return log_sink_init(
            LOG_SINK_TYPE_FILE,
            levels,
            settings,
            log_sink_file_printer,
            log_sink_file_free);
}

void log_sink_file_free(
        log_sink_settings_t* settings) {
    if (settings->file.internal.fd > 0) {
        close(settings->file.internal.fd);
        settings->file.internal.fd = 0;
    }
}

void log_sink_file_printer(
        log_sink_settings_t* settings,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        size_t message_len) {
    char* log_message;
    char log_message_static_buffer[200] = { 0 };
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
    log_sink_support_printer_str(
            log_message,
            log_message_size,
            tag,
            timestamp,
            level,
            early_prefix_thread,
            message,
            message_len);

    // Ensure that the log message is always written entirely to the disk and to catch any error
    size_t data_written = 0;
    do {
        ssize_t data_written_now = write(
                settings->file.internal.fd,
                log_message + data_written,
                log_message_size - data_written);

        if (data_written_now < 0) {
            break;
        }

        data_written += data_written_now;
    } while (data_written < log_message_size);

    if (!log_message_static_buffer_selected) {
        xalloc_free(log_message);
    }
}
