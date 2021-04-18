#ifndef CACHEGRAND_PROGRAM_ARGUMENTS_H
#define CACHEGRAND_PROGRAM_ARGUMENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#if DEBUG == 1
extern int program_arguments_parser_testing;
#endif

#define program_arguments_strval_map_len(STRVAL_ARRAY) (sizeof(STRVAL_ARRAY) / sizeof(program_arguments_strval_map_t))

#define program_arguments_strval_map_get(STRVAL_ARRAY, STRING, VALUE) { \
    (VALUE) = -1; \
    for(int STRVAL_ARRAY_index = 0; STRVAL_ARRAY_index < program_arguments_strval_map_len(STRVAL_ARRAY); STRVAL_ARRAY_index++) { \
        program_arguments_strval_map_t strval_map = ((STRVAL_ARRAY)[(STRVAL_ARRAY_index)]); \
        size_t strval_len = strlen(strval_map.key); \
        if (strncmp(strval_map.key, STRING, strval_len) == 0) { \
            (VALUE) = strval_map.value; \
        } \
    } \
}

typedef struct program_arguments_strval_map program_arguments_strval_map_t;
struct program_arguments_strval_map {
    const char *key;
    int value;
};

enum program_arguments_log_level {
    PROGRAM_ARGUMENTS_LOG_LEVEL_ERROR,
    PROGRAM_ARGUMENTS_LOG_LEVEL_RECOVERABLE,
    PROGRAM_ARGUMENTS_LOG_LEVEL_WARNING,
    PROGRAM_ARGUMENTS_LOG_LEVEL_INFO,
    PROGRAM_ARGUMENTS_LOG_LEVEL_VERBOSE,
    PROGRAM_ARGUMENTS_LOG_LEVEL_DEBUG
};
typedef enum program_arguments_log_level program_arguments_log_level_t;

const program_arguments_strval_map_t program_arguments_log_level_strings[] = {
        { "error", PROGRAM_ARGUMENTS_LOG_LEVEL_ERROR },
        { "recoverable", PROGRAM_ARGUMENTS_LOG_LEVEL_RECOVERABLE },
        { "warning", PROGRAM_ARGUMENTS_LOG_LEVEL_WARNING },
        { "info", PROGRAM_ARGUMENTS_LOG_LEVEL_INFO },
        { "verbose", PROGRAM_ARGUMENTS_LOG_LEVEL_VERBOSE },
        { "debug", PROGRAM_ARGUMENTS_LOG_LEVEL_DEBUG },
};

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
void program_arguments_parse(
        int argc,
        char **argv,
        program_arguments_t* program_arguments);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROGRAM_ARGUMENTS_H
