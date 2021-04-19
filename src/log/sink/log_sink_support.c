#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "log/log.h"

#include "log_sink_support.h"

size_t log_sink_support_printer_str_len(
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        va_list args) {
    size_t log_message_size;

    // Calculate how much memory is needed for the message to be printed
    va_list args_copy;
    va_copy(args_copy, args);
    log_message_size = log_sink_support_printer_str(
            NULL,
            0,
            tag,
            timestamp,
            level,
            early_prefix_thread,
            message,
            args_copy);
    va_end(args_copy);

    return log_message_size;
}

size_t log_sink_support_printer_str(
        char* message_out,
        size_t message_out_len,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        va_list args) {
    size_t message_out_len_res = 0;
    char timestamp_str[LOG_MESSAGE_TIMESTAMP_MAX_LENGTH] = {0};

    message_out_len_res = snprintf(
            message_out != NULL
                ? message_out + message_out_len_res
                : NULL,
            message_out_len,
            "[%s][%-11s]%s[%s] ",
            log_message_timestamp_str(timestamp, timestamp_str, LOG_MESSAGE_TIMESTAMP_MAX_LENGTH),
            log_level_to_string(level),
            early_prefix_thread != NULL ? early_prefix_thread : "",
            tag);
    message_out_len_res += vsnprintf(
            message_out != NULL
                ? message_out + message_out_len_res
                : NULL,
            message_out_len,
            message,
            args);
    message_out_len_res += snprintf(
            message_out != NULL
                ? message_out + message_out_len_res
                : NULL,
            message_out_len,
            "\n");

    return message_out_len_res;
}
