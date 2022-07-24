#ifndef CACHEGRAND_MODULE_REDIS_H
#define CACHEGRAND_MODULE_REDIS_H

#ifdef __cplusplus
extern "C" {
#endif

#define MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, TYPE) \
    module_redis_process_command_##COMMAND_FUNC_PTR##_##TYPE

#define MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, TYPE) \
    module_redis_process_command_##COMMAND_FUNC_PTR##_##TYPE##_autogen

#define MODULE_REDIS_COMMAND_FUNCPTR_GENERIC(COMMAND_FUNC_PTR, TYPE, ARGUMENTS) \
    module_redis_command_funcptr_retval_t MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, TYPE) (ARGUMENTS)

#define MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN(COMMAND_FUNC_PTR, TYPE, ARGUMENTS) \
    module_redis_command_funcptr_retval_t MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, TYPE) (ARGUMENTS)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN \
    network_channel_t *channel, \
    storage_db_t *db, \
    module_redis_context_t *protocol_context, \
    protocol_redis_reader_context_t *reader_context

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_REQUIRE_STREAM \
    MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_BEGIN \
    MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index, \
    size_t argument_length

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_DATA \
    MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index, \
    char* chunk_data, \
    size_t chunk_length

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_END \
    MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_FULL \
    MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN, \
    uint32_t argument_index, \
    char* chunk_data, \
    size_t chunk_length

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END \
    MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_FREE \
    MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN

#define MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_BEGIN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        command_begin, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_require_stream, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_REQUIRE_STREAM)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_stream_begin, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_BEGIN)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_stream_data, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_DATA)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_stream_end, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_END)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_full, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_FULL)

#define MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        command_end, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END)

#define MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_FREE(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC( \
        COMMAND_FUNC_PTR, \
        command_free, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END)

#define MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_BEGIN_AUTOGEN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN( \
        COMMAND_FUNC_PTR, \
        command_begin, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM_AUTOGEN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN( \
        COMMAND_FUNC_PTR, \
        argument_require_stream, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_REQUIRE_STREAM)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN_AUTOGEN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN( \
        COMMAND_FUNC_PTR, \
        argument_stream_begin, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_BEGIN)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA_AUTOGEN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN( \
        COMMAND_FUNC_PTR, \
        argument_stream_data, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_DATA)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END_AUTOGEN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN( \
        COMMAND_FUNC_PTR, \
        argument_stream_end, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_END)

#define MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL_AUTOGEN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN( \
        COMMAND_FUNC_PTR, \
        argument_full, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_FULL)

#define MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END_AUTOGEN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN( \
        COMMAND_FUNC_PTR, \
        command_end, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END)

#define MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_FREE_AUTOGEN(COMMAND_FUNC_PTR) \
    MODULE_REDIS_COMMAND_FUNCPTR_GENERIC_AUTOGEN( \
        COMMAND_FUNC_PTR, \
        command_free, \
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END)

#define MODULE_REDIS_COMMAND_AUTOGEN(ID, COMMAND, COMMAND_FUNC_PTR, POS_ARGS_COUNT) \
    { \
        .command = MODULE_REDIS_COMMAND_##ID, \
        .length = sizeof(COMMAND) - 1, /* sizeof takes into account the NULL char at the end, different behaviour than strlen */ \
        .string = (COMMAND), \
        .string_len = strlen(COMMAND), \
        .command_begin_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, command_begin), \
        .argument_require_stream_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, argument_require_stream), \
        .argument_stream_begin_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, argument_stream_begin), \
        .argument_stream_data_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, argument_stream_data), \
        .argument_stream_end_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, argument_stream_end), \
        .argument_full_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, argument_full), \
        .command_end_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, command_end), \
        .command_free_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME_AUTOGEN(COMMAND_FUNC_PTR, command_free), \
        .required_positional_arguments_count = (POS_ARGS_COUNT) \
    }

#define MODULE_REDIS_COMMAND(ID, COMMAND, COMMAND_FUNC_PTR, POS_ARGS_COUNT) \
    { \
        .command = MODULE_REDIS_COMMAND_##ID, \
        .length = sizeof(COMMAND) - 1, /* sizeof takes into account the NULL char at the end, different behaviour than strlen */ \
        .string = (COMMAND), \
        .string_len = strlen(COMMAND), \
        .command_begin_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_begin), \
        .argument_require_stream_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_require_stream), \
        .argument_stream_begin_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_stream_begin), \
        .argument_stream_data_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_stream_data), \
        .argument_stream_end_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_stream_end), \
        .argument_full_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_full), \
        .command_end_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_end), \
        .command_free_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_free), \
        .required_positional_arguments_count = (POS_ARGS_COUNT) \
    }

