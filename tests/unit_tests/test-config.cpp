/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <cyaml/cyaml.h>

#include "xalloc.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_support.h"
#include "support/simple_file_io.h"

#include "config.h"
#include "config_cyaml_config.h"
#include "config_cyaml_schema.h"

#include "support.h"

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
    auto ctx = (test_config_cyaml_logger_context_t*)ctx_raw;

    // Calculate how much memory is needed
    va_list args_copy;
    va_copy(args_copy, args);
    size_t log_message_size = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    size_t new_data_length = ctx->data_length + log_message_size;

    if (ctx->data == nullptr) {
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
            nullptr);
}

std::string test_config_correct_all_fields_yaml_data =
        R"EOF(
cpus:
  - all
workers_per_cpus: 2
run_in_foreground: false
pidfile_path: /var/run/cachegrand.pid
use_huge_pages: false
network:
  backend: io_uring
  max_clients: 10000
  listen_backlog: 100
modules:
  - type: redis
    redis:
      max_key_length: 8192
      max_command_length: 1048576
      max_command_arguments: 10000
      strict_parsing: false
    network:
      timeout:
        read_ms: 2000
        write_ms: 2000
      keepalive:
        time: 0
        interval: 0
        probes: 0
      tls:
        certificate_path: "/path/to/certificate.pem"
        private_key_path: "/path/to/certificate.key"
        min_version: tls1.2
        max_version: any
        cipher_suites:
          - TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384
      bindings:
        - host: 0.0.0.0
          port: 6379
        - host: "::"
          port: 6379
          tls: true
database:
  limits:
    hard:
      max_keys: 1000000
    soft:
      max_keys: 999999
  backend: memory
  memory:
    limits:
      hard:
        max_memory_usage: 1000000
      soft:
        max_memory_usage: 999999
  keys_eviction:
    policy: lru
    only_ttl: true
sentry:
  enable: true
logs:
  - type: console
    level: [ all, no-verbose, no-debug ]
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

std::string test_config_broken_config_validate_after_load_fails =
        R"EOF(
cpus:
  - all
workers_per_cpus: 2
run_in_foreground: false
pidfile_path: /var/run/cachegrand.pid
use_huge_pages: false
network:
  backend: io_uring
  max_clients: 10000
  listen_backlog: 100
modules:
  - type: redis
    redis:
      max_key_length: 8192
      max_command_length: 1048576
      max_command_arguments: 10000
      strict_parsing: false
    network:
      timeout:
        read_ms: 2000
        write_ms: 2000
      keepalive:
        time: 0
        interval: 0
        probes: 0
      tls:
        certificate_path: "/path/to/non-existent/certificate"
        private_key_path: "/path/to/non-existent/private_key"
      bindings:
        - host: 0.0.0.0
          port: 6379
        - host: "::"
          port: 6379
database:
  limits:
    hard:
      max_keys: 1000000
    soft:
      max_keys: 999999
  backend: memory
  memory:
    limits:
      hard:
        max_memory_usage: 1000000
      soft:
        max_memory_usage: 999999
  keys_eviction:
    policy: lru
    only_ttl: true
sentry:
  enable: true
logs:
  - type: console
    level: [ all, no-verbose, no-debug ]
  - type: file
    level: [ all, no-verbose, no-debug ]
    file:
      path: /var/log/cachegrand.log
)EOF";

uint16_t max_cpu_count_high = 8;
uint16_t max_cpu_count_low = 4;
uint16_t max_cpu_count = max_cpu_count_high;
uint16_t* cpus_map = nullptr;
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
    config_t* config = nullptr;

    // Initialize the schema and the cyaml config
    auto config_top_schema = (cyaml_schema_value_t*)config_cyaml_schema_get_top_schema();
    cyaml_config_t * config_cyaml_config = config_cyaml_config_get_global();

    // Initialize the internal test logger context
    test_config_cyaml_logger_context_t cyaml_logger_context = { nullptr };
    config_cyaml_config->log_level = CYAML_LOG_WARNING;
    config_cyaml_config->log_fn = test_config_cyaml_logger;
    config_cyaml_config->log_ctx = (void*)&cyaml_logger_context;

    SECTION("config_parse_string_absolute_or_percent") {
        int64_t return_value;
        config_parse_string_absolute_or_percent_return_value_t return_value_type;

        SECTION("absolute") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123",
                    3,
                    false,
                    false,
                    false,
                    true,
                    false,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == 123);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("absolute with spaces") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)" 123 ",
                    5,
                    false,
                    false,
                    false,
                    true,
                    false,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == 123);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("percent") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"99%",
                    4,
                    false,
                    false,
                    true,
                    false,
                    false,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == 99);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_PERCENT);
        }

        SECTION("percent with space") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)" 99 % ",
                    6,
                    false,
                    false,
                    true,
                    false,
                    false,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == 99);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_PERCENT);
        }

        SECTION("absolute with bytes suffix") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123B",
                    4,
                    false,
                    false,
                    false,
                    true,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == 123);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("absolute with kb suffix") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123KB",
                    5,
                    false,
                    false,
                    false,
                    true,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == 123 * 1024);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("absolute with mb suffix") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123MB",
                    5,
                    false,
                    false,
                    false,
                    true,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == 123 * 1024 * 1024);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("absolute with gb suffix") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123GB",
                    5,
                    false,
                    false,
                    false,
                    true,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == (int64_t)123 * (int64_t)1024 * (int64_t)1024 * (int64_t)1024);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("absolute with tb suffix") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123TB",
                    5,
                    false,
                    false,
                    false,
                    true,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == (int64_t)123 * (int64_t)1024 * (int64_t)1024 * (int64_t)1024 * (int64_t)1024);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("absolute negative") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"-123",
                    4,
                    true,
                    false,
                    false,
                    true,
                    false,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == -123);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("absolute zero") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"0",
                    1,
                    false,
                    true,
                    false,
                    true,
                    false,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == true);
            REQUIRE(return_value == 0);
            REQUIRE(return_value_type == CONFIG_PARSE_STRING_ABSOLUTE_OR_PERCENT_RETURN_VALUE_ABSOLUTE);
        }

        SECTION("zero not allowed") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"0",
                    1,
                    true,
                    false,
                    true,
                    true,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == false);
        }

        SECTION("negative not allowed") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"-123",
                    4,
                    false,
                    true,
                    true,
                    true,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == false);
        }

        SECTION("absolute not allowed") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123",
                    3,
                    false,
                    false,
                    true,
                    false,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == false);
        }

        SECTION("percent not allowed") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123%",
                    4,
                    false,
                    false,
                    true,
                    false,
                    true,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == false);
        }

        SECTION("sizes not allowed") {
            bool result = config_parse_string_absolute_or_percent(
                    (char*)"123MB",
                    5,
                    false,
                    false,
                    true,
                    true,
                    false,
                    &return_value,
                    &return_value_type);
            REQUIRE(result == false);
        }
    }

    SECTION("validate config schema") {
        SECTION("correct - all fields") {
            err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING);
            REQUIRE(config->modules_count == 1);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->use_huge_pages != nullptr);
            REQUIRE(*config->use_huge_pages == false);
            REQUIRE(config->logs_count == 2);
            REQUIRE(cyaml_logger_context.data == nullptr);
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
                    nullptr);

            REQUIRE(config == nullptr);
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
                    nullptr);

            REQUIRE(config == nullptr);
            REQUIRE(strcmp(str_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(str_cmp));
            REQUIRE(err == CYAML_ERR_INVALID_KEY);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - config_validate_after_load fails") {
            err = cyaml_load_data(
                    (const uint8_t *)(test_config_broken_config_validate_after_load_fails.c_str()),
                    test_config_broken_config_validate_after_load_fails.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(config_validate_after_load(config) == false);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_cpus") {
        SECTION("valid") {
             err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_cpus(config));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_database_backend_file") {
        SECTION("valid") {
            err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_database_backend_file(config));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_database_backend_memory") {
        SECTION("valid") {
            int64_t return_value;
            config_parse_string_absolute_or_percent_return_value_t return_value_type;

            err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            // The check on the soft / hard limits requires the soft limit being less than the hard limit.
            config->database->memory->limits->hard->max_memory_usage = 1;
            REQUIRE(config_validate_after_load_database_backend_memory(config));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_database_memory_control") {
        SECTION("valid") {
             err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_database_keys_eviction(config));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_modules_network_timeout") {
        SECTION("valid") {
             err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_modules_network_timeout(&config->modules[0]));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_modules_network_bindings") {
        SECTION("valid") {
             err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_modules_network_bindings(&config->modules[0]));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_modules_network_tls") {
        SECTION("valid") {
            // Create empty temporary files
            int fixture_temp_path_suffix_len = 4;
            char certificate_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            char private_key_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            close(mkstemps(certificate_temp_path, fixture_temp_path_suffix_len));
            close(mkstemps(private_key_temp_path, fixture_temp_path_suffix_len));

            // Replace the old paths with the new ones
            std::string test_config_correct_all_fields_yaml_data_with_temp_paths = test_config_correct_all_fields_yaml_data;
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.pem"),
                    strlen("/path/to/certificate.pem"),
                    certificate_temp_path);
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.key"),
                    strlen("/path/to/certificate.key"),
                    private_key_temp_path);

            // Load the new config
            err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data_with_temp_paths.c_str()),
                    test_config_correct_all_fields_yaml_data_with_temp_paths.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_modules_network_tls(&config->modules[0]));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);

            // Remove the temporary files
            unlink(certificate_temp_path);
            unlink(private_key_temp_path);
        }
    }

    SECTION("config_validate_after_load_modules_redis") {
        SECTION("valid") {
             err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_modules_redis(&config->modules[0]));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_modules") {
        SECTION("valid") {
            // Create empty temporary files
            int fixture_temp_path_suffix_len = 4;
            char certificate_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            char private_key_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            close(mkstemps(certificate_temp_path, fixture_temp_path_suffix_len));
            close(mkstemps(private_key_temp_path, fixture_temp_path_suffix_len));

            // Replace the old paths with the new ones
            std::string test_config_correct_all_fields_yaml_data_with_temp_paths = test_config_correct_all_fields_yaml_data;
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.pem"),
                    strlen("/path/to/certificate.pem"),
                    certificate_temp_path);
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.key"),
                    strlen("/path/to/certificate.key"),
                    private_key_temp_path);

            // Load the new config
            err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data_with_temp_paths.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_modules(config));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);

            // Remove the temporary files
            unlink(certificate_temp_path);
            unlink(private_key_temp_path);
        }
    }

    SECTION("config_validate_after_load_log_file") {
        SECTION("valid") {
             err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_log_file(&config->logs[1]));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_validate_after_load_logs") {
        SECTION("valid") {
            // Create empty temporary files
            int fixture_temp_path_suffix_len = 4;
            char certificate_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            char private_key_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            close(mkstemps(certificate_temp_path, fixture_temp_path_suffix_len));
            close(mkstemps(private_key_temp_path, fixture_temp_path_suffix_len));

            // Replace the old paths with the new ones
            std::string test_config_correct_all_fields_yaml_data_with_temp_paths = test_config_correct_all_fields_yaml_data;
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.pem"),
                    strlen("/path/to/certificate.pem"),
                    certificate_temp_path);
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.key"),
                    strlen("/path/to/certificate.key"),
                    private_key_temp_path);

            // Load the new config
            err = cyaml_load_data(
                    (const uint8_t *)(test_config_correct_all_fields_yaml_data_with_temp_paths.c_str()),
                    test_config_correct_all_fields_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    nullptr);

            REQUIRE(config != nullptr);
            REQUIRE(err == CYAML_OK);

            REQUIRE(config_validate_after_load_logs(config));

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);

            // Remove the temporary files
            unlink(certificate_temp_path);
            unlink(private_key_temp_path);
        }
    }

    SECTION("config_validate_after_load") {
        // Create empty temporary files
        int fixture_temp_path_suffix_len = 4;
        char certificate_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        char private_key_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
        close(mkstemps(certificate_temp_path, fixture_temp_path_suffix_len));
        close(mkstemps(private_key_temp_path, fixture_temp_path_suffix_len));

        // Replace the old paths with the new ones
        std::string test_config_correct_all_fields_yaml_data_with_temp_paths = test_config_correct_all_fields_yaml_data;
        test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.pem"),
                strlen("/path/to/certificate.pem"),
                certificate_temp_path);
        test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.key"),
                strlen("/path/to/certificate.key"),
                private_key_temp_path);

        // Load the new config
        err = cyaml_load_data(
                (const uint8_t *)(test_config_correct_all_fields_yaml_data_with_temp_paths.c_str()),
                test_config_correct_all_fields_yaml_data_with_temp_paths.length(),
                config_cyaml_config,
                config_top_schema,
                (cyaml_data_t **)&config,
                nullptr);

        REQUIRE(config != nullptr);
        REQUIRE(err == CYAML_OK);

        // The check on the soft / hard limits requires the soft limit being less than the hard limit.
        config->database->memory->limits->hard->max_memory_usage = 1;

        SECTION("valid") {
            REQUIRE(config_validate_after_load(config) == true);
        }

        SECTION("broken - network redis max_key_length > object size max") {
            config->modules[0].redis->max_key_length = 64 * 1024 + 1;
            REQUIRE(config_validate_after_load(config) == false);
        }

        SECTION("broken - network.timeout.read_ms < -1") {
            config->modules[0].network->timeout->read_ms = -2;
            REQUIRE(config_validate_after_load(config) == false);
        }

        SECTION("broken - network.timeout.read_ms == 0") {
            config->modules[0].network->timeout->read_ms = 0;
            REQUIRE(config_validate_after_load(config) == false);
        }

        SECTION("broken - network.timeout.write_ms < -1") {
            config->modules[0].network->timeout->write_ms = -2;
            REQUIRE(config_validate_after_load(config) == false);
        }

        SECTION("broken - network.timeout.write_ms == 0") {
            config->modules[0].network->timeout->write_ms = 0;
            REQUIRE(config_validate_after_load(config) == false);
        }

        SECTION("broken - non existing certificate path") {
            config_module_network_tls_t tls = {
                    .certificate_path = "/path/to/non/existing/certificate",
                    .private_key_path = "/tmp",
            };
            config->modules[0].network->tls = &tls;

            REQUIRE(config_validate_after_load(config) == false);

            config->modules[0].network->tls = nullptr;
        }

        SECTION("broken - non existing certificate path") {
            config_module_network_tls_t tls = {
                    .certificate_path = "/tmp",
                    .private_key_path = "/path/to/non/existing/private_key",
            };
            config->modules[0].network->tls = &tls;

            REQUIRE(config_validate_after_load(config) == false);

            config->modules[0].network->tls = nullptr;
        }

        SECTION("valid - existing certificate path and private_key") {
            config_module_network_tls_t tls = {
                    .certificate_path = "/tmp",
                    .private_key_path = "/tmp",
            };
            config->modules[0].network->tls = &tls;

            REQUIRE(config_validate_after_load(config) == true);

            config->modules[0].network->tls = nullptr;
        }

        SECTION("broken - tls endpoint without no tls settings") {
            config_module_network_tls_t* tls_temp = config->modules[0].network->tls;
            config->modules[0].network->tls = nullptr;
            config->modules[0].network->bindings[0].tls = true;

            REQUIRE(config_validate_after_load(config) == false);

            config->modules[0].network->tls = tls_temp;
        }

        SECTION("valid - tls endpoint with tls settings") {
            config_module_network_tls_t tls = {
                    .certificate_path = "/tmp",
                    .private_key_path = "/tmp",
            };
            config->modules[0].network->tls = &tls;
            config->modules[0].network->bindings[0].tls = true;

            REQUIRE(config_validate_after_load(config) == true);

            config->modules[0].network->tls = nullptr;
        }

        cyaml_free(config_cyaml_config, config_top_schema, config, 0);

        // Remove the temporary files
        unlink(certificate_temp_path);
        unlink(private_key_temp_path);
    }

    SECTION("config_internal_cyaml_load") {
        SECTION("correct - all fields") {
            // Create empty temporary files
            int fixture_temp_path_suffix_len = 4;
            char certificate_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            char private_key_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            close(mkstemps(certificate_temp_path, fixture_temp_path_suffix_len));
            close(mkstemps(private_key_temp_path, fixture_temp_path_suffix_len));

            // Replace the old paths with the new ones
            std::string test_config_correct_all_fields_yaml_data_with_temp_paths = test_config_correct_all_fields_yaml_data;
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.pem"),
                    strlen("/path/to/certificate.pem"),
                    certificate_temp_path);
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.key"),
                    strlen("/path/to/certificate.key"),
                    private_key_temp_path);

            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.c_str(),
                    test_config_correct_all_fields_yaml_data_with_temp_paths.length(),
                    config_path,
                    {
                        err = config_internal_cyaml_load(
                                &config,
                                config_path,
                                config_cyaml_config,
                                config_top_schema);
                    })

            // Remove the temporary files
            unlink(certificate_temp_path);
            unlink(private_key_temp_path);

            REQUIRE(config != nullptr);
            REQUIRE(config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING);
            REQUIRE(config->modules_count == 1);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->logs_count == 2);
            REQUIRE(cyaml_logger_context.data == nullptr);
            REQUIRE(cyaml_logger_context.data_length == 0);
            REQUIRE(err == CYAML_OK);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);

            // Remove the temporary files
            unlink(certificate_temp_path);
            unlink(private_key_temp_path);
        }

        SECTION("broken - missing field") {
            const char* str_cmp =
                    "Load: Missing required mapping field: max_clients\nLoad: Backtrace:\n  in mapping field 'backend' (line: 3, column: 12)\n  in mapping field 'network' (line: 3, column: 3)\n";

            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    test_config_broken_missing_field_yaml_data.c_str(),
                    test_config_broken_missing_field_yaml_data.length(),
                    config_path,
                    {
                        err = config_internal_cyaml_load(
                                &config,
                                config_path,
                                config_cyaml_config,
                                config_top_schema);
                    })

            REQUIRE(config == nullptr);
            REQUIRE(strcmp(str_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(str_cmp));
            REQUIRE(err == CYAML_ERR_MAPPING_FIELD_MISSING);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - unknown field") {
            const char* str_cmp =
                    "Load: Unexpected key: unknown_field\nLoad: Backtrace:\n  in mapping (line: 2, column: 1)\n";

            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    test_config_broken_unknown_field_yaml_data.c_str(),
                    test_config_broken_unknown_field_yaml_data.length(),
                    config_path,
                    {
                        err = config_internal_cyaml_load(
                                &config,
                                config_path,
                                config_cyaml_config,
                                config_top_schema);
                    })

            REQUIRE(config == nullptr);
            REQUIRE(strcmp(str_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(str_cmp));
            REQUIRE(err == CYAML_ERR_INVALID_KEY);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_load") {
        SECTION("correct - all fields") {
            // Create empty temporary files
            int fixture_temp_path_suffix_len = 4;
            char certificate_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            char private_key_temp_path[] = "/tmp/cachegrand-tests-XXXXXX.tmp";
            close(mkstemps(certificate_temp_path, fixture_temp_path_suffix_len));
            close(mkstemps(private_key_temp_path, fixture_temp_path_suffix_len));

            // Replace the old paths with the new ones
            std::string test_config_correct_all_fields_yaml_data_with_temp_paths = test_config_correct_all_fields_yaml_data;
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.pem"),
                    strlen("/path/to/certificate.pem"),
                    certificate_temp_path);
            test_config_correct_all_fields_yaml_data_with_temp_paths.replace(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.find("/path/to/certificate.key"),
                    strlen("/path/to/certificate.key"),
                    private_key_temp_path);

            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    test_config_correct_all_fields_yaml_data_with_temp_paths.c_str(),
                    test_config_correct_all_fields_yaml_data_with_temp_paths.length(),
                    config_path,
                    {
                        config = config_load(config_path);
                    })

            REQUIRE(config != nullptr);
            REQUIRE(config->network->backend == CONFIG_NETWORK_BACKEND_IO_URING);
            REQUIRE(config->modules_count == 1);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->logs_count == 2);
            REQUIRE(cyaml_logger_context.data == nullptr);
            REQUIRE(cyaml_logger_context.data_length == 0);
            REQUIRE(err == CYAML_OK);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - missing field") {
            TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                    test_config_broken_missing_field_yaml_data.c_str(),
                    test_config_broken_missing_field_yaml_data.length(),
                    config_path,
                    {
                        config = config_load(config_path);
                    })

            REQUIRE(config == nullptr);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }
    }

    SECTION("config_free") {
        TEST_SUPPORT_FIXTURE_FILE_FROM_DATA(
                test_config_correct_all_fields_yaml_data.c_str(),
                test_config_correct_all_fields_yaml_data.length(),
                config_path,
                {
                    err = config_internal_cyaml_load(
                            &config,
                            config_path,
                            config_cyaml_config,
                            config_top_schema);
                })

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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(cpus_map != nullptr);
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
            REQUIRE(errors[1] == CONFIG_CPUS_VALIDATE_ERROR_NO_MULTI_CPUS_WITH_ALL);
            REQUIRE(errors[2] == CONFIG_CPUS_VALIDATE_ERROR_NO_MULTI_CPUS_WITH_ALL);
        }

        SECTION("all cpus - after other") {
            REQUIRE(config_cpus_validate(
                    max_cpu_count_low,
                    test_config_cpus_all_after_other,
                    test_config_cpus_all_after_other_count,
                    errors) == true);

            REQUIRE(errors[0] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[1] == CONFIG_CPUS_VALIDATE_OK);
            REQUIRE(errors[2] == CONFIG_CPUS_VALIDATE_ERROR_NO_MULTI_CPUS_WITH_ALL);
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
        uint16_t* unique_cpus_duplicates = nullptr;
        uint16_t unique_cpus_duplicates_count = 0;

        SECTION("list with duplicates") {
            uint16_t cpus_duplicates[] = { 2, 2, 3, 3, 1, 4, 2 };
            uint16_t cpus_duplicates_count = sizeof(cpus_duplicates) / sizeof(uint16_t);

            config_cpus_filter_duplicates(
                    cpus_duplicates,
                    cpus_duplicates_count,
                    &unique_cpus_duplicates,
                    &unique_cpus_duplicates_count);

            REQUIRE(unique_cpus_duplicates != nullptr);
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

            REQUIRE(unique_cpus_duplicates != nullptr);
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

            REQUIRE(unique_cpus_duplicates == nullptr);
            REQUIRE(unique_cpus_duplicates_count == 0);
        }
    }

    SECTION("config_internal_cyaml_log") {
        auto level = (log_level_t)LOG_LEVEL_ALL;
        log_sink_settings_t settings = { false };
        log_sink_register(test_config_internal_log_sink_init(level, &settings));

        SECTION("CYAML_LOG_DEBUG") {
            char* str_cmp = "[DEBUG      ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_DEBUG, nullptr, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_NOTICE") {
            char* str_cmp = "[WARNING    ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_NOTICE, nullptr, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_WARNING") {
            char* str_cmp = "[WARNING    ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_WARNING, nullptr, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_ERROR") {
            char* str_cmp = "[ERROR      ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_ERROR, nullptr, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_INFO") {
            char* str_cmp = "[INFO       ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_INFO, nullptr, "test log message: %s", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_INFO - new line at end") {
            char* str_cmp = "[INFO       ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_INFO, nullptr, "test log message: %s\n", "test argument");
            REQUIRE(strcmp(str_cmp, test_config_internal_log_sink_printer_data + 22) == 0);
        }

        SECTION("CYAML_LOG_INFO - multiple new lines at end") {
            char* str_cmp = "[INFO       ][config] test log message: test argument\n";
            test_config_internal_cyaml_log_wrapper(CYAML_LOG_INFO, nullptr, "test log message: %s\r\n\r\n", "test argument");
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

    SECTION("ensure etc/cachegrand.yaml.skel is valid") {
        ssize_t tests_executable_path_len;
        char tests_executable_path[256] = { 0 };
        char config_file_path_rel[] = "../../../etc/cachegrand.yaml.skel";

        // Build the path to the config file dinamically
        REQUIRE((tests_executable_path_len = readlink(
                "/proc/self/exe", tests_executable_path, sizeof(tests_executable_path))) > 0);
        strncpy(
                strrchr(tests_executable_path, '/') + 1,
                config_file_path_rel,
                strlen(config_file_path_rel));

        err = cyaml_load_file(
                tests_executable_path,
                config_cyaml_config,
                config_top_schema,
                (cyaml_data_t **)&config,
                nullptr);

        REQUIRE(config != nullptr);
        REQUIRE(config_validate_after_load(config) == true);

        cyaml_free(config_cyaml_config, config_top_schema, config, 0);
    }

    if (cyaml_logger_context.data != nullptr) {
        xalloc_free(cyaml_logger_context.data);
    }
}
