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
#include <time.h>
#include <locale.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "misc.h"
#include "xalloc.h"
#include "mimalloc.h"

#include "log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_console.h"

// TODO: Everytime a thread tries to submit a message to be logged should check if it has been registered with the
//       logging system and if not it should register, threads are not spawn dynamically so the logging thread will
//       waste away checks on ended threads, but if it will be necessary a GC can be performed on threads that are not
//       logging and the logging mechanism will take care of re-registering the thread if it has been de-registered
//       simply tracking the registration status.
//       The registration mechanism must be provided in a transparent way by the logging mechanism.
//       To make the logging thread safe and fast enough to cope with the performance requirements, a double ring-buffer
//       of pre-allocated objects should be used.

char* log_levels_text[] = {
        LOG_LEVEL_STR_UNKNOWN_TEXT,
        LOG_LEVEL_STR_DEBUG_INTERNALS_TEXT,
        LOG_LEVEL_STR_DEBUG_TEXT,
        LOG_LEVEL_STR_VERBOSE_TEXT,
        LOG_LEVEL_STR_INFO_TEXT,
        LOG_LEVEL_STR_WARNING_TEXT,
        LOG_LEVEL_STR_ERROR_TEXT
};

thread_local char* log_early_prefix_thread = NULL;

const char* log_level_to_string(log_level_t level) {
    switch(level) {
        case LOG_LEVEL_DEBUG_INTERNALS:
            return log_levels_text[LOG_LEVEL_STR_DEBUG_INTERNALS_INDEX];
        case LOG_LEVEL_DEBUG:
            return log_levels_text[LOG_LEVEL_STR_DEBUG_INDEX];
        case LOG_LEVEL_VERBOSE:
            return log_levels_text[LOG_LEVEL_STR_VERBOSE_INDEX];
        case LOG_LEVEL_INFO:
            return log_levels_text[LOG_LEVEL_STR_INFO_INDEX];
        case LOG_LEVEL_WARNING:
            return log_levels_text[LOG_LEVEL_STR_WARNING_INDEX];
        case LOG_LEVEL_ERROR:
            return log_levels_text[LOG_LEVEL_STR_ERROR_INDEX];
        default:
            return log_levels_text[LOG_LEVEL_STR_UNKNOWN_INDEX];
    }
}

void log_set_early_prefix_thread(
        char* prefix) {
    log_early_prefix_thread = prefix;
}

char* log_get_early_prefix_thread() {
    return log_early_prefix_thread;
}

void log_unset_early_prefix_thread() {
    log_early_prefix_thread = NULL;
}

time_t log_message_timestamp() {
    return time(NULL);
}

char* log_message_timestamp_str(
        time_t timestamp,
        char* dest,
        size_t maxlen) {
    assert(!(dest == NULL && maxlen > 0));

    struct tm tm = { 0 };
    gmtime_r(&timestamp, &tm);

    snprintf(dest, maxlen, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             1900 + tm.tm_year, tm.tm_mon, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    return dest;
}

void log_message_internal(
        const char *tag,
        log_level_t level,
        const char *message,
        va_list args) {
    char* message_with_args;
    size_t message_with_args_len = 0;
    char message_with_args_static_buffer[200] = { 0 };
    bool message_with_args_static_buffer_selected = false;
    time_t timestamp = log_message_timestamp();

    // Calculate message with args size
    va_list args_copy;
    va_copy(args_copy, args);
    message_with_args_len = vsnprintf(NULL, 0, message, args_copy);
    va_end(args_copy);

    // Decide if a static buffer can be used or a new one has to be allocated
    message_with_args = log_buffer_static_or_alloc_new(
            message_with_args_static_buffer,
            sizeof(message_with_args_static_buffer),
            message_with_args_len,
            &message_with_args_static_buffer_selected);

    // Build the message
    vsnprintf(message_with_args, message_with_args_len + 1, message, args);

    // Loop over the registered sinks and print the message
    log_sink_t** log_sink_registered = log_sink_registered_get();
    uint8_t log_sink_registered_count = log_sink_registered_count_get();
    for(
            uint8_t log_sink_registered_index = 0;
            log_sink_registered_index < log_sink_registered_count && log_sink_registered_index < LOG_SINK_REGISTERED_MAX;
            log_sink_registered_index++) {
        log_sink_t* log_sink = log_sink_registered[log_sink_registered_index];
        if ((level & log_sink->levels) != level) {
            continue;
        }

        log_sink->printer_fn(
                &log_sink->settings,
                tag,
                timestamp,
                level,
                log_early_prefix_thread,
                message_with_args,
                message_with_args_len);
    }

    if (!message_with_args_static_buffer_selected) {
        xalloc_free(message_with_args);
    }
}

void log_message(
        const char *tag,
        log_level_t level,
        const char* message,
        ...) {
    va_list args;
    va_start(args, message);

    log_message_internal(tag, level, message, args);

    va_end(args);
}

void log_message_print_os_error(
        const char *tag) {
    int error_code;
#if defined(__linux__)
    char buf[1024] = {0};
    char *error_message;
    error_code = errno;

    // If error code is OK skip
    if (error_code == 0) {
        return;
    }

    strerror_r(error_code, buf, sizeof(buf) - 1);
    error_message = buf;
#else
#error Platform not supported
#endif

    log_message(tag, LOG_LEVEL_ERROR, "OS Error: %s (%d)", error_message, error_code);

#if defined(__MINGW32__)
    LocalFree(error_message);
#endif
}

char* log_buffer_static_or_alloc_new(
        char* static_buffer,
        size_t static_buffer_size,
        size_t data_size,
        bool* static_buffer_selected) {
    char* buffer;

    // If the message is small enough, avoid allocating & freeing memory, uses < to keep 1 byte free for NULL
    // termination
    if (data_size < static_buffer_size - 1) {
        buffer = static_buffer;
        *static_buffer_selected = true;
    } else {
        buffer = xalloc_alloc(data_size + 1);
        *static_buffer_selected = false;
    }

    buffer[data_size] = 0;

    return buffer;
}
