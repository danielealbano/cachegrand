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

bool module_redis_command_process_argument_begin(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index,
        uint32_t argument_length);

bool module_redis_command_process_argument_require_stream(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index);

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

bool module_redis_command_process_argument_end(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index);

bool module_redis_command_process_end(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_process_try_free(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_acquire_slice_and_write_blob_start(
        network_channel_t *network_channel,
        size_t slice_length,
        size_t value_length,
        network_channel_buffer_data_t **send_buffer,
        network_channel_buffer_data_t **send_buffer_start,
        network_channel_buffer_data_t **send_buffer_end);

bool module_redis_command_stream_entry_with_one_chunk(
        network_channel_t *network_channel,
        storage_db_t *db,
        storage_db_entry_index_t *entry_index);

bool module_redis_command_stream_entry_with_multiple_chunks(
        network_channel_t *network_channel,
        storage_db_t *db,
        storage_db_entry_index_t *entry_index);

bool module_redis_command_stream_entry(
        network_channel_t *network_channel,
        storage_db_t *db,
        storage_db_entry_index_t *entry_index);

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
