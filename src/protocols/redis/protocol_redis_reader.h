#ifndef CACHEGRAND_PROTOCOL_REDIS_READER_H
#define CACHEGRAND_PROTOCOL_REDIS_READER_H

#ifdef __cplusplus
extern "C" {
#endif

enum protocol_redis_reader_errors {
    PROTOCOL_REDIS_READER_ERROR_OK,
    PROTOCOL_REDIS_READER_ERROR_NO_DATA,
    PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH,
    PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES,
    PROTOCOL_REDIS_READER_ERROR_COMMAND_ALREADY_PARSED,
    PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_EXPECTED,
    PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH,
    PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_MISSING_END_SIGNATURE,
};
typedef enum protocol_redis_reader_errors protocol_redis_reader_errors_t;


enum protocol_redis_types {
    PROTOCOL_REDIS_TYPE_SIMPLE_STRING = '+',
    PROTOCOL_REDIS_TYPE_BLOB_STRING = '$',
    PROTOCOL_REDIS_TYPE_VERBATIM_STRING = '=',
    PROTOCOL_REDIS_TYPE_NUMBER = ':',
    PROTOCOL_REDIS_TYPE_DOUBLE = ',',
    PROTOCOL_REDIS_TYPE_BIG_NUMBER = '(',
    PROTOCOL_REDIS_TYPE_NULL = '_',
    PROTOCOL_REDIS_TYPE_BOOLEAN = '#',
    PROTOCOL_REDIS_TYPE_ARRAY = '*',
    PROTOCOL_REDIS_TYPE_MAP = '%',
    PROTOCOL_REDIS_TYPE_SET = '~',
    PROTOCOL_REDIS_TYPE_ATTRIBUTE = '|',
    PROTOCOL_REDIS_TYPE_PUSH = '>',

    PROTOCOL_REDIS_TYPE_SIMPLE_ERROR = '-',
    PROTOCOL_REDIS_TYPE_BLOB_ERROR = '!',
};
typedef enum protocol_redis_types protocol_redis_types_t;

enum protocol_redis_reader_states {
    PROTOCOL_REDIS_READER_STATE_BEGIN,
    PROTOCOL_REDIS_READER_STATE_INLINE_WAITING_ARGUMENT,
    PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_LENGTH,
    PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA,
    PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED,
};
typedef enum protocol_redis_reader_states protocol_redis_reader_states_t;

struct protocol_redis_reader_context_argument {
    char* value;
    unsigned long length;
    bool all_read;
    bool copied_from_buffer;
};
typedef struct protocol_redis_reader_context_argument protocol_redis_reader_context_argument_t;

struct protocol_redis_reader_context {
    protocol_redis_reader_states_t state;
    protocol_redis_reader_errors_t error;

    struct {
        protocol_redis_reader_context_argument_t *list;
        long count;
        struct {
            long index;
            bool beginning;
            unsigned long length;
        } current;

        union {
            struct {
                struct {
                    char received_length;
                } current;
            } resp_protocol;
            struct {
                struct {
                    char quote_char;
                    bool decode_escaped_chars;
                } current;
            } inline_protocol;
        };
    } arguments;
};
typedef struct protocol_redis_reader_context protocol_redis_reader_context_t;


protocol_redis_reader_context_t* protocol_redis_reader_context_init();
void protocol_redis_reader_context_free(
        protocol_redis_reader_context_t* context);

void protocol_redis_reader_context_arguments_free(
        protocol_redis_reader_context_t* context);

int protocol_redis_reader_context_arguments_clone_current(
        protocol_redis_reader_context_t* context);

void protocol_redis_reader_context_reset(
        protocol_redis_reader_context_t* context);

long protocol_redis_reader_read(
        char* buffer,
        size_t length,
        protocol_redis_reader_context_t* context);


#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROTOCOL_REDIS_READER_H
