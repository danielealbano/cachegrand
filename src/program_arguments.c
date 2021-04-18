#include <stdbool.h>
#include <string.h>
#include <argp.h>

#include "xalloc.h"
#include "cmake_config.h"

#include "program_arguments.h"

#if DEBUG == 1
int program_arguments_parser_testing = 0;
#endif

static struct argp_option program_arguments_parser_options[] = {
        {"config-file", 'c', "FILE", 0,
            "Config file (default config file " CACHEGRAND_CONFIG_PATH_DEFAULT " )"},
        {"log-level",   'l', "LOG LEVEL", 0,
                "log level (error, warning, info, verbose, debug)" },
        { NULL }
};

error_t program_arguments_argp_parser(
        int key,
        char *arg,
        struct argp_state *state) {
    program_arguments_t* program_arguments = state->input;

    switch (key) {
        case 'c':
            program_arguments->config_file = arg;
            break;

        case 'l':
            program_arguments_strval_map_get(program_arguments_log_level_strings, arg, program_arguments->log_level);
            if (program_arguments->log_level == -1) {
#if DEBUG == 1
                if (program_arguments_parser_testing == 0) {
#endif
                argp_error(state, "invalid value for log-level");
#if DEBUG == 1
                } else {
                    return ARGP_ERR_UNKNOWN;
                }
#endif
            }
            break;

        case ARGP_KEY_ARG:
            argp_usage(state);

            // arpg_usage normally exits but for the testing we set ARGP_SILENT that sets ARGP_NO_EXIT, among other
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
    return (program_arguments_t*)xalloc_alloc_zero(sizeof(program_arguments_t));
}

void program_arguments_free(program_arguments_t* program_arguments) {
    xalloc_free(program_arguments);
}

void program_arguments_parse(
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

#if DEBUG == 1
    if (program_arguments_parser_testing != 0) {
        arg_parse_flags |= ARGP_SILENT;
    }
#endif

    error_t res = argp_parse(&argp, argc, argv, arg_parse_flags, 0, program_arguments);

    program_arguments_docs_header_free(docs_header);

}