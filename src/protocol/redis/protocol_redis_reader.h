#ifndef CACHEGRAND_PROTOCOL_REDIS_READER_H
#define CACHEGRAND_PROTOCOL_REDIS_READER_H

#ifdef __cplusplus
extern "C" {
#endif

enum protocol_redis_reader_errors {
    PROTOCOL_REDIS_READER_ERROR_OK,
    PROTOCOL_REDIS_READER_ERROR_NO_DATA,
    PROTOCOL_REDIS_READER_ERROR_INLINE_PROTOCOL_NOT_SUPPORTED,
    PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH,
    PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES,
    PROTOCOL_REDIS_READER_ERROR_COMMAND_ALREADY_PARSED,
    PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_EXPECTED,
    PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH,
    PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_MISSING_END_SIGNATURE,
};
typedef enum protocol_redis_reader_errors protocol_redis_reader_errors_t;

enum protocol_redis_reader_states {
    PROTOCOL_REDIS_READER_STATE_BEGIN,
    PROTOCOL_REDIS_READER_STATE_INLINE_WAITING_ARGUMENT,
    PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_LENGTH,
    PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA,
    PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA_END,
    PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED,
};
typedef enum protocol_redis_reader_states protocol_redis_reader_states_t;

enum protocol_redis_reader_op_type {
    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN,
    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END,
    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
};
typedef enum protocol_redis_reader_op_type protocol_redis_reader_op_type_t;

typedef struct protocol_redis_reader_op protocol_redis_reader_op_t;
struct protocol_redis_reader_op {
    protocol_redis_reader_op_type_t type;
    off_t data_read_len;
    union {
        struct {
            uint32_t arguments_count;
        } command;
        struct {
            uint32_t index;
            size_t offset;
            size_t length;
            size_t data_length;
        } argument;
    } data;
};

struct protocol_redis_reader_context {
    protocol_redis_reader_states_t state;
    protocol_redis_reader_protocol_types_t protocol_type;
    protocol_redis_reader_errors_t error;

    struct {
        uint32_t count;
        struct {
            long index;
            bool beginning;
            size_t length;
            size_t received_length;
        } current;
    } arguments;
};
typedef struct protocol_redis_reader_context protocol_redis_reader_context_t;

void protocol_redis_reader_context_free(
        protocol_redis_reader_context_t* context);

void protocol_redis_reader_context_reset(
        protocol_redis_reader_context_t* context);

int32_t protocol_redis_reader_read(
        char* buffer,
        size_t length,
        protocol_redis_reader_context_t* context,
        protocol_redis_reader_op_t* ops,
        uint8_t ops_size);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROTOCOL_REDIS_READER_H
