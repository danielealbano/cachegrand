#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_HELLO_H
#define CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_HELLO_H

#ifdef __cplusplus
extern "C" {
#endif

struct module_redis_command_helper_hello_response_item {
    char* key;
    protocol_redis_types_t value_type;
    union {
        const char* string;
        long number;
        struct {
            void* list;
            long count;
        } array;
    } value;
};
typedef struct module_redis_command_helper_hello_response_item module_redis_command_helper_hello_response_item_t;

bool module_redis_command_helper_hello_client_authorized_to_invoke_command(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context);

bool module_redis_command_helper_hello_client_trying_to_reauthenticate(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context);

bool module_redis_command_helper_hello_has_valid_proto_version(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context);

void module_redis_command_helper_hello_try_fetch_proto_version(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context);

void module_redis_command_helper_hello_try_fetch_client_name(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context);

bool module_redis_command_helper_hello_send_error_invalid_proto_version(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_helper_hello_send_response(
        module_redis_connection_context_t *connection_context,
        module_redis_command_helper_hello_response_item_t *hello_items,
        size_t hello_items_count);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_HELLO_H
