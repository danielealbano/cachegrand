/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdbool.h>
#include <string.h>
#include <argp.h>

#include "xalloc.h"
#include "cmake_config.h"

#include "program_arguments.h"

#define OPTION_LIST_TLS_CIPHER_SUITES_KEY   0x1000

int program_arguments_parser_testing = 0;

static struct argp_option program_arguments_parser_options[] = {
        {"config-file", 'c', "FILE", 0,
            "Config file (default config file " CACHEGRAND_CONFIG_PATH_DEFAULT " )"},
        {"log-level",   'l', "LOG LEVEL", 0,
                "log level (error, warning, info, verbose, debug)" },
        {"list-tls-cipher-suites",   OPTION_LIST_TLS_CIPHER_SUITES_KEY, NULL, 0,
                "list the available tls cipher suites" },
        { NULL }
};

const program_arguments_strval_map_t program_arguments_log_level_strings[] = {
        { "error", PROGRAM_ARGUMENTS_LOG_LEVEL_ERROR },
        { "warning", PROGRAM_ARGUMENTS_LOG_LEVEL_WARNING },
        { "info", PROGRAM_ARGUMENTS_LOG_LEVEL_INFO },
        { "verbose", PROGRAM_ARGUMENTS_LOG_LEVEL_VERBOSE },
        { "debug", PROGRAM_ARGUMENTS_LOG_LEVEL_DEBUG },
};

bool program_arguments_strval_map_get(
        const program_arguments_strval_map_t* array,
        size_t array_length,
        char* key,
        int* value) {
    bool found = false;

    for(int array_index = 0; array_index < array_length; array_index++) {
        program_arguments_strval_map_t strval_map = array[array_index];
        size_t strval_len = strlen(strval_map.key);
        if (strncmp(strval_map.key, key, strval_len) == 0) {
            *value = strval_map.value;
            found = true;
            break;
        }
    }

    return found;
}

error_t program_arguments_argp_parser(
        int key,
        char *arg,
        struct argp_state *state) {
    program_arguments_t* program_arguments = state->input;

    switch (key) {
        case OPTION_LIST_TLS_CIPHER_SUITES_KEY:
            program_arguments->list_tls_available_cipher_suites = true;
            break;

        case 'c':
            program_arguments->config_file = arg;
            break;

        case 'l':
            if (program_arguments_strval_map_get(
                    program_arguments_log_level_strings,
                    program_arguments_strval_map_len(program_arguments_log_level_strings),
                    arg,
                    (int*)(&program_arguments->log_level)) == false) {
                if (program_arguments_parser_testing == 0) {
                    argp_error(state, "invalid value for log-level");
                } else {
                    return ARGP_ERR_UNKNOWN;
                }
            }
            break;

        case ARGP_KEY_ARG:
            argp_usage(state);

            // argp_usage normally exits but for the testing we set ARGP_SILENT that sets ARGP_NO_EXIT, among other
            // flags, and therefore we need a return with ARGP_ERR_UNKNOWN to stop argp from further parsing.
            return ARGP_ERR_UNKNOWN;

        case ARGP_KEY_END:
            // do nothing
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

char* program_arguments_docs_header_prepare() {
    char* docs_header;
    char docs_header_format[] = "";

    size_t string_length = snprintf(NULL, 0, docs_header_format, "");

    docs_header = (char*)xalloc_alloc_zero(string_length);
    snprintf(docs_header, string_length, docs_header_format, "");

    return docs_header;
}

void program_arguments_docs_header_free(char* docs_header) {
    xalloc_free(docs_header);
}

program_arguments_t* program_arguments_init() {
    program_arguments_t* program_arguments = (program_arguments_t*)xalloc_alloc_zero(sizeof(program_arguments_t));

    program_arguments->log_level = PROGRAM_ARGUMENTS_LOG_LEVEL_MAX;

    return program_arguments;
}

void program_arguments_free(
        program_arguments_t* program_arguments) {
    xalloc_free(program_arguments);
}

bool program_arguments_parse(
        int argc,
        char **argv,
        program_arguments_t* program_arguments) {
    int arg_parse_flags = 0;
    char* docs_header = program_arguments_docs_header_prepare();

    struct argp argp = {
            .options = program_arguments_parser_options,
            .parser = program_arguments_argp_parser,
            .doc = "",
            .args_doc = docs_header
    };

    if (program_arguments_parser_testing != 0) {
        arg_parse_flags |= ARGP_SILENT;
    }

    error_t res = argp_parse(&argp, argc, argv, arg_parse_flags, 0, program_arguments);

    program_arguments_docs_header_free(docs_header);

    return res == 0;
}
