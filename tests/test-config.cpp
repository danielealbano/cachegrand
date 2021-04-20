#include <catch2/catch.hpp>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <cyaml/cyaml.h>

#include "xalloc.h"
#include "log/log.h"
#include "log/sink/log_sink.h"

#include "config.h"
#include "config_cyaml_config.h"
#include "config_cyaml_schema.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

typedef struct test_config_cyaml_logger_context test_config_cyaml_logger_context_t;
struct test_config_cyaml_logger_context {
    char* data;
    size_t data_length;
};

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

bool test_config_fixture_file_from_data_create(
        char* path,
        int path_suffix_len,
        const char* data,
        size_t data_len) {

    if (!mkstemps(path, path_suffix_len)) {
        return false;
    }

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
worker_type: io_uring
cpus:
  - all

run_in_foreground: false
pidfile_path: /var/run/cachegrand.pid
use_slab_allocator: true

network_max_clients: 0
network_listen_backlog: 0
storage_max_partition_size_mb: 0
memory_max_keys: 0

protocols:
  - type: redis
    timeout:
      connection: 2000
      read: 2000
      write: 2000
      inactivity: 10000
    keepalive:
      time: 0
      interval: 0
      probes: 0
    redis:
      max_key_length: 8192
      max_command_length: 0
    bindings:
      - host: 0.0.0.0
        port: 12345

logs:
  - type: console
    level: [ warning, error ]

  - type: file
    level: [ all, no-verbose ]
    file:
      path: /var/log/cachegrand.log
)EOF";

std::string test_config_broken_missing_field_yaml_data =
        R"EOF(
worker_type: io_uring

)EOF";

std::string test_config_broken_unknown_field_yaml_data =
        R"EOF(
unknown_field: io_uring
)EOF";

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
            REQUIRE(config->worker_type == CONFIG_WORKER_TYPE_IO_URING);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->use_slab_allocator != NULL);
            REQUIRE(*config->use_slab_allocator == true);
            REQUIRE(config->protocols_count == 1);
            REQUIRE(config->logs_count == 2);
            REQUIRE(cyaml_logger_context.data == NULL);
            REQUIRE(cyaml_logger_context.data_length == 0);
            REQUIRE(err == CYAML_OK);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - missing field") {
            const char* cyaml_logger_context_data_cmp =
                    "Load: Missing required mapping field: run_in_foreground\nLoad: Backtrace:\n  in mapping field: worker_type\n";

            err = cyaml_load_data(
                    (const uint8_t *)(test_config_broken_missing_field_yaml_data.c_str()),
                    test_config_broken_missing_field_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    NULL);

            REQUIRE(config == NULL);
            REQUIRE(strcmp(cyaml_logger_context_data_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(cyaml_logger_context_data_cmp));
            REQUIRE(err == CYAML_ERR_MAPPING_FIELD_MISSING);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - unknown field") {
            const char* cyaml_logger_context_data_cmp =
                    "Load: Unexpected key: unknown_field\nLoad: Backtrace:\n  in mapping:\n";

            err = cyaml_load_data(
                    (const uint8_t *)(test_config_broken_unknown_field_yaml_data.c_str()),
                    test_config_broken_unknown_field_yaml_data.length(),
                    config_cyaml_config,
                    config_top_schema,
                    (cyaml_data_t **)&config,
                    NULL);

            REQUIRE(config == NULL);
            REQUIRE(strcmp(cyaml_logger_context_data_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(cyaml_logger_context_data_cmp));
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
            REQUIRE(config->worker_type == CONFIG_WORKER_TYPE_IO_URING);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->protocols_count == 1);
            REQUIRE(config->logs_count == 2);
            REQUIRE(cyaml_logger_context.data == NULL);
            REQUIRE(cyaml_logger_context.data_length == 0);
            REQUIRE(err == CYAML_OK);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - missing field") {
            const char* cyaml_logger_context_data_cmp =
                    "Load: Missing required mapping field: run_in_foreground\nLoad: Backtrace:\n  in mapping field: worker_type\n";

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
            REQUIRE(strcmp(cyaml_logger_context_data_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(cyaml_logger_context_data_cmp));
            REQUIRE(err == CYAML_ERR_MAPPING_FIELD_MISSING);

            cyaml_free(config_cyaml_config, config_top_schema, config, 0);
        }

        SECTION("broken - unknown field") {
            const char* cyaml_logger_context_data_cmp =
                    "Load: Unexpected key: unknown_field\nLoad: Backtrace:\n  in mapping:\n";

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
            REQUIRE(strcmp(cyaml_logger_context_data_cmp, cyaml_logger_context.data) == 0);
            REQUIRE(cyaml_logger_context.data_length == strlen(cyaml_logger_context_data_cmp));
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
            REQUIRE(config->worker_type == CONFIG_WORKER_TYPE_IO_URING);
            REQUIRE(config->cpus_count == 1);
            REQUIRE(config->protocols_count == 1);
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

    SECTION("config_log_level_t == log_level_t") {
        REQUIRE((int)CONFIG_LOG_LEVEL_ERROR == LOG_LEVEL_ERROR);
        REQUIRE((int)CONFIG_LOG_LEVEL_RECOVERABLE == LOG_LEVEL_RECOVERABLE);
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
}
