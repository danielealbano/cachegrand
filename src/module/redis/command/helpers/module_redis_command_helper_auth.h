#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_AUTH_H
#define CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_AUTH_H

#ifdef __cplusplus
extern "C" {
#endif

bool module_redis_command_helper_auth_try_positional_parameters(
        module_redis_connection_context_t *connection_context,
        char *parameter_position_1,
        size_t parameter_position_1_len,
        char *parameter_position_2,
        size_t parameter_position_2_len);

bool module_redis_command_helper_auth_client_trying_to_reauthenticate(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_helper_auth_error_failed(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_helper_auth_error_already_authenticated(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_helper_auth_error_not_authenticated(
        module_redis_connection_context_t *connection_context);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_COMMAND_HELPER_AUTH_H
