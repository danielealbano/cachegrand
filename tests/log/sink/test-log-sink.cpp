#include <catch2/catch.hpp>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xalloc.h"

#include "log/log.h"
#include "log/sink/log_sink.h"

bool test_log_sink_free_invoked = false;

void test_log_sink_printer(
        log_sink_settings_t *settings,
        const char *tag,
        time_t timestamp,
        log_level_t level,
        char *early_prefix_thread,
        const char *message,
        size_t message_len) {
    // do nothing
}

void test_log_sink_free(
        log_sink_settings_t *settings) {
    test_log_sink_free_invoked = true;
}

TEST_CASE("log/sink/log_sink.c", "[log][sink][log_sink]") {
    SECTION("log_sink_init") {
        log_sink_settings_t settings = {
                .console = {
                        .use_stdout_for_errors = true,
                }
        };

        SECTION("valid sink") {
            log_sink_t *sink = log_sink_init(
                    LOG_SINK_TYPE_CONSOLE,
                    LOG_LEVEL_INFO,
                    &settings,
                    test_log_sink_printer,
                    NULL);

            REQUIRE(sink != NULL);
            REQUIRE(sink->settings.console.use_stdout_for_errors == settings.console.use_stdout_for_errors);
            REQUIRE(sink->type == LOG_SINK_TYPE_CONSOLE);
            REQUIRE(sink->levels == LOG_LEVEL_INFO);
            REQUIRE(sink->printer_fn == test_log_sink_printer);
            REQUIRE(sink->free_fn == NULL);

            xalloc_free(sink);
        }

        SECTION("valid sink - empty settings") {
            log_sink_t *sink = log_sink_init(
                    LOG_SINK_TYPE_CONSOLE,
                    LOG_LEVEL_INFO,
                    NULL,
                    test_log_sink_printer,
                    NULL);

            REQUIRE(sink != NULL);
            REQUIRE(sink->settings.file.path == NULL);
            REQUIRE(sink->type == LOG_SINK_TYPE_CONSOLE);
            REQUIRE(sink->levels == LOG_LEVEL_INFO);
            REQUIRE(sink->printer_fn == test_log_sink_printer);
            REQUIRE(sink->free_fn == NULL);

            xalloc_free(sink);
        }

        SECTION("null printer_fn") {
            log_sink_t *sink = log_sink_init(
                    LOG_SINK_TYPE_CONSOLE,
                    LOG_LEVEL_INFO,
                    &settings,
                    NULL,
                    NULL);

            REQUIRE(sink == NULL);
        }
    }

    SECTION("log_sink_free") {
        test_log_sink_free_invoked = false;
        log_sink_settings_t settings = {
                .console = {
                        .use_stdout_for_errors = true,
                }
        };

        SECTION("without free_fn") {
            log_sink_t *sink = log_sink_init(
                    LOG_SINK_TYPE_CONSOLE,
                    LOG_LEVEL_INFO,
                    &settings,
                    test_log_sink_printer,
                    NULL);

            log_sink_free(sink);

            REQUIRE(test_log_sink_free_invoked == false);
        }

        SECTION("with free_fn") {
            log_sink_t *sink = log_sink_init(
                    LOG_SINK_TYPE_CONSOLE,
                    LOG_LEVEL_INFO,
                    &settings,
                    test_log_sink_printer,
                    test_log_sink_free);

            log_sink_free(sink);

            REQUIRE(test_log_sink_free_invoked == true);
        }
    }

    SECTION("log_sink_register") {
        log_sink_settings_t settings = {
                .console = {
                        .use_stdout_for_errors = true,
                }
        };

        log_sink_t *sink1 = log_sink_init(
                LOG_SINK_TYPE_CONSOLE,
                LOG_LEVEL_INFO,
                &settings,
                test_log_sink_printer,
                NULL);

        log_sink_t *sink2 = log_sink_init(
                LOG_SINK_TYPE_CONSOLE,
                LOG_LEVEL_INFO,
                &settings,
                test_log_sink_printer,
                NULL);

        SECTION("no sinks") {
            REQUIRE(log_sink_registered == NULL);
        }

        SECTION("valid sink") {
            REQUIRE(log_sink_register(sink1) == true);
            REQUIRE(log_sink_registered[0] == sink1);
            REQUIRE(log_sink_registered_count == 1);
        }

        SECTION("null sink") {
            REQUIRE(log_sink_register(NULL) == false);
            REQUIRE(log_sink_registered_count == 0);
        }

        SECTION("multiple sinks") {
            REQUIRE(log_sink_register(sink1) == true);
            REQUIRE(log_sink_register(sink2) == true);

            REQUIRE(log_sink_registered[0] == sink1);
            REQUIRE(log_sink_registered[1] == sink2);
            REQUIRE(log_sink_registered_count == 2);
        }

        log_sink_free(sink1);
        log_sink_free(sink2);

        if (log_sink_registered != NULL) {
            xalloc_free(log_sink_registered);
            log_sink_registered = NULL;
            log_sink_registered_count = 0;
        }
    }

    SECTION("log_sink_registered_get") {
        log_sink_settings_t settings = {
                .console = {
                        .use_stdout_for_errors = true,
                }
        };

        log_sink_t *sink1 = log_sink_init(
                LOG_SINK_TYPE_CONSOLE,
                LOG_LEVEL_INFO,
                &settings,
                test_log_sink_printer,
                NULL);

        REQUIRE(log_sink_register(sink1) == true);
        REQUIRE(log_sink_registered_get() == log_sink_registered);

        log_sink_free(sink1);

        if (log_sink_registered != NULL) {
            xalloc_free(log_sink_registered);
            log_sink_registered = NULL;
            log_sink_registered_count = 0;
        }
    }

    SECTION("log_sink_registered_count_get") {
        log_sink_settings_t settings = {
                .console = {
                        .use_stdout_for_errors = true,
                }
        };

        log_sink_t *sink1 = log_sink_init(
                LOG_SINK_TYPE_CONSOLE,
                LOG_LEVEL_INFO,
                &settings,
                test_log_sink_printer,
                NULL);

        REQUIRE(log_sink_register(sink1) == true);
        REQUIRE(log_sink_registered_count_get() == log_sink_registered_count);

        log_sink_free(sink1);

        if (log_sink_registered != NULL) {
            xalloc_free(log_sink_registered);
            log_sink_registered = NULL;
            log_sink_registered_count = 0;
        }
    }

    SECTION("log_sink_registered_free") {
        log_sink_settings_t settings = {
                .console = {
                        .use_stdout_for_errors = true,
                }
        };

        log_sink_t *sink1 = log_sink_init(
                LOG_SINK_TYPE_CONSOLE,
                LOG_LEVEL_INFO,
                &settings,
                test_log_sink_printer,
                NULL);

        log_sink_t *sink2 = log_sink_init(
                LOG_SINK_TYPE_CONSOLE,
                LOG_LEVEL_INFO,
                &settings,
                test_log_sink_printer,
                NULL);

        REQUIRE(log_sink_register(sink1) == true);
        REQUIRE(log_sink_register(sink2) == true);

        log_sink_registered_free();

        REQUIRE(log_sink_registered == NULL);
        REQUIRE(log_sink_registered_count == 0);
    }

    SECTION("log_sink_factory") {
        SECTION("console sink") {
            log_sink_settings_t settings = {
                    .console = {
                            .use_stdout_for_errors = true,
                    }
            };

            log_sink_t *sink = log_sink_factory(
                    LOG_SINK_TYPE_CONSOLE,
                    LOG_LEVEL_INFO,
                    &settings);

            REQUIRE(sink != NULL);
            REQUIRE(sink->type == LOG_SINK_TYPE_CONSOLE);
            REQUIRE(sink->levels == LOG_LEVEL_INFO);
            REQUIRE(sink->settings.console.use_stdout_for_errors == settings.console.use_stdout_for_errors);

            log_sink_free(sink);
        }

        SECTION("file sink") {
            char fd_link_buf[100] = { 0 };
            char fd_link_resolved_buf[100] = { 0 };
            char path[] = "/tmp/cachegrand-tests-XXXXXX.log";
            int path_suffix_len = 4;

            REQUIRE(mkstemps(path, path_suffix_len));

            log_sink_settings_t settings = {
                    .file = {
                            .path = path,
                    }
            };

            log_sink_t *sink = log_sink_factory(
                    LOG_SINK_TYPE_FILE,
                    LOG_LEVEL_INFO,
                    &settings);

            REQUIRE(sink != NULL);
            REQUIRE(sink->type == LOG_SINK_TYPE_FILE);
            REQUIRE(sink->levels == LOG_LEVEL_INFO);
            REQUIRE(sink->settings.file.path == settings.file.path);
            REQUIRE(sink->settings.file.internal.fp != NULL);

            int fd = fileno(sink->settings.file.internal.fp);

            snprintf(fd_link_buf, sizeof(fd_link_buf), "/proc/self/fd/%d", fd);
            REQUIRE(readlink(fd_link_buf, fd_link_resolved_buf, sizeof(fd_link_resolved_buf)) > 0);
            REQUIRE(strncmp(fd_link_resolved_buf, path, sizeof(path)) == 0);

            log_sink_free(sink);
        }

        SECTION("unsupported sink") {
            log_sink_settings_t settings = { 0 };

            log_sink_t *sink = log_sink_factory(
                    (log_sink_type_t)-1,
                    LOG_LEVEL_INFO,
                    &settings);

            REQUIRE(sink == NULL);
        }
    }
}