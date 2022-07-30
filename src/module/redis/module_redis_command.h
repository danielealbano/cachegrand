#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_H
#define CACHEGRAND_MODULE_REDIS_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif


bool module_redis_command_is_key_too_long(
        network_channel_t *channel,
        size_t key_length);

bool module_redis_command_process_begin(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_process_argument_require_stream(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index);

bool module_redis_command_process_argument_stream_begin(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index,
        uint32_t arguments_count);

bool module_redis_command_process_argument_stream_data(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index,
        char *chunk_data,
        size_t chunk_length);

bool module_redis_command_process_argument_stream_end(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_process_argument_full(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index,
        char *chunk_data,
        size_t chunk_length);

bool module_redis_command_process_end(
        module_redis_connection_context_t *connection_context);

#if DEBUG == 1
void module_redis_command_dump_argument(
        storage_db_t *db,
        uint32_t argument_index,
        module_redis_command_argument_t *argument,
        uintptr_t argument_context_base_addr,
        int depth);

void module_redis_command_dump_arguments(
        storage_db_t *db,
        module_redis_command_argument_t arguments[],
        int arguments_count,
        uintptr_t argument_context_base_addr,
        int depth);
#endif

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_REDIS_COMMAND_H
