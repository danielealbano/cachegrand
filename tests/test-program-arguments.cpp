/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "xalloc.h"
#include "log/log.h"
#include "config.h"

#include "program_arguments.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

#define TEST_PROGRAM_NAME "test-program-arguments-fake"

const char* test_program_arguments_empty[] = {};
const char* test_program_arguments_only_config_file[] = { TEST_PROGRAM_NAME, "-c", "/path/to/config/file.conf" };
const char* test_program_arguments_only_log_level_supported[] = { TEST_PROGRAM_NAME, "-l", "debug" };
const char* test_program_arguments_only_log_level_unsupported[] = { TEST_PROGRAM_NAME, "-l", "unsupported_log_level" };
const char* test_program_arguments_multiple[] = { TEST_PROGRAM_NAME, "-c", "/path/to/config/file.conf", "-l", "debug" };
const char* test_program_arguments_position_unsupported[] = { TEST_PROGRAM_NAME, "positional", "arguments" };

TEST_CASE("program_arguments.c", "[program][arguments]") {
    program_arguments_parser_testing = 1;

    SECTION("program_arguments_log_level_t == log_level_t") {
        REQUIRE((int)PROGRAM_ARGUMENTS_LOG_LEVEL_ERROR == LOG_LEVEL_ERROR);
        REQUIRE((int)PROGRAM_ARGUMENTS_LOG_LEVEL_WARNING == LOG_LEVEL_WARNING);
        REQUIRE((int)PROGRAM_ARGUMENTS_LOG_LEVEL_INFO == LOG_LEVEL_INFO);
        REQUIRE((int)PROGRAM_ARGUMENTS_LOG_LEVEL_VERBOSE == LOG_LEVEL_VERBOSE);
        REQUIRE((int)PROGRAM_ARGUMENTS_LOG_LEVEL_DEBUG == LOG_LEVEL_DEBUG);
    }

    SECTION("program_arguments_docs_header_prepare") {
        char* docs_header = program_arguments_docs_header_prepare();

        REQUIRE(docs_header != NULL);

        xalloc_free(docs_header);
    }

    SECTION("program_arguments_docs_header_free") {
        char* docs_header = program_arguments_docs_header_prepare();
        program_arguments_docs_header_free(docs_header);
    }

    SECTION("program_arguments_init") {
        program_arguments_t* program_arguments = program_arguments_init();

        REQUIRE(program_arguments != NULL);
        REQUIRE(program_arguments->log_level == PROGRAM_ARGUMENTS_LOG_LEVEL_MAX);
        REQUIRE(program_arguments->config_file == NULL);

        xalloc_free(program_arguments);
    }

    SECTION("program_arguments_free") {
        program_arguments_t* program_arguments = program_arguments_init();
        program_arguments_free(program_arguments);
    }

    SECTION("program_arguments_parse") {
        program_arguments_t* program_arguments = program_arguments_init();

        SECTION("empty arguments") {
            int argc = sizeof(test_program_arguments_empty) / sizeof(char*);

            REQUIRE(program_arguments_parse(argc, (char**)test_program_arguments_empty, program_arguments) == true);
            REQUIRE(program_arguments->log_level == PROGRAM_ARGUMENTS_LOG_LEVEL_MAX);
            REQUIRE(program_arguments->config_file == NULL);
        }

        SECTION("only config file - supported") {
            int argc = sizeof(test_program_arguments_only_config_file) / sizeof(char*);

            REQUIRE(program_arguments_parse(argc, (char**)test_program_arguments_only_config_file, program_arguments) == true);
            REQUIRE(program_arguments->log_level == PROGRAM_ARGUMENTS_LOG_LEVEL_MAX);
            REQUIRE(program_arguments->config_file == test_program_arguments_only_config_file[2]);
        }

        SECTION("only log level - supported") {
            int argc = sizeof(test_program_arguments_only_log_level_supported) / sizeof(char*);

            REQUIRE(program_arguments_parse(argc, (char**)test_program_arguments_only_log_level_supported, program_arguments) == true);
            REQUIRE(program_arguments->log_level == PROGRAM_ARGUMENTS_LOG_LEVEL_DEBUG);
            REQUIRE(program_arguments->config_file == NULL);
        }

        SECTION("only log level - unsupported") {
            int argc = sizeof(test_program_arguments_only_log_level_unsupported) / sizeof(char*);

            REQUIRE(program_arguments_parse(argc, (char**)test_program_arguments_only_log_level_unsupported, program_arguments) == false);
        }

        SECTION("multiple - supported") {
            int argc = sizeof(test_program_arguments_multiple) / sizeof(char*);

            REQUIRE(program_arguments_parse(argc, (char**)test_program_arguments_multiple, program_arguments) == true);
            REQUIRE(program_arguments->log_level == PROGRAM_ARGUMENTS_LOG_LEVEL_DEBUG);
            REQUIRE(program_arguments->config_file == test_program_arguments_only_config_file[2]);
        }

        SECTION("positional - unsupported") {
            int argc = sizeof(test_program_arguments_position_unsupported) / sizeof(char*);

            REQUIRE(program_arguments_parse(argc, (char**)test_program_arguments_position_unsupported, program_arguments) == false);
        }

        program_arguments_free(program_arguments);
    }

    program_arguments_parser_testing = 0;
}
