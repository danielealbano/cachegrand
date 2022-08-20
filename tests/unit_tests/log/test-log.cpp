/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>
#include <string.h>

#include "xalloc.h"

#include "log/log.h"
#include "log/sink/log_sink.h"

typedef struct test_log_sink_printer_request test_log_sink_printer_request_t;
struct test_log_sink_printer_request {
    void* settings;
    const char* tag;
    time_t timestamp;
    log_level_t level;
    char* early_prefix_thread;
    const char* message;
    size_t message_len;
};

typedef struct test_log_sink_printer_settings test_log_sink_printer_settings_t;
struct test_log_sink_printer_settings {
    test_log_sink_printer_request_t* request;
};

log_sink_t *test_log_sink = NULL;

void test_log_sink_printer(
        void* settings,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        size_t message_len) {
    char *message_clone = (char*)xalloc_alloc(strlen(message) + 1);
    strcpy(message_clone, message);

    test_log_sink_printer_request_t *request =
            ((test_log_sink_printer_settings_t*)settings)->request;

    request->settings = settings;
    request->tag = tag;
    request->timestamp = timestamp;
    request->level = level;
    request->early_prefix_thread = early_prefix_thread;
    request->message = message_clone;
    request->message_len = message_len;
}

void test_log_sink_register(
        log_sink_settings_t *settings,
        log_level_t log_level) {
    test_log_sink = log_sink_init(
            LOG_SINK_TYPE_CONSOLE,
            log_level,
            settings,
            (log_sink_printer_fn_t*)test_log_sink_printer,
            NULL);
    log_sink_register(test_log_sink);
}

void test_log_sink_unregister_all() {
    log_sink_registered_free();
    test_log_sink = NULL;
}

