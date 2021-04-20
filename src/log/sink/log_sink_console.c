#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <assert.h>

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
        va_list args) {
    char* log_message;
    char* log_message_beginning;
    char log_message_static_buffer[150] = { 0 };
    size_t color_fg_desired_len = 0;
    size_t color_fg_reset_len = 0;
    bool level_has_color = log_sink_console_log_level_color_fg_lookup[level] != NULL;
    FILE* out = level == LOG_LEVEL_ERROR && !settings->console.use_stdout_for_errors
            ? stderr
            : stdout;

    if (level_has_color) {
        color_fg_desired_len = strlen(log_sink_console_log_level_color_fg_lookup[level]);
        color_fg_reset_len = strlen(log_sink_console_log_level_color_fg_lookup[0]);
    }

    size_t log_message_size = log_sink_support_printer_str_len(
            tag,
            timestamp,
            level,
            early_prefix_thread,
            message,
            args);
    log_message_size += color_fg_desired_len + color_fg_reset_len;

    // If the message is small enough, avoid allocating & freeing memory
    if (log_message_size < sizeof(log_message_static_buffer)) {
        log_message = log_message_static_buffer;
    } else {
        // xalloc_alloc is slower than the slab_allocator but the console log sink should not be used in production for non
        // error logging, also we can't depend on the slab allocator as the slab allocator prints messages via the logging
        log_message = xalloc_alloc(log_message_size + 1);
    }

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
            args);

    // Restore the foreground color
    if (level_has_color) {
        strncpy(log_message, log_sink_console_log_level_color_fg_lookup[0], color_fg_reset_len);
        log_message += color_fg_reset_len;
    }

    assert(log_message == log_message_beginning + log_message_size);

    fwrite(log_message_beginning, log_message_size, 1, out);
    fflush(out);

    if (log_message_size >= sizeof(log_message_static_buffer)) {
        xalloc_free(log_message_beginning);
    }
}
