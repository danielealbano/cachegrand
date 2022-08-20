#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_H
#define CACHEGRAND_MODULE_REDIS_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CACHEGRAND_MODULE_REDIS_COMMAND_DUMP_CONTEXT
#define CACHEGRAND_MODULE_REDIS_COMMAND_DUMP_CONTEXT 0
#endif

typedef struct module_redis_command_context_has_token_padding_detection
        module_redis_command_context_has_token_padding_detection_t;
struct module_redis_command_context_has_token_padding_detection {
    bool has_token;
    void *pointer;
};

bool module_redis_command_process_begin(
        module_redis_connection_context_t *connection_context);

bool module_redis_command_process_argument_begin(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_length);

bool module_redis_command_process_argument_stream_data(
        module_redis_connection_context_t *connection_context,
        char *chunk_data,
        size_t chunk_length);

bool module_redis_command_process_argument_full(
        module_redis_connection_context_t *connection_context,
        char *chunk_data,
        size_t chunk_length);

bool module_redis_command_process_argument_end(
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

static inline __attribute__((always_inline)) bool module_redis_command_process_end(
        module_redis_connection_context_t *connection_context) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;
    module_redis_command_argument_t *expected_argument = command_parser_context->current_argument.expected_argument;

    if (
            expected_argument &&
            expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BLOCK &&
            command_parser_context->current_argument.block_argument_index > 0) {
        if (expected_argument->is_positional) {
            return module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR wrong number of arguments for %s",
                    connection_context->command.info->string);
        } else {
            return module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR syntax error in %s option '%s'",
                    connection_context->command.info->string,
                    expected_argument->token);
        }
    }

    return connection_context->command.info->command_end_funcptr(
            connection_context);
}

static inline __attribute__((always_inline)) bool module_redis_command_process_try_free(
        module_redis_connection_context_t *connection_context) {
    if (connection_context->command.info == NULL || connection_context->command.context == NULL) {
        return true;
    }

#if CACHEGRAND_MODULE_REDIS_COMMAND_DUMP_CONTEXT == 1
    module_redis_command_dump_arguments(
            connection_context->db,
            connection_context->command.info->arguments,
            connection_context->command.info->arguments_count,
            (uintptr_t)connection_context->command.context,
            0);
#endif

    connection_context->command.info->command_free_funcptr(
            connection_context);

    connection_context->command.context = NULL;

    return true;
}

static inline __attribute__((always_inline)) bool module_redis_command_process_argument_stream_end(
        module_redis_connection_context_t *connection_context) {
    return true;
}

static inline __attribute__((always_inline)) void module_redis_command_context_has_token_set(
        module_redis_command_argument_t *argument,
        void *base_addr,
        bool has_token) {
    assert(argument->token);

    *(bool *)base_addr = has_token;
}

static inline __attribute__((always_inline)) bool module_redis_command_context_has_token_get(
        module_redis_command_argument_t *argument,
        void *base_addr) {
    assert(argument->token);

    return *(bool *)base_addr;
}

static inline __attribute__((always_inline)) size_t module_redis_command_get_context_has_token_padding_size() {
    COMPILE_TIME_ASSERT(offsetof(module_redis_command_context_has_token_padding_detection_t, has_token)==0);
    return offsetof(module_redis_command_context_has_token_padding_detection_t, pointer);
}

static inline __attribute__((always_inline)) void *module_redis_command_context_base_addr_skip_has_token(
        module_redis_command_argument_t *argument,
        void *base_addr) {
    if (argument->token != NULL) {
        base_addr += module_redis_command_get_context_has_token_padding_size();
    }

    return base_addr;
}

static inline __attribute__((always_inline)) int module_redis_command_context_list_get_count(
        module_redis_command_argument_t *argument,
        void *base_addr) {
    assert(argument->has_multiple_occurrences);

    base_addr = module_redis_command_context_base_addr_skip_has_token(argument, base_addr);

    return *(int *)(base_addr + sizeof(void *));
}

static inline __attribute__((always_inline)) void module_redis_command_context_list_set_count(
        module_redis_command_argument_t *argument,
        void *base_addr,
        int count) {
    assert(argument->has_multiple_occurrences);

    base_addr = module_redis_command_context_base_addr_skip_has_token(argument, base_addr);

    *(int *)(base_addr + sizeof(void *)) = count;
}

static inline __attribute__((always_inline)) void module_redis_command_context_list_set_list(
        module_redis_command_argument_t *argument,
        void *base_addr,
        void *list) {
    assert(argument->has_multiple_occurrences);

    base_addr = module_redis_command_context_base_addr_skip_has_token(argument, base_addr);

    void **list_ptr = (void**)base_addr;
    *list_ptr = list;
}

static inline __attribute__((always_inline)) void *module_redis_command_context_list_get_list(
        module_redis_command_argument_t *argument,
        void *base_addr) {
    assert(argument->has_multiple_occurrences);

    base_addr = module_redis_command_context_base_addr_skip_has_token(argument, base_addr);

    void **list_ptr = (void**)base_addr;
    base_addr = *list_ptr;

    return base_addr;
}

static inline __attribute__((always_inline)) void *module_redis_command_context_list_get_entry(
        module_redis_command_argument_t *argument,
        void *base_addr,
        uint16_t index) {
    assert(argument->has_multiple_occurrences);

    void *list = module_redis_command_context_list_get_list(argument, base_addr);
    void *list_entry = list + (index * argument->argument_context_member_size);

    return list_entry;
}

static inline __attribute__((always_inline)) bool module_redis_command_is_key_too_long(
        network_channel_t *channel,
        size_t key_length) {
    if (unlikely(key_length > channel->module_config->redis->max_key_length)) {
        return true;
    }

    return false;
}

static inline __attribute__((always_inline)) bool module_redis_command_process_argument_require_stream(
        module_redis_connection_context_t *connection_context) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;

    return command_parser_context->current_argument.require_stream;
}

static inline __attribute__((always_inline)) bool module_redis_command_stream_entry(
        network_channel_t *network_channel,
        storage_db_t *db,
        storage_db_entry_index_t *entry_index) {
    // Check if the value is small enough to be contained in 1 single chunk and if it would fit in a memory single
    // memory allocation leaving enough space for the protocol begin and end signatures themselves.
    // The 32 bytes extra are required for the protocol data
    if (entry_index->value->count == 1 && entry_index->value->size < NETWORK_CHANNEL_MAX_PACKET_SIZE) {
        return module_redis_command_stream_entry_with_one_chunk(
                network_channel,
                db,
                entry_index);
    } else {
        return module_redis_command_stream_entry_with_multiple_chunks(
                network_channel,
                db,
                entry_index);
    }
}

#if CACHEGRAND_MODULE_REDIS_COMMAND_DUMP_CONTEXT == 1
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
