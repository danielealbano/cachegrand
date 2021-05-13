/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "log/log.h"

#include "log_sink_support.h"

size_t log_sink_support_printer_str_len(
        const char* tag,
        const char* early_prefix_thread,
        size_t message_len) {
    size_t log_message_len;

    log_message_len =
            1 + LOG_MESSAGE_TIMESTAMP_MAX_LENGTH + 1 + // timestamp
            1 + 11 + 1 +  // log level padded with up to 11 spaces
            (early_prefix_thread != NULL ? strlen(early_prefix_thread) : 0) + // early_prefix_thread
            1 + strlen(tag) + 1 + // tag with brackets
            1 +  // space
            message_len + // message
            1; // \n


    return log_message_len;
}

size_t log_sink_support_printer_str(
        char* message_out,
        size_t message_out_len,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        const char* early_prefix_thread,
        const char* message,
        size_t message_len) {
    size_t message_out_len_res = 0;
    char timestamp_str[LOG_MESSAGE_TIMESTAMP_MAX_LENGTH + 1] = {0};

    message_out_len_res = snprintf(
            message_out,
            message_out_len,
            "[%s][%-11s]%s[%s] ",
            log_message_timestamp_str(timestamp, timestamp_str, sizeof(timestamp_str)),
            log_level_to_string(level),
            early_prefix_thread != NULL ? early_prefix_thread : "",
            tag);

    strcpy(message_out + message_out_len_res, message);
    message_out_len_res += message_len;

    message_out[message_out_len_res] = '\n';
    message_out_len_res++;

    return message_out_len_res;
}