#define MODULE_REDIS_COMMAND_NO_STREAM(ID, COMMAND, COMMAND_FUNC_PTR, POS_ARGS_COUNT) \
    { \
        .command = MODULE_REDIS_COMMAND_##ID, \
        .length = sizeof(COMMAND) - 1, /* sizeof takes into account the NULL char at the end, different behaviour than strlen */ \
        .string = (COMMAND), \
        .string_len = strlen(COMMAND), \
        .command_begin_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_begin), \
        .argument_require_stream_funcptr = NULL, \
        .argument_stream_begin_funcptr = NULL, \
        .argument_stream_data_funcptr = NULL, \
        .argument_stream_end_funcptr = NULL, \
        .argument_full_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_full), \
        .command_end_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_end), \
        .command_free_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_free), \
        .required_positional_arguments_count = (POS_ARGS_COUNT) \
    }

#define MODULE_REDIS_COMMAND_NO_ARGS(ID, COMMAND, COMMAND_FUNC_PTR, POS_ARGS_COUNT) \
    { \
        .command = MODULE_REDIS_COMMAND_##ID, \
        .length = sizeof(COMMAND) - 1, /* sizeof takes into account the NULL char at the end, different behaviour than strlen */ \
        .string = (COMMAND), \
        .string_len = strlen(COMMAND), \
        .command_begin_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_begin), \
        .argument_require_stream_funcptr = NULL, \
        .argument_stream_begin_funcptr = NULL, \
        .argument_stream_data_funcptr = NULL, \
        .argument_stream_end_funcptr = NULL, \
        .argument_full_funcptr = NULL, \
        .command_end_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_end), \
        .command_free_funcptr = MODULE_REDIS_COMMAND_FUNCPTR_NAME(COMMAND_FUNC_PTR, command_free), \
        .required_positional_arguments_count = (POS_ARGS_COUNT) \
    }

typedef void module_redis_command_context_t;
typedef bool module_redis_command_funcptr_retval_t;

// This typedef is needed before the declaration of the function pointers as it's used in there
// the entire struct can't be moved because of the dependencies
typedef struct module_redis_context module_redis_context_t;

typedef module_redis_command_funcptr_retval_t (module_redis_command_begin_funcptr_t)(
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_BEGIN);
typedef module_redis_command_funcptr_retval_t (module_redis_command_argument_require_stream_funcptr_t)(
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_REQUIRE_STREAM);
typedef module_redis_command_funcptr_retval_t (module_redis_command_argument_stream_begin_funcptr_t)(
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_BEGIN);
typedef module_redis_command_funcptr_retval_t (module_redis_command_argument_stream_data_funcptr_t)(
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_DATA);
typedef module_redis_command_funcptr_retval_t (module_redis_command_argument_stream_end_funcptr_t)(
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_STREAM_END);
typedef module_redis_command_funcptr_retval_t (module_redis_command_argument_full_funcptr_t)(
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_ARGUMENT_FULL);
typedef module_redis_command_funcptr_retval_t (module_redis_command_end_funcptr_t)(
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_END);
typedef module_redis_command_funcptr_retval_t (module_redis_command_free_funcptr_t)(
        MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENTS_COMMAND_FREE);

enum module_redis_commands {
    MODULE_REDIS_COMMAND_NOP = 0,
    MODULE_REDIS_COMMAND_UNKNOWN,
    MODULE_REDIS_COMMAND_SHUTDOWN,
    MODULE_REDIS_COMMAND_HELLO,
    MODULE_REDIS_COMMAND_QUIT,
    MODULE_REDIS_COMMAND_PING,
    MODULE_REDIS_COMMAND_GET,
    MODULE_REDIS_COMMAND_SET,
    MODULE_REDIS_COMMAND_DEL,
    MODULE_REDIS_COMMAND_MGET,
};
typedef enum module_redis_commands module_redis_commands_t;

typedef struct module_redis_command_info module_redis_command_info_t;
struct module_redis_command_info {
    module_redis_commands_t command;
    size_t length;
    // Redis longest command is 10 chars
    char string[32];
    uint8_t string_len;
    module_redis_command_begin_funcptr_t *command_begin_funcptr;
    module_redis_command_argument_require_stream_funcptr_t *argument_require_stream_funcptr;
    module_redis_command_argument_stream_begin_funcptr_t *argument_stream_begin_funcptr;
    module_redis_command_argument_stream_data_funcptr_t *argument_stream_data_funcptr;
    module_redis_command_argument_stream_end_funcptr_t *argument_stream_end_funcptr;
    module_redis_command_argument_full_funcptr_t *argument_full_funcptr;
    module_redis_command_end_funcptr_t *command_end_funcptr;
    module_redis_command_free_funcptr_t *command_free_funcptr;
    uint8_t required_positional_arguments_count;
};

struct module_redis_context {
    protocol_redis_resp_version_t resp_version;
    protocol_redis_reader_context_t reader_context;
    size_t command_length;
    module_redis_commands_t command;
    module_redis_command_info_t *command_info;
    module_redis_command_context_t *command_context;
    size_t current_argument_token_data_offset;
    bool skip_command;
};

void module_redis_accept(
        network_channel_t *channel);

bool module_redis_process_data(
        network_channel_t *channel,
        network_channel_buffer_t *read_buffer,
        module_redis_context_t *protocol_context);

bool module_redis_is_key_too_long(
        network_channel_t *channel,
        size_t key_length);

#ifdef __cplusplus
}
#endif

#endif //MODULE_PROTOCOL_REDIS_H