TEST_CASE("log/log.c", "[log]") {
    SECTION("LOG_LEVEL_ALL") {
        REQUIRE((LOG_LEVEL_ALL & LOG_LEVEL_DEBUG_INTERNALS) != 0);
        REQUIRE((LOG_LEVEL_ALL & LOG_LEVEL_DEBUG) != 0);
        REQUIRE((LOG_LEVEL_ALL & LOG_LEVEL_VERBOSE) != 0);
        REQUIRE((LOG_LEVEL_ALL & LOG_LEVEL_INFO) != 0);
        REQUIRE((LOG_LEVEL_ALL & LOG_LEVEL_WARNING) != 0);
        REQUIRE((LOG_LEVEL_ALL & LOG_LEVEL_ERROR) != 0);
    }

    SECTION("log_level_to_string") {
        SECTION("supported level") {
            REQUIRE(log_level_to_string(LOG_LEVEL_ERROR) ==
                    log_levels_text[LOG_LEVEL_STR_ERROR_INDEX]);
        }

        SECTION("unsupported level") {
            REQUIRE(log_level_to_string((log_level_t)0) ==
                    log_levels_text[LOG_LEVEL_STR_UNKNOWN_INDEX]);
        }

        SECTION("all levels text") {
            REQUIRE(log_level_to_string((log_level_t)0) ==
                    log_levels_text[LOG_LEVEL_STR_UNKNOWN_INDEX]);
            REQUIRE(log_level_to_string(LOG_LEVEL_DEBUG_INTERNALS) ==
                    log_levels_text[LOG_LEVEL_STR_DEBUG_INTERNALS_INDEX]);
            REQUIRE(log_level_to_string(LOG_LEVEL_DEBUG) ==
                    log_levels_text[LOG_LEVEL_STR_DEBUG_INDEX]);
            REQUIRE(log_level_to_string(LOG_LEVEL_VERBOSE) ==
                    log_levels_text[LOG_LEVEL_STR_VERBOSE_INDEX]);
            REQUIRE(log_level_to_string(LOG_LEVEL_INFO) ==
                    log_levels_text[LOG_LEVEL_STR_INFO_INDEX]);
            REQUIRE(log_level_to_string(LOG_LEVEL_WARNING) ==
                    log_levels_text[LOG_LEVEL_STR_WARNING_INDEX]);
            REQUIRE(log_level_to_string(LOG_LEVEL_ERROR) ==
                    log_levels_text[LOG_LEVEL_STR_ERROR_INDEX]);
        }
    }

    SECTION("log_message_timestamp_str") {
        char timestamp_dest[100] = { 0 };
        time_t timestamp = 10000;

        // Timestamp to compare
        struct tm tm_cmp = { 0 };
        gmtime_r(&timestamp, &tm_cmp);
        char timestamp_dest_cmp[100] = { 0 };
        snprintf(timestamp_dest_cmp, sizeof(timestamp_dest_cmp),
                 "%04d-%02d-%02dT%02d:%02d:%02dZ",
                 1900 + tm_cmp.tm_year, tm_cmp.tm_mon, tm_cmp.tm_mday,
                 tm_cmp.tm_hour, tm_cmp.tm_min, tm_cmp.tm_sec);

        SECTION("validate format") {
            REQUIRE(log_message_timestamp_str(
                    timestamp,
                    timestamp_dest,
                    sizeof(timestamp_dest)) == timestamp_dest);
            REQUIRE(strncmp(timestamp_dest, timestamp_dest_cmp, sizeof(timestamp_dest)) == 0);
        }

        SECTION("short string") {
            REQUIRE(log_message_timestamp_str(
                    timestamp,
                    timestamp_dest,
                    10) == timestamp_dest);

            REQUIRE(strncmp(timestamp_dest, timestamp_dest_cmp, 9) == 0);
            REQUIRE(timestamp_dest[10] == 0);
        }

        SECTION("null string with zero length") {
            REQUIRE(log_message_timestamp_str(
                    timestamp,
                    NULL,
                    0) == NULL);
        }
    }

    SECTION("log_buffer_static_or_alloc_new") {
        SECTION("static buffer") {
            char static_buffer[10] = { 0 };
            bool static_buffer_selected = false;
            size_t data_len = 5;

            REQUIRE(log_buffer_static_or_alloc_new(
                    static_buffer,
                    sizeof(static_buffer),
                    data_len,
                    &static_buffer_selected) == static_buffer);
            REQUIRE(static_buffer_selected == true);
            REQUIRE(static_buffer[5] == 0);
        }

        SECTION("dynamic buffer") {
            char static_buffer[10] = { 1 };
            bool static_buffer_selected = false;
            char *buffer = NULL;
            size_t data_len = 20;

            REQUIRE((buffer = log_buffer_static_or_alloc_new(
                    static_buffer,
                    sizeof(static_buffer),
                    data_len,
                    &static_buffer_selected)) != static_buffer);
            REQUIRE(static_buffer_selected == false);
            REQUIRE(*(buffer + data_len) == 0);

            xalloc_free(buffer);
        }
    }

    SECTION("log_get_early_prefix_thread") {
        REQUIRE(log_get_early_prefix_thread() == log_early_prefix_thread);
    }

    SECTION("log_set_early_prefix_thread") {
        char test_log_early_prefix_thread[] = "test prefix";
        log_set_early_prefix_thread(test_log_early_prefix_thread);

        REQUIRE(log_early_prefix_thread == test_log_early_prefix_thread);

        log_early_prefix_thread = NULL;
    }

    SECTION("log_unset_early_prefix_thread") {
        char test_log_early_prefix_thread[] = "test prefix";

        log_set_early_prefix_thread(test_log_early_prefix_thread);
        log_unset_early_prefix_thread();

        REQUIRE(log_early_prefix_thread == NULL);
    }

    SECTION("log_message") {
        char test_log_early_prefix_thread[] = "EARLY PREFIX THREAD";
        char test_tag[] = "A TAG";
        char test_message_cmp[] = "this is a test message";
        char test_message_format[] = "this is a test %s";
        char test_message_format_argument_1[] = "message";
        char test_message_cmp_very_long[] =
                "this is a test message - this is a test message - this is a test message - this is a test message - "
                "this is a test message - this is a test message - this is a test message - this is a test message - "
                "this is a test message - this is a test message - this is a test message - this is a test message - "
                "this is a test message - this is a test message - this is a test message - this is a test message - "
                "this is a test message - this is a test message";

        test_log_sink_printer_request_t test_log_sink_printer_request = { 0 };

        log_sink_settings_t log_sink_settings = { 0 };
        test_log_sink_printer_settings_t *test_log_sink_printer_settings =
                (test_log_sink_printer_settings_t*)&log_sink_settings;
        test_log_sink_printer_settings->request = &test_log_sink_printer_request;

        test_log_sink_register(
                &log_sink_settings,
                (log_level_t)(LOG_LEVEL_ALL - LOG_LEVEL_VERBOSE));

        SECTION("enabled level, no arguments, no early prefix thread") {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
            log_message(test_tag, LOG_LEVEL_INFO, test_message_cmp);
#pragma GCC diagnostic pop

            REQUIRE(strncmp(test_tag, test_log_sink_printer_request.tag, strlen(test_tag)) == 0);
            REQUIRE(strncmp(test_message_cmp, test_log_sink_printer_request.message, strlen(test_message_cmp)) == 0);
            REQUIRE(test_log_sink_printer_request.message_len == strlen(test_message_cmp));
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == LOG_LEVEL_INFO);
        }

        SECTION("enabled level, long message, no arguments, no early prefix thread") {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
            log_message(test_tag, LOG_LEVEL_INFO, test_message_cmp_very_long);
#pragma GCC diagnostic pop

            REQUIRE(strncmp(test_tag, test_log_sink_printer_request.tag, strlen(test_tag)) == 0);
            REQUIRE(strncmp(test_message_cmp_very_long, test_log_sink_printer_request.message, strlen(test_message_cmp_very_long)) == 0);
            REQUIRE(test_log_sink_printer_request.message_len == strlen(test_message_cmp_very_long));
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == LOG_LEVEL_INFO);
        }

        SECTION("disabled level, no arguments, no early prefix thread") {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
            log_message(test_tag, LOG_LEVEL_VERBOSE, test_message_cmp);
#pragma GCC diagnostic pop

            REQUIRE(test_log_sink_printer_request.tag == NULL);
            REQUIRE(test_log_sink_printer_request.message == NULL);
            REQUIRE(test_log_sink_printer_request.message_len == 0);
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == 0);
        }

        SECTION("enabled level, with arguments, no early prefix thread") {
            log_message(test_tag, LOG_LEVEL_INFO, test_message_format, test_message_format_argument_1);

            REQUIRE(strncmp(test_tag, test_log_sink_printer_request.tag, strlen(test_tag)) == 0);
            REQUIRE(strncmp(test_message_cmp, test_log_sink_printer_request.message, strlen(test_message_cmp)) == 0);
            REQUIRE(test_log_sink_printer_request.message_len == strlen(test_message_cmp));
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == LOG_LEVEL_INFO);
        }

        SECTION("disabled level, with arguments, no early prefix thread") {
            log_message(test_tag, LOG_LEVEL_VERBOSE, test_message_format, test_message_format_argument_1);

            REQUIRE(test_log_sink_printer_request.tag == NULL);
            REQUIRE(test_log_sink_printer_request.message == NULL);
            REQUIRE(test_log_sink_printer_request.message_len == 0);
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == 0);
        }

        SECTION("enabled level, with arguments, with early prefix thread") {
            log_early_prefix_thread = test_log_early_prefix_thread;
            log_message(test_tag, LOG_LEVEL_INFO, test_message_format, test_message_format_argument_1);
            log_early_prefix_thread = NULL;

            REQUIRE(strncmp(test_tag, test_log_sink_printer_request.tag, strlen(test_tag)) == 0);
            REQUIRE(strncmp(test_message_cmp, test_log_sink_printer_request.message, strlen(test_message_cmp)) == 0);
            REQUIRE(test_log_sink_printer_request.message_len == strlen(test_message_cmp));
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == test_log_early_prefix_thread);
            REQUIRE(test_log_sink_printer_request.level == LOG_LEVEL_INFO);
        }

        SECTION("disabled level, with arguments, with early prefix thread") {
            log_early_prefix_thread = test_log_early_prefix_thread;
            log_message(test_tag, LOG_LEVEL_VERBOSE, test_message_format, test_message_format_argument_1);
            log_early_prefix_thread = NULL;

            REQUIRE(test_log_sink_printer_request.tag == NULL);
            REQUIRE(test_log_sink_printer_request.message == NULL);
            REQUIRE(test_log_sink_printer_request.message_len == 0);
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == 0);
        }

        SECTION("two log sinks") {
            test_log_sink_printer_request_t test_log_sink_printer_request2 = { 0 };

            log_sink_settings_t log_sink_settings2 = { 0 };
            test_log_sink_printer_settings_t *test_log_sink_printer_settings2 =
                    (test_log_sink_printer_settings_t*)&log_sink_settings2;
            test_log_sink_printer_settings2->request = &test_log_sink_printer_request2;

            test_log_sink_register(
                    &log_sink_settings2,
                    (log_level_t)(LOG_LEVEL_ALL - LOG_LEVEL_VERBOSE));


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
            log_message(test_tag, LOG_LEVEL_INFO, test_message_cmp);
#pragma GCC diagnostic pop

            REQUIRE(strncmp(test_tag, test_log_sink_printer_request.tag, strlen(test_tag)) == 0);
            REQUIRE(strncmp(test_message_cmp, test_log_sink_printer_request.message, strlen(test_message_cmp)) == 0);
            REQUIRE(test_log_sink_printer_request.message_len == strlen(test_message_cmp));
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == LOG_LEVEL_INFO);

            REQUIRE(strncmp(test_tag, test_log_sink_printer_request2.tag, strlen(test_tag)) == 0);
            REQUIRE(strncmp(test_message_cmp, test_log_sink_printer_request2.message, strlen(test_message_cmp)) == 0);
            REQUIRE(test_log_sink_printer_request2.message_len == strlen(test_message_cmp));
            REQUIRE(test_log_sink_printer_request2.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request2.level == LOG_LEVEL_INFO);

            if (test_log_sink_printer_request2.message) {
                xalloc_free((void *)test_log_sink_printer_request2.message);
            }
        }

        test_log_sink_unregister_all();

        if (test_log_sink_printer_request.message) {
            xalloc_free((void *)test_log_sink_printer_request.message);
        }
    }

    SECTION("log_message_print_os_error") {
        char test_tag[] = "A TAG";

        test_log_sink_printer_request_t test_log_sink_printer_request = { 0 };

        log_sink_settings_t log_sink_settings = { 0 };
        test_log_sink_printer_settings_t *test_log_sink_printer_settings =
                (test_log_sink_printer_settings_t*)&log_sink_settings;
        test_log_sink_printer_settings->request = &test_log_sink_printer_request;

        test_log_sink_register(
                &log_sink_settings,
                (log_level_t)(LOG_LEVEL_ALL - LOG_LEVEL_VERBOSE));

        SECTION("error code 0") {
            errno = 0;
            log_message_print_os_error(test_tag);

            REQUIRE(test_log_sink_printer_request.tag == NULL);
            REQUIRE(test_log_sink_printer_request.message == NULL);
            REQUIRE(test_log_sink_printer_request.message_len == 0);
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == 0);
        }

        SECTION("error code 1 (Operation not permitted)") {
            char test_message_cmp[] = "OS Error: Operation not permitted (1)";
            errno = 1;
            log_message_print_os_error(test_tag);

            REQUIRE(test_log_sink_printer_request.tag == test_tag);
            REQUIRE(strncmp(test_message_cmp, test_log_sink_printer_request.message, strlen(test_message_cmp)) == 0);
            REQUIRE(test_log_sink_printer_request.message_len == strlen(test_message_cmp));
            REQUIRE(test_log_sink_printer_request.early_prefix_thread == NULL);
            REQUIRE(test_log_sink_printer_request.level == LOG_LEVEL_ERROR);
        }

        test_log_sink_unregister_all();

        if (test_log_sink_printer_request.message) {
            xalloc_free((void *)test_log_sink_printer_request.message);
        }
    }
}
