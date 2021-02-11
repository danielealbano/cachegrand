#ifndef CACHEGRAND_NETWORK_PROTOCOL_REDIS_H
#define CACHEGRAND_NETWORK_PROTOCOL_REDIS_H

#ifdef __cplusplus
extern "C" {
#endif


#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_NAME(COMMAND_FUNC_PTR, TYPE) \
    network_protocol_redis_process_command_##COMMAND_FUNC_PTR##_##TYPE

#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_GENERIC(COMMAND_FUNC_PTR, TYPE, ARGUMENTS) \
    char* NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_NAME(COMMAND_FUNC_PTR, TYPE) (ARGUMENTS)

#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_BEGIN \
    protocol_redis_reader_context_t *reader_context, \
    network_protocol_redis_command_context_t *command_context, \
    char* send_buffer_start, \
    char* send_buffer_end

#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_ARGUMENT_PROCESSED \
    NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_BEGIN, \
    uint32_t argument_index

#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_END \
    NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_BEGIN

#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_BEGIN(COMMAND_FUNC_PTR, ...) \
    NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_GENERIC( \
        COMMAND_FUNC_PTR, \
        begin, \
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_BEGIN)

#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_ARGUMENT_PROCESSED(COMMAND_FUNC_PTR, ...) \
    NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_GENERIC( \
        COMMAND_FUNC_PTR, \
        argument_processed, \
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_ARGUMENT_PROCESSED)

#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_END(COMMAND_FUNC_PTR, ...) \
    NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_GENERIC( \
        COMMAND_FUNC_PTR, \
        end, \
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_END)

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_BEGIN(COMMAND_FUNC_PTR, ...) \
    NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_BEGIN(COMMAND_FUNC_PTR) { \
        __VA_ARGS__ \
        return send_buffer_start; \
    }

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_ARGUMENT_PROCESSED(COMMAND_FUNC_PTR, ...) \
    NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_ARGUMENT_PROCESSED(COMMAND_FUNC_PTR) { \
        __VA_ARGS__ \
        return send_buffer_start; \
    }

#define NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_END(COMMAND_FUNC_PTR, ...) \
    NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_END(COMMAND_FUNC_PTR) { \
        __VA_ARGS__ \
        return send_buffer_start; \
    }

#define NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR(...) \
    __VA_ARGS__ \
    if (send_buffer_start == NULL) { \
        return send_buffer_start; \
    }

#define NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_ITEM(ID, COMMAND, COMMAND_FUNC_PTR, POS_ARGS_COUNT) \
    { \
        NETWORK_PROTOCOL_REDIS_COMMAND_##ID, \
        sizeof(COMMAND) - 1, /* sizeof takes into account the NULL char at the end, different behaviour than strlen */ \
        COMMAND, \
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_NAME(COMMAND_FUNC_PTR, begin), \
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_NAME(COMMAND_FUNC_PTR, argument_processed), \
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_NAME(COMMAND_FUNC_PTR, end), \
        POS_ARGS_COUNT \
    }

typedef struct network_protocol_redis_command_context network_protocol_redis_command_context_t;
struct network_protocol_redis_command_context {

};

typedef char* (network_protocol_redis_command_begin_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_BEGIN);
typedef char* (network_protocol_redis_command_argument_processed_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_ARGUMENT_PROCESSED);
typedef char* (network_protocol_redis_command_end_funcptr_t)(
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_ARGUMENTS_END);

enum network_protocol_redis_commands {
    NETWORK_PROTOCOL_REDIS_COMMAND_NOP = 0,
    NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN,
    NETWORK_PROTOCOL_REDIS_COMMAND_QUIT,
    NETWORK_PROTOCOL_REDIS_COMMAND_HELLO,
    NETWORK_PROTOCOL_REDIS_COMMAND_PING,
    NETWORK_PROTOCOL_REDIS_COMMAND_GET,
    NETWORK_PROTOCOL_REDIS_COMMAND_SET,
};
typedef enum network_protocol_redis_commands network_protocol_redis_commands_t;

typedef struct network_protocol_redis_command_info network_protocol_redis_command_info_t;
struct network_protocol_redis_command_info {
    network_protocol_redis_commands_t command;
    size_t length;
    // Redis longest command is 10 chars
    char string[11];
    network_protocol_redis_command_begin_funcptr_t *being_funcptr;
    network_protocol_redis_command_argument_processed_funcptr_t *argument_processed_funcptr;
    network_protocol_redis_command_end_funcptr_t *end_funcptr;
    uint8_t positional_arguments_count;
};

typedef struct network_protocol_redis_context network_protocol_redis_context_t;
struct network_protocol_redis_context {
    protocol_redis_reader_context_t *reader_context;
    network_protocol_redis_commands_t command;
    network_protocol_redis_command_info_t *command_info;
    network_protocol_redis_command_context_t command_context;
    bool skip_command;
};

NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_BEGIN(quit);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_ARGUMENT_PROCESSED(quit);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_END(quit);

NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_BEGIN(hello);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_ARGUMENT_PROCESSED(hello);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_END(hello);

NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_BEGIN(ping);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_ARGUMENT_PROCESSED(ping);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_END(ping);

NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_BEGIN(set);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_ARGUMENT_PROCESSED(set);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_END(set);

NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_BEGIN(get);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_ARGUMENT_PROCESSED(get);
NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_FUNCPTR_SIGNATURE_END(get);

bool network_protocol_redis_recv(
        void *network_channel_user_data,
        char* read_buffer_with_offset);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_PROTOCOL_REDIS_H
