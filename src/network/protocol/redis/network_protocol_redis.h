#ifndef CACHEGRAND_NETWORK_PROTOCOL_REDIS_H
#define CACHEGRAND_NETWORK_PROTOCOL_REDIS_H

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_PROTOCOL_REDIS_KEY_MAX_LENGTH SLAB_OBJECT_SIZE_MAX

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, TYPE) \
    network_protocol_redis_process_command_##COMMAND_FUNC_PTR##_##TYPE

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_GENERIC(COMMAND_FUNC_PTR, TYPE, ARGUMENTS) \
    network_protocol_redis_command_funcptr_retval_t NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, TYPE) (ARGUMENTS)

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN \
    network_channel_t *channel, \
    storage_db_t *db, \
    network_protocol_redis_context_t *protocol_context, \
    protocol_redis_reader_context_t *reader_context

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_REQUIRE_STREAM \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_BEGIN \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index, \
    size_t argument_length

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_DATA \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index, \
    char* chunk_data, \
    size_t chunk_length

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_END \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_FULL \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index, \
    char* chunk_data, \
    size_t chunk_length

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_BEGIN(COMMAND_FUNC_PTR) \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        command_begin, \
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN)

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM(COMMAND_FUNC_PTR) \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_require_stream, \
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_REQUIRE_STREAM)

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN(COMMAND_FUNC_PTR) \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_stream_begin, \
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_BEGIN)

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA(COMMAND_FUNC_PTR) \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_stream_data, \
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_DATA)

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END(COMMAND_FUNC_PTR) \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_stream_end, \
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_END)

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL(COMMAND_FUNC_PTR) \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_full, \
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_FULL)

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_END(COMMAND_FUNC_PTR) \
    NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        command_end, \
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END)

#define NETWORK_PROTOCOL_REDIS_COMMAND(ID, COMMAND, COMMAND_FUNC_PTR, POS_ARGS_COUNT) \
    { \
        .command = NETWORK_PROTOCOL_REDIS_COMMAND_##ID, \
        .length = sizeof(COMMAND) - 1, /* sizeof takes into account the NULL char at the end, different behaviour than strlen */ \
        .string = (COMMAND), \
        .string_len = strlen(COMMAND), \
        .command_begin_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_begin), \
        .argument_require_stream_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_require_stream), \
        .argument_stream_begin_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_stream_begin), \
        .argument_stream_data_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_stream_data), \
        .argument_stream_end_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_stream_end), \
        .argument_full_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_full), \
        .command_end_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_end), \
        .required_positional_arguments_count = (POS_ARGS_COUNT) \
    }

#define NETWORK_PROTOCOL_REDIS_COMMAND_NO_STREAM(ID, COMMAND, COMMAND_FUNC_PTR, POS_ARGS_COUNT) \
    { \
        .command = NETWORK_PROTOCOL_REDIS_COMMAND_##ID, \
        .length = sizeof(COMMAND) - 1, /* sizeof takes into account the NULL char at the end, different behaviour than strlen */ \
        .string = (COMMAND), \
        .string_len = strlen(COMMAND), \
        .command_begin_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_begin), \
        .argument_require_stream_funcptr = NULL, \
        .argument_stream_begin_funcptr = NULL, \
        .argument_stream_data_funcptr = NULL, \
        .argument_stream_end_funcptr = NULL, \
        .argument_full_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_full), \
        .command_end_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_end), \
        .required_positional_arguments_count = (POS_ARGS_COUNT) \
    }

#define NETWORK_PROTOCOL_REDIS_COMMAND_NO_ARGS(ID, COMMAND, COMMAND_FUNC_PTR, POS_ARGS_COUNT) \
    { \
        .command = NETWORK_PROTOCOL_REDIS_COMMAND_##ID, \
        .length = sizeof(COMMAND) - 1, /* sizeof takes into account the NULL char at the end, different behaviour than strlen */ \
        .string = (COMMAND), \
        .string_len = strlen(COMMAND), \
        .command_begin_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_begin), \
        .argument_require_stream_funcptr = NULL, \
        .argument_stream_begin_funcptr = NULL, \
        .argument_stream_data_funcptr = NULL, \
        .argument_stream_end_funcptr = NULL, \
        .argument_full_funcptr = NULL, \
        .command_end_funcptr = NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_end), \
        .required_positional_arguments_count = (POS_ARGS_COUNT) \
    }

