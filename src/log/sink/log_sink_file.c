#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_support.h"
#include "xalloc.h"

#include "log_sink_file.h"

#define TAG "log_sink_file"

log_sink_t *log_sink_file_init(
        log_level_t levels,
        log_sink_settings_t* settings) {

    FILE* fp = fopen(settings->file.log_path, "a");
    if (fp == NULL) {
        LOG_E(TAG, "Unable to open %s for logging", settings->file.log_path);
        return NULL;
    }

    settings->file.internal.fp = fp;

    return log_sink_init(
            LOG_SINK_TYPE_FILE,
            levels,
            settings,
            log_sink_file_printer,
            log_sink_file_free);
}

void log_sink_file_free(
        log_sink_settings_t* settings) {
    if (settings->file.internal.fp) {
        fclose(settings->file.internal.fp);
        settings->file.internal.fp = NULL;
    }
}

void log_sink_file_printer(
        log_sink_settings_t* log_sink_settings,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        va_list args) {
    char* log_message;
    char log_message_static_buffer[150] = { 0 };

    size_t log_message_size = log_sink_support_printer_str_len(
            tag,
            timestamp,
            level,
            early_prefix_thread,
            message,
            args);

    // If the message is small enough, avoid allocating & freeing memory
    if (log_message_size < sizeof(log_message_static_buffer)) {
        log_message = log_message_static_buffer;
    } else {
        // xalloc_alloc is slower than the slab_allocator but the file log sink should not be used in production for non
        // error logging, also we can't depend on the slab allocator as the slab allocator prints messages via the logging
        log_message = xalloc_alloc(log_message_size + 1);
    }

    // Write the log message
    log_sink_support_printer_str(
            log_message,
            log_message_size,
            tag,
            timestamp,
            level,
            early_prefix_thread,
            message,
            args);

    fwrite(log_message, log_message_size, 1, log_sink_settings->file.internal.fp);

    if (log_message_size >= sizeof(log_message_static_buffer)) {
        xalloc_free(log_message);
    }
}
