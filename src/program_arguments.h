#ifndef CACHEGRAND_PROGRAM_ARGUMENTS_H
#define CACHEGRAND_PROGRAM_ARGUMENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#if DEBUG == 1
extern int program_arguments_parser_testing;
#endif

#define program_arguments_strval_map_len(STRVAL_ARRAY) (sizeof(STRVAL_ARRAY) / sizeof(program_arguments_strval_map_t))

typedef struct program_arguments_strval_map program_arguments_strval_map_t;
struct program_arguments_strval_map {
    const char *key;
    int value;
};

enum program_arguments_log_level {
    PROGRAM_ARGUMENTS_LOG_LEVEL_ERROR = 1 << 1,
    PROGRAM_ARGUMENTS_LOG_LEVEL_RECOVERABLE = 1 << 2,
    PROGRAM_ARGUMENTS_LOG_LEVEL_WARNING = 1 << 3,
    PROGRAM_ARGUMENTS_LOG_LEVEL_INFO = 1 << 4,
    PROGRAM_ARGUMENTS_LOG_LEVEL_VERBOSE = 1 << 5,
    PROGRAM_ARGUMENTS_LOG_LEVEL_DEBUG = 1 << 6,
    PROGRAM_ARGUMENTS_LOG_LEVEL_MAX
};
typedef enum program_arguments_log_level program_arguments_log_level_t;

typedef struct program_arguments program_arguments_t;
struct program_arguments {
    program_arguments_log_level_t log_level;
    char *config_file;
};

char* program_arguments_docs_header_prepare();

void program_arguments_docs_header_free(
        char* docs_header);

program_arguments_t* program_arguments_init();

void program_arguments_free(
        program_arguments_t* program_arguments);

bool program_arguments_parse(
        int argc,
        char **argv,
        program_arguments_t* program_arguments);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROGRAM_ARGUMENTS_H
