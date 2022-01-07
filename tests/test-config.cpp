#include <catch2/catch.hpp>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <cyaml/cyaml.h>

#include "xalloc.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_support.h"

#include "config.h"
#include "config_cyaml_config.h"
#include "config_cyaml_schema.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

typedef struct test_config_cyaml_logger_context test_config_cyaml_logger_context_t;
struct test_config_cyaml_logger_context {
    char* data;
    size_t data_length;
};

char test_config_internal_log_sink_printer_data[200] =  { 0 };

void test_config_cyaml_logger(
        cyaml_log_t level_cyaml,
        void *ctx_raw,
        const char *fmt,
        va_list args) {
    test_config_cyaml_logger_context_t* ctx = (test_config_cyaml_logger_context_t*)ctx_raw;

    // Calculate how much memory is needed
    va_list args_copy;
    va_copy(args_copy, args);
    size_t log_message_size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    size_t new_data_length = ctx->data_length + log_message_size;

    if (ctx->data == NULL) {
        ctx->data = (char*)xalloc_alloc(new_data_length + 1);
    } else {
        ctx->data = (char*)xalloc_realloc(ctx->data, new_data_length + 1);
    }

    vsnprintf(ctx->data + ctx->data_length, log_message_size + 1, fmt, args);

    ctx->data_length = new_data_length;
    ctx->data[ctx->data_length] = 0;
}

void test_config_internal_cyaml_log_wrapper(
        cyaml_log_t level_cyaml,
        void *ctx,
        const char *fmt,
        ...) {
    va_list args;
    va_start(args, fmt);
    config_internal_cyaml_log(level_cyaml, ctx, fmt, args);
    va_end(args);
}

void test_config_internal_log_sink_printer(
        log_sink_settings_t* settings,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        size_t message_len) {

    size_t log_message_size = log_sink_support_printer_str_len(
            tag,
            early_prefix_thread,
            message_len);

    memset(test_config_internal_log_sink_printer_data, 0, sizeof(test_config_internal_log_sink_printer_data));

    log_sink_support_printer_str(
            test_config_internal_log_sink_printer_data,
            log_message_size,
            tag,
            timestamp,
            level,
            early_prefix_thread,
            message,
            message_len);
}

log_sink_t *test_config_internal_log_sink_init(
        log_level_t levels,
        log_sink_settings_t* settings) {
    return log_sink_init(
            LOG_SINK_TYPE_CONSOLE,
            levels,
            settings,
            test_config_internal_log_sink_printer,
            NULL);
}

bool test_config_fixture_file_from_data_create(
        char* path,
        int path_suffix_len,
        const char* data,
        size_t data_len) {

    close(mkstemps(path, path_suffix_len));

    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        return false;
    }

    size_t res;
    if ((res = fwrite(data, 1, data_len, fp)) != data_len) {
        fclose(fp);
        unlink(path);
        return false;
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        unlink(path);
        return false;
    }

    fclose(fp);

    return true;
}

void test_config_fixture_file_from_data_cleanup(
        const char* path) {
    unlink(path);
}

#define TEST_CONFIG_FIXTURE_FILE_FROM_DATA(DATA, DATA_LEN, CONFIG_PATH, ...) { \
    { \
        char CONFIG_PATH[] = "/tmp/cachegrand-tests-XXXXXX.tmp"; \
        int CONFIG_PATH_suffix_len = 4; /** .tmp **/ \
        REQUIRE(test_config_fixture_file_from_data_create(CONFIG_PATH, CONFIG_PATH_suffix_len, DATA, DATA_LEN)); \
        __VA_ARGS__; \
        test_config_fixture_file_from_data_cleanup(CONFIG_PATH); \
    } \
}

std::string test_config_correct_all_fields_yaml_data =
        R"EOF(
# Set cpu pinning, accepted values:
# - cpu number, starting from 0 (ie. 0, 1, 2)
# - cpu range, starting from 0 (ie. 2-12)
# - all, will use all the available cpus
cpus:
  - all

# Suggested 2 thread per cpu, depending on the hardware up to 4 threads will still scale well
# The number of workers that will be started will be n. cpus * threads_per_count
workers_per_cpus: 2

run_in_foreground: false

pidfile_path: /var/run/cachegrand.pid

# The slab allocator requires huge pages
use_slab_allocator: false

network:
  backend: io_uring
  max_clients: 10000
  listen_backlog: 100

  protocols:
    - type: redis
      timeout:
        read: 2000
        write: 2000
        inactivity: 2000
      keepalive:
        time: 0
        interval: 0
        probes: 0
      redis:
        max_key_length: 8192
        max_command_length: 1048576
      bindings:
        - host: 0.0.0.0
          port: 6379
        - host: "::"
          port: 6379

storage:
  backend: memory
  max_shard_size_mb: 0

database:
  max_keys: 10000

# The sentry.io service is used to automatically collect minidumps in case of crashes, it doesn't
# store them after that they are processed but be aware that minidumps will contain memory regions
# used by cachegrand and therefore may they may also contain cached data!
# To avoid accidental/unwanted uploads, by default the dsn is not set to avoid accidental/unwanted
# uploads, if you want to provide crash data without having to attach it to a github issue simply
# set the dsn parameter, or the SENTRY_DSN env variable, to
# https://05dd54814d8149cab65ba2987d560340@o590814.ingest.sentry.io/5740234
sentry:
  enable: true
  # dsn: "https://05dd54814d8149cab65ba2987d560340@o590814.ingest.sentry.io/5740234"

# LOGS
# ---
# type:     console or file
# level:    array of flags: debug, verbose, info, warning, recoverable, error
#           it's possible to negate a flag prefixing it with no- (ie. no-debug), it is also
#           possible to select all the flags using the keyword all
#
# For type file
# file:
#   path:   path to the log file

logs:
  - type: console
    # Print to the console errors, warnings and informational messages
    level: [ all, no-verbose, no-debug]

    # or print everything
    # level: [ all ]

  - type: file
    level: [ all, no-verbose, no-debug ]
    file:
      path: /var/log/cachegrand.log
)EOF";

std::string test_config_broken_missing_field_yaml_data =
        R"EOF(
network:
  backend: io_uring
)EOF";

std::string test_config_broken_unknown_field_yaml_data =
        R"EOF(
unknown_field: unknown_value
)EOF";

uint16_t max_cpu_count_high = 8;
uint16_t max_cpu_count_low = 4;
uint16_t max_cpu_count = max_cpu_count_high;
uint16_t* cpus_map = NULL;
uint16_t cpus_map_count;

#define TEST_CONFIG_CPUS_COUNT_MAX 4

char* test_config_cpus_1_cpu[] = { "2" };
unsigned test_config_cpus_1_cpu_count = 1;

char* test_config_cpus_2_cpus[] = { "3", "4" };
unsigned test_config_cpus_2_cpus_count = 2;

char* test_config_cpus_1_cpu_repeated[] = { "2", "2", "2", "2" };
unsigned test_config_cpus_1_cpu_repeated_count = 4;

char* test_config_cpus_1_cpu_range[] = { "2-6" };
unsigned test_config_cpus_1_cpu_range_count = 1;

char* test_config_cpus_2_cpu_ranges[] = { "2-3", "6-8" };
unsigned test_config_cpus_2_cpu_ranges_count = 2;

char* test_config_cpus_mixed[] = { "2-3", "1", "6-8", "0" };
unsigned test_config_cpus_mixed_count = 4;

char* test_config_cpus_all[] = { "all" };
unsigned test_config_cpus_all_count = 1;

char* test_config_cpus_all_before_other[] = { "all", "1", "2-3" };
unsigned test_config_cpus_all_before_other_count = 3;

char* test_config_cpus_all_after_other[] = { "1", "2-3", "all" };
unsigned test_config_cpus_all_after_other_count = 3;

char* test_config_cpus_1_cpu_over[] = { "9" };
unsigned test_config_cpus_cpu_over_count = 1;

char* test_config_cpus_1_cpu_range_start_over[] = { "9-10" };
unsigned test_config_cpus_1_cpu_range_start_over_count = 1;

char* test_config_cpus_1_cpu_range_end_over[] = { "6-10" };
unsigned test_config_cpus_1_cpu_range_end_over_count = 1;

char* test_config_cpus_1_cpu_range_too_small[] = { "1-1" };
unsigned test_config_cpus_1_cpu_range_too_small_count = 1;

char* test_config_cpus_1_cpu_range_multiple[] = { "2-3-4" };
unsigned test_config_cpus_1_cpu_range_multiple_count = 1;

char* test_config_cpus_1_cpu_with_comma[] = { "1,2" };
unsigned test_config_cpus_1_cpu_with_comma_count = 1;

char* test_config_cpus_1_cpu_with_dot[] = { "1.2" };
unsigned test_config_cpus_1_cpu_with_dot_count = 1;

TEST_CASE("config.c", "[config]") {
    cyaml_err_t err = CYAML_OK;
    config_t* config = NULL;

    // Initialize the schema and the cyaml config
    cyaml_schema_value_t* config_top_schema = (cyaml_schema_value_t*)config_cyaml_schema_get_top_schema();
    cyaml_config_t * config_cyaml_config = config_cyaml_config_get_global();

    // Initialize the internal test logger context
    test_config_cyaml_logger_context_t cyaml_logger_context = { 0 };
    config_cyaml_config->log_level = CYAML_LOG_WARNING;
    config_cyaml_config->log_fn = test_config_cyaml_logger;
    config_cyaml_config->log_ctx = (void*)&cyaml_logger_context;

    SECTION("validate config schema") {
        SECTION("correct - all fields") {
            err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    NULL);

            REQUIRE(config != NULL);
            REQUIRE(config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING);
            REQUIRE(config->network->protocols_count == 1);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->use_slab_allocator != NULL);
            REQUIRE(*config->use_slab_allocator == false);
            REQUIRE(config->logs_count == 2);
            REQUIRE(cyaml_logger_context.data == NULL);
            REQUIRE(cyaml_logger_context.data_length == 0);
            REQUIRE(err == CYAML_OK);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - missing field") {
            const char* str_cmp =
                "Load: Missing required mapping field: max_clients\nLoad: Backtrace:\n  in mapping field 'backend' (line: 3, column: 12)\n  in mapping field 'network' (line: 3, column: 3)\n";
            err = cyaml_load_data(
                    (const uint8_t *)(test_config_broken_missing_field_yaml_data.c_str()),
                    test_config_broken_missing_field_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    NULL);

            REQUIRE(config == NULL);
            REQUIRE(strcmp(str_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(str_cmp));
            REQUIRE(err == CYAML_ERR_MAPPING_FIELD_MISSING);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - unknown field") {
            const char* str_cmp =
                    "Load: Unexpected key: unknown_field\nLoad: Backtrace:\n  in mapping (line: 2, column: 1)\n";

            err = cyaml_load_data(
                    (const uint8_t *)(test_config_broken_unknown_field_yaml_data.c_str()),
                    test_config_broken_unknown_field_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    NULL);

            REQUIRE(config == NULL);
            REQUIRE(strcmp(str_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(str_cmp));
            REQUIRE(err == CYAML_ERR_INVALID_KEY);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_internal_cyaml_load") {
        SECTION("correct - all fields") {
            TEST_CONFIG_FIXTURE_FILE_FROM_DATA(
                    test_config_correct_all_fields_yaml_data.c_str(),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_path,
                    {
                        err = config_internal_cyaml_load(
                                &config,
                                config_path,
                                config_cyaml_config,
                                config_top_schema);
                    });

            REQUIRE(config != NULL);
            REQUIRE(config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING);
            REQUIRE(config->network->protocols_count == 1);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->logs_count == 2);
            REQUIRE(cyaml_logger_context.data == NULL);
            REQUIRE(cyaml_logger_context.data_length == 0);
            REQUIRE(err == CYAML_OK);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - missing field") {
            const char* str_cmp =
                    "Load: Missing required mapping field: max_clients\nLoad: Backtrace:\n  in mapping field 'backend' (line: 3, column: 12)\n  in mapping field 'network' (line: 3, column: 3)\n";

            TEST_CONFIG_FIXTURE_FILE_FROM_DATA(
                    test_config_broken_missing_field_yaml_data.c_str(),
                    test_config_broken_missing_field_yaml_data.length(),
                    config_path,
                    {
                        err = config_internal_cyaml_load(
                                &config,
                                config_path,
                                config_cyaml_config,
                                config_top_schema);
                    });

            REQUIRE(config == NULL);
            REQUIRE(strcmp(str_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(str_cmp));
            REQUIRE(err == CYAML_ERR_MAPPING_FIELD_MISSING);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - unknown field") {
            const char* str_cmp =
                    "Load: Unexpected key: unknown_field\nLoad: Backtrace:\n  in mapping (line: 2, column: 1)\n";

            TEST_CONFIG_FIXTURE_FILE_FROM_DATA(
                    test_config_broken_unknown_field_yaml_data.c_str(),
                    test_config_broken_unknown_field_yaml_data.length(),
                    config_path,
                    {
                        err = config_internal_cyaml_load(
                                &config,
                                config_path,
                                config_cyaml_config,
                                config_top_schema);
                    });

            REQUIRE(config == NULL);
            REQUIRE(strcmp(str_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(str_cmp));
            REQUIRE(err == CYAML_ERR_INVALID_KEY);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_load") {
        SECTION("correct - all fields") {
            TEST_CONFIG_FIXTURE_FILE_FROM_DATA(
                    test_config_correct_all_fields_yaml_data.c_str(),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_path,
                    {
                        config = config_load(config_path);
                    });

            REQUIRE(config != NULL);
            REQUIRE(config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING);
            REQUIRE(config->network->protocols_count == 1);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->logs_count == 2);
            REQUIRE(cyaml_logger_context.data == NULL);
            REQUIRE(cyaml_logger_context.data_length == 0);
            REQUIRE(err == CYAML_OK);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - missing field") {
            TEST_CONFIG_FIXTURE_FILE_FROM_DATA(
                    test_config_broken_missing_field_yaml_data.c_str(),
                    test_config_broken_missing_field_yaml_data.length(),
                    config_path,
                    {
                        config = config_load(config_path);
                    });

            REQUIRE(config == NULL);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_free") {
        TEST_CONFIG_FIXTURE_FILE_FROM_DATA(
                test_config_correct_all_fields_yaml_data.c_str(),
                test_config_correct_all_fields_yaml_data.length(),
                config_path,
                {
                    err = config_internal_cyaml_load(
                            &config,
                            config_path,
                            config_cyaml_config,
                            config_top_schema);
                });

        config_free(config);
    }

    SECTION("config_cpus_parse") {
        SECTION("1 cpu") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu,
                    test_config_cpus_1_cpu_count,
                    &cpus_map,
                    &cpus_map_count) == true);
            REQUIRE(cpus_map_count == 1);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 2);

            xalloc_free(cpus_map);
        }

        SECTION("2 cpus") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_2_cpus,
                    test_config_cpus_2_cpus_count,
                    &cpus_map,
                    &cpus_map_count) == true);

            REQUIRE(cpus_map_count == 2);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 3);
            REQUIRE(cpus_map[1] == 4);

            xalloc_free(cpus_map);
        }

        SECTION("1 cpu repeated") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_repeated,
                    test_config_cpus_1_cpu_repeated_count,
                    &cpus_map,
                    &cpus_map_count) == true);

            REQUIRE(cpus_map_count == 4);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 2);
            REQUIRE(cpus_map[1] == 2);
            REQUIRE(cpus_map[2] == 2);
            REQUIRE(cpus_map[3] == 2);

            xalloc_free(cpus_map);
        }

        SECTION("single cpus range") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range,
                    test_config_cpus_1_cpu_range_count,
                    &cpus_map,
                    &cpus_map_count) == true);

            REQUIRE(cpus_map_count == 5);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 2);
            REQUIRE(cpus_map[1] == 3);
            REQUIRE(cpus_map[2] == 4);
            REQUIRE(cpus_map[3] == 5);
            REQUIRE(cpus_map[4] == 6);

            xalloc_free(cpus_map);
        }

        SECTION("two cpus range") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_2_cpu_ranges,
                    test_config_cpus_2_cpu_ranges_count,
                    &cpus_map,
                    &cpus_map_count) == true);

            REQUIRE(cpus_map_count == 2 + 3);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 2);
            REQUIRE(cpus_map[1] == 3);
            REQUIRE(cpus_map[2] == 6);
            REQUIRE(cpus_map[3] == 7);
            REQUIRE(cpus_map[4] == 8);

            xalloc_free(cpus_map);
        }

        SECTION("mixed cpus") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_mixed,
                    test_config_cpus_mixed_count,
                    &cpus_map,
                    &cpus_map_count) == true);

            REQUIRE(cpus_map_count == 2 + 1 + 3 + 1);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 2);
            REQUIRE(cpus_map[1] == 3);
            REQUIRE(cpus_map[2] == 1);
            REQUIRE(cpus_map[3] == 6);
            REQUIRE(cpus_map[4] == 7);
            REQUIRE(cpus_map[5] == 8);
            REQUIRE(cpus_map[6] == 0);

            xalloc_free(cpus_map);
        }

        SECTION("all cpus") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count_low,
                    test_config_cpus_all,
                    test_config_cpus_all_count,
                    &cpus_map,
                    &cpus_map_count) == true);

            REQUIRE(cpus_map_count == max_cpu_count_low);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 0);
            REQUIRE(cpus_map[1] == 1);
            REQUIRE(cpus_map[2] == 2);
            REQUIRE(cpus_map[3] == 3);

            xalloc_free(cpus_map);
        }

        SECTION("all cpus - before other") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count_low,
                    test_config_cpus_all_before_other,
                    test_config_cpus_all_before_other_count,
                    &cpus_map,
                    &cpus_map_count) == true);

            REQUIRE(cpus_map_count == max_cpu_count_low);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 0);
            REQUIRE(cpus_map[1] == 1);
            REQUIRE(cpus_map[2] == 2);
            REQUIRE(cpus_map[3] == 3);

            xalloc_free(cpus_map);
        }

        SECTION("all cpus - after other") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count_low,
                    test_config_cpus_all_after_other,
                    test_config_cpus_all_after_other_count,
                    &cpus_map,
                    &cpus_map_count) == true);

            REQUIRE(cpus_map_count == max_cpu_count_low);
            REQUIRE(cpus_map != NULL);
            REQUIRE(cpus_map[0] == 0);
            REQUIRE(cpus_map[1] == 1);
            REQUIRE(cpus_map[2] == 2);
            REQUIRE(cpus_map[3] == 3);

            xalloc_free(cpus_map);
        }

        SECTION("1 cpu over max") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_over,
                    test_config_cpus_cpu_over_count,
                    &cpus_map,
                    &cpus_map_count) == false);
        }

        SECTION("1 cpu range start over max") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range_start_over,
                    test_config_cpus_1_cpu_range_start_over_count,
                    &cpus_map,
                    &cpus_map_count) == false);
        }

        SECTION("1 cpu range end over max") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range_end_over,
                    test_config_cpus_1_cpu_range_end_over_count,
                    &cpus_map,
                    &cpus_map_count) == false);
        }

        SECTION("1 cpu range too small") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range_too_small,
                    test_config_cpus_1_cpu_range_too_small_count,
                    &cpus_map,
                    &cpus_map_count) == false);
        }

        SECTION("1 cpu range multiple") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range_multiple,
                    test_config_cpus_1_cpu_range_multiple_count,
                    &cpus_map,
                    &cpus_map_count) == false);
        }

        SECTION("1 cpu with comma") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_with_comma,
                    test_config_cpus_1_cpu_with_comma_count,
                    &cpus_map,
                    &cpus_map_count) == false);
        }

        SECTION("1 cpu with comma") {
            REQUIRE(config_cpus_parse(
                    max_cpu_count,
                    test_config_cpus_1_cpu_with_dot,
                    test_config_cpus_1_cpu_with_dot_count,
                    &cpus_map,
                    &cpus_map_count) == false);
        }
    }

    SECTION("config_cpus_validate") {
        config_cpus_validate_error_t errors[TEST_CONFIG_CPUS_COUNT_MAX] = { CONFIG_CPUS_VALIDATE_OK };

        SECTION("1 cpu") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu,
                    test_config_cpus_1_cpu_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("2 cpus") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_2_cpus,
                    test_config_cpus_2_cpus_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[1] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("1 cpu repeated") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_repeated,
                    test_config_cpus_1_cpu_repeated_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[1] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[2] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[3] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("single cpus range") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range,
                    test_config_cpus_1_cpu_range_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("two cpus range") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_2_cpu_ranges,
                    test_config_cpus_2_cpu_ranges_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[1] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("mixed cpus") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_mixed,
                    test_config_cpus_mixed_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[1] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[2] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[3] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("all cpus") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count_low,
                    test_config_cpus_all,
                    test_config_cpus_all_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("all cpus - before other") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count_low,
                    test_config_cpus_all_before_other,
                    test_config_cpus_all_before_other_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[1] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[2] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("all cpus - after other") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count_low,
                    test_config_cpus_all_after_other,
                    test_config_cpus_all_after_other_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[1] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[2] == CONFIG_CPUS_VALIDATE_OK);
        }

        SECTION("1 cpu over max") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_over,
                    test_config_cpus_cpu_over_count,
                    errors) == false);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_ERROR_INVALID_CPU);
        }

        SECTION("1 cpu range start over max") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range_start_over,
                    test_config_cpus_1_cpu_range_start_over_count,
                    errors) == false);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_ERROR_INVALID_CPU);
        }

        SECTION("1 cpu range end over max") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range_end_over,
                    test_config_cpus_1_cpu_range_end_over_count,
                    errors) == false);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_ERROR_INVALID_CPU);
        }

        SECTION("1 cpu range too small") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range_too_small,
                    test_config_cpus_1_cpu_range_too_small_count,
                    errors) == false);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_ERROR_RANGE_TOO_SMALL);
        }

        SECTION("1 cpu range multiple") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_range_multiple,
                    test_config_cpus_1_cpu_range_multiple_count,
                    errors) == false);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_ERROR_MULTIPLE_RANGES);
        }

        SECTION("1 cpu with comma") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_with_comma,
                    test_config_cpus_1_cpu_with_comma_count,
                    errors) == false);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_ERROR_UNEXPECTED_CHARACTER);
        }

        SECTION("1 cpu with comma") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count,
                    test_config_cpus_1_cpu_with_dot,
                    test_config_cpus_1_cpu_with_dot_count,
                    errors) == false);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_ERROR_UNEXPECTED_CHARACTER);
        }
    }

    SECTION("config_cpus_filter_duplicates") {
        uint16_t* unique_cpus_duplicates = NULL;
        uint16_t unique_cpus_duplicates_count = 0;

        SECTION("list with duplicates") {
            uint16_t cpus_duplicates[] = { 2, 2, 3, 3, 1, 4, 2 };
            uint16_t cpus_duplicates_count = sizeof(cpus_duplicates) / sizeof(uint16_t);

            config_cpus_filter_duplicates(
                    cpus_duplicates,
                    cpus_duplicates_count,
                    &unique_cpus_duplicates,
                    &unique_cpus_duplicates_count);

            REQUIRE(unique_cpus_duplicates != NULL);
            REQUIRE(unique_cpus_duplicates_count == 4);
            REQUIRE(unique_cpus_duplicates[0] == 2);
            REQUIRE(unique_cpus_duplicates[1] == 3);
            REQUIRE(unique_cpus_duplicates[2] == 1);
            REQUIRE(unique_cpus_duplicates[3] == 4);

            xalloc_free(unique_cpus_duplicates);
        }

        SECTION("list without duplicates") {
            uint16_t cpus_duplicates[] = { 2, 3, 1, 4 };
            uint16_t cpus_duplicates_count = sizeof(cpus_duplicates) / sizeof(uint16_t);

            config_cpus_filter_duplicates(
                    cpus_duplicates,
                    cpus_duplicates_count,
                    &unique_cpus_duplicates,
                    &unique_cpus_duplicates_count);

            REQUIRE(unique_cpus_duplicates != NULL);
            REQUIRE(unique_cpus_duplicates_count == 4);
            REQUIRE(unique_cpus_duplicates[0] == 2);
            REQUIRE(unique_cpus_duplicates[1] == 3);
            REQUIRE(unique_cpus_duplicates[2] == 1);
            REQUIRE(unique_cpus_duplicates[3] == 4);

            xalloc_free(unique_cpus_duplicates);
        }

        SECTION("empty list") {
            uint16_t cpus_duplicates[] = { };
            uint16_t cpus_duplicates_count = 0;

            config_cpus_filter_duplicates(
                    cpus_duplicates,
                    cpus_duplicates_count,
                    &unique_cpus_duplicates,
                    &unique_cpus_duplicates_count);

            REQUIRE(unique_cpus_duplicates == NULL);
            REQUIRE(unique_cpus_duplicates_count == 0);
        }
    }

    SECTION("config_internal_cyaml_log") {
        log_level_t level = (log_level_t)LOG_LEVEL_ALL;
        log_sink_settings_t settings = { 0 };
        log_sink_register(test_config_internal_log_sink_init(level, &settings));

        SECTION("CYAML_LOG_DEBUG") {
            char* str_cmp = "[DEBUG      ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_DEBUG, NULL, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_NOTICE") {
            char* str_cmp = "[WARNING    ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_NOTICE, NULL, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_WARNING") {
            char* str_cmp = "[WARNING    ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_WARNING, NULL, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_ERROR") {
            char* str_cmp = "[ERROR      ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_ERROR, NULL, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_INFO") {
            char* str_cmp = "[INFO       ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_INFO, NULL, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        log_sink_registered_free();
    }

    SECTION("config_log_level_t == log_level_t") {
        REQUIRE((int)CONFIG_LOG_LEVEL_ERROR == LOG_LEVEL_ERROR);
        REQUIRE((int)CONFIG_LOG_LEVEL_WARNING == LOG_LEVEL_WARNING);
        REQUIRE((int)CONFIG_LOG_LEVEL_INFO == LOG_LEVEL_INFO);
        REQUIRE((int)CONFIG_LOG_LEVEL_VERBOSE == LOG_LEVEL_VERBOSE);
        REQUIRE((int)CONFIG_LOG_LEVEL_DEBUG == LOG_LEVEL_DEBUG);
        REQUIRE((int)CONFIG_LOG_LEVEL_MAX == LOG_LEVEL_MAX);
    }

    SECTION("config_log_type_t == log_sink_type_t") {
        REQUIRE((int)CONFIG_LOG_TYPE_CONSOLE == LOG_SINK_TYPE_CONSOLE);
        REQUIRE((int)CONFIG_LOG_TYPE_FILE == LOG_SINK_TYPE_FILE);
        REQUIRE((int)CONFIG_LOG_TYPE_MAX == LOG_SINK_TYPE_MAX);
    }

    if (cyaml_logger_context.data != NULL) {
        xalloc_free(cyaml_logger_context.data);
    }
}