typedef void network_protocol_redis_command_context_t;
typedef bool network_protocol_redis_command_funcptr_retval_t;

// This typedef is needed before the declaration of the function pointers as it's used in there
// the entire struct can't be moved because of the dependencies
typedef struct network_protocol_redis_context network_protocol_redis_context_t;

typedef network_protocol_redis_command_funcptr_retval_t (network_protocol_redis_command_begin_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN);
typedef network_protocol_redis_command_funcptr_retval_t (network_protocol_redis_command_argument_require_stream_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_REQUIRE_STREAM);
typedef network_protocol_redis_command_funcptr_retval_t (network_protocol_redis_command_argument_stream_begin_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_BEGIN);
typedef network_protocol_redis_command_funcptr_retval_t (network_protocol_redis_command_argument_stream_data_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_DATA);
typedef network_protocol_redis_command_funcptr_retval_t (network_protocol_redis_command_argument_stream_end_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_END);
typedef network_protocol_redis_command_funcptr_retval_t (network_protocol_redis_command_argument_full_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_FULL);
typedef network_protocol_redis_command_funcptr_retval_t (network_protocol_redis_command_end_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END);

enum network_protocol_redis_commands {
    NETWORK_PROTOCOL_REDIS_COMMAND_NOP = 0,
    NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN,
    NETWORK_PROTOCOL_REDIS_COMMAND_SHUTDOWN,
    NETWORK_PROTOCOL_REDIS_COMMAND_HELLO,
    NETWORK_PROTOCOL_REDIS_COMMAND_QUIT,
    NETWORK_PROTOCOL_REDIS_COMMAND_PING,
    NETWORK_PROTOCOL_REDIS_COMMAND_GET,
    NETWORK_PROTOCOL_REDIS_COMMAND_SET,
    NETWORK_PROTOCOL_REDIS_COMMAND_DEL,
};
typedef enum network_protocol_redis_commands network_protocol_redis_commands_t;

typedef struct network_protocol_redis_command_info network_protocol_redis_command_info_t;
struct network_protocol_redis_command_info {
    network_protocol_redis_commands_t command;
    size_t length;
    // Redis longest command is 10 chars
    char string[32];
    uint8_t string_len;
    network_protocol_redis_command_begin_funcptr_t *command_begin_funcptr;
    network_protocol_redis_command_argument_require_stream_funcptr_t *argument_require_stream_funcptr;
    network_protocol_redis_command_argument_stream_begin_funcptr_t *argument_stream_begin_funcptr;
    network_protocol_redis_command_argument_stream_data_funcptr_t *argument_stream_data_funcptr;
    network_protocol_redis_command_argument_stream_end_funcptr_t *argument_stream_end_funcptr;
    network_protocol_redis_command_argument_full_funcptr_t *argument_full_funcptr;
    network_protocol_redis_command_end_funcptr_t *command_end_funcptr;
    uint8_t required_positional_arguments_count;
};

struct network_protocol_redis_context {
    protocol_redis_resp_version_t resp_version;
    protocol_redis_reader_context_t reader_context;
    network_protocol_redis_commands_t command;
    network_protocol_redis_command_info_t *command_info;
    network_protocol_redis_command_context_t *command_context;
    bool skip_command;
};

void network_protocol_redis_accept(
        network_channel_t *channel);

//bool network_protocol_redis_read_buffer_rewind(
//        network_channel_buffer_t *read_buffer,
//        network_protocol_redis_context_t *protocol_context);

bool network_protocol_redis_process_events(
        network_channel_t *channel,
        network_channel_buffer_t *read_buffer,
        network_protocol_redis_context_t *protocol_context);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_PROTOCOL_REDIS_H
