#ifndef CACHEGRAND_MODULE_REDIS_CONNECTION_H
#define CACHEGRAND_MODULE_REDIS_CONNECTION_H

#ifdef __cplusplus
extern "C" {
#endif

void module_redis_connection_context_init(
        module_redis_connection_context_t *connection_context,
        network_channel_t *network_channel,
        config_module_t *config_module);

void module_redis_connection_context_cleanup(
        module_redis_connection_context_t *connection_context);

void module_redis_connection_context_reset(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_reader_has_error(
        module_redis_connection_context_t *connection_context);

void module_redis_connection_set_error_message_from_reader(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_should_terminate_connection(
        module_redis_connection_context_t *connection_context);

void module_redis_connection_error_message_vprintf_internal(
        module_redis_connection_context_t *connection_context,
        bool override_previous_error,
        char *error_message,
        va_list args);

void module_redis_connection_error_message_printf_noncritical(
        module_redis_connection_context_t *connection_context,
        char *error_message,
        ...);

void module_redis_connection_error_message_printf_critical(
        module_redis_connection_context_t *connection_context,
        char *error_message,
        ...);

bool module_redis_connection_has_error(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_send_error(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_send_ok(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_flush_and_close(
        module_redis_connection_context_t *connection_context);

void module_redis_connection_try_free_command_context(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_command_too_long(
        module_redis_connection_context_t *connection_context);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_CONNECTION_H