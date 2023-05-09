#ifndef CACHEGRAND_MODULE_REDIS_CONNECTION_H
#define CACHEGRAND_MODULE_REDIS_CONNECTION_H

#ifdef __cplusplus
extern "C" {
#endif

void module_redis_connection_accept(
        network_channel_t *channel);

bool module_redis_connection_process_data(
        module_redis_connection_context_t *connection_context,
        network_channel_buffer_t *read_buffer);

void module_redis_connection_context_init(
        module_redis_connection_context_t *connection_context,
        storage_db_t *db,
        network_channel_t *network_channel);

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

bool module_redis_connection_error_message_vprintf_internal(
        module_redis_connection_context_t *connection_context,
        bool override_previous_error,
        char *error_message,
        va_list args);

__attribute__((format(printf, 2, 3)))
bool module_redis_connection_error_message_printf_noncritical(
        module_redis_connection_context_t *connection_context,
        char *error_message,
        ...);

__attribute__((format(printf, 2, 3)))
bool module_redis_connection_error_message_printf_critical(
        module_redis_connection_context_t *connection_context,
        char *error_message,
        ...);

bool module_redis_connection_has_error(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_send_error(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_send_number(
        module_redis_connection_context_t *connection_context,
        int64_t number);

bool module_redis_connection_send_array_header(
        module_redis_connection_context_t *connection_context,
        uint64_t array_length);

bool module_redis_connection_send_ok(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_send_string_null(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_send_blob_string(
        module_redis_connection_context_t *connection_context,
        char *string,
        size_t string_length);

bool module_redis_connection_send_simple_string(
        module_redis_connection_context_t *connection_context,
        char *string,
        size_t string_length);

bool module_redis_connection_send_array(
        module_redis_connection_context_t *connection_context,
        uint32_t count);

bool module_redis_connection_flush_and_close(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_command_too_long(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_authenticate(
        module_redis_connection_context_t *connection_context,
        char *client_username,
        size_t client_username_len,
        char *client_password,
        size_t client_password_len);

bool module_redis_connection_requires_authentication(
        module_redis_connection_context_t *connection_context);

bool module_redis_connection_is_authenticated(
        module_redis_connection_context_t *connection_context);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_CONNECTION_H
