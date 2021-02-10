#ifndef CACHEGRAND_NETWORK_PROTOCOL_REDIS_H
#define CACHEGRAND_NETWORK_PROTOCOL_REDIS_H

#ifdef __cplusplus
extern "C" {
#endif

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

enum network_protocol_redis_named_arguments {
    NETWORK_PROTOCOL_REDIS_SUBCOMMAND_SET_AX,
    NETWORK_PROTOCOL_REDIS_SUBCOMMAND_SET_PX,
    NETWORK_PROTOCOL_REDIS_SUBCOMMAND_SET_NX
};
typedef enum network_protocol_redis_named_arguments network_protocol_redis_named_arguments_t;

typedef struct network_protocol_redis_command_map_named_argument network_protocol_redis_command_map_named_argument_t;
struct network_protocol_redis_command_map_named_argument {
    network_protocol_redis_named_arguments_t named_argument;
    size_t length;
    // Redis longest named argument is 10 chars
    char string[11];
};

typedef struct network_protocol_redis_command_map network_protocol_redis_command_map_t;
struct network_protocol_redis_command_map {
    network_protocol_redis_commands_t command;
    size_t length;
    // Redis longest command is 10 chars
    char string[11];
    uint8_t positional_arguments;
    int named_arguments_length;
    // Redis commands don't have more than 10 named arguments
    network_protocol_redis_command_map_named_argument_t named_arguments[10];
};

typedef struct network_protocol_redis_context network_protocol_redis_context_t;
struct network_protocol_redis_context {
    protocol_redis_reader_context_t* context;
    network_protocol_redis_commands_t command;
    bool skip_command;
};

bool network_protocol_redis_recv(
        void *network_channel_user_data,
        char* read_buffer_with_offset);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_PROTOCOL_REDIS_H
