#include <stdint.h>
#include <string.h>
#include <time.h>

#include "log/log.h"
#include "log/sink/log_sink_support.h"
#include "xalloc.h"

#include "log_sink_console.h"

static char** log_sink_console_log_level_color_fg_lookup = NULL;
void log_sink_console_log_level_color_fg_lookup_init() {
    log_sink_console_log_level_color_fg_lookup = xalloc_alloc_zero((LOG_LEVEL_MAX - 1) * sizeof(char*));

    // red
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_ERROR] = "\x1B[31m";

    // yellow
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_WARNING] = "\x1B[33m";

    // light gray
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_VERBOSE] = "\x1B[37m";

    // dark gray
    log_sink_console_log_level_color_fg_lookup[LOG_LEVEL_DEBUG] = "\x1B[90m";

    // Use 0 to reset the foreground color status, not used by logging
    log_sink_console_log_level_color_fg_lookup[0] = "\x1B[0m";
}

log_sink_t *log_sink_console_init(
        log_level_t levels,
        log_sink_settings_t* log_sink_settings) {
    if (!log_sink_console_log_level_color_fg_lookup) {
        log_sink_console_log_level_color_fg_lookup_init();
    }

    return log_sink_init(
            LOG_SINK_TYPE_CONSOLE,
            levels,
            log_sink_settings,
            log_sink_console_printer,
            NULL);
}

void log_sink_console_printer(
        log_sink_settings_t* log_sink_settings,
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
    FILE* out = level == LOG_LEVEL_ERROR && !log_sink_settings->console.use_stdout_for_errors
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
        // error logging, also we can't depened on the slab allocator as the slab allocator prints messages via the logging
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

    fwrite(log_message_beginning, log_message_size, 1, out);

    if (log_message_size >= sizeof(log_message_static_buffer)) {
        xalloc_free(log_message_beginning);
    }
}
