/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "config.h"
#include "fiber.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "network/protocol/redis/network_protocol_redis.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "network_protocol_redis_command_get"

struct get_command_context {
    char error_message[200];
    bool has_error;
    storage_db_entry_index_t *entry_index;
    char *key;
    size_t key_length;
    size_t key_offset;
    bool key_copied_in_buffer;
};
typedef struct get_command_context get_command_context_t;

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_BEGIN(get) {
    protocol_context->command_context = slab_allocator_mem_alloc(sizeof(get_command_context_t));

    get_command_context_t *get_command_context = (get_command_context_t*)protocol_context->command_context;
    get_command_context->has_error = false;
    get_command_context->entry_index = NULL;

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM(get) {
    // All the arguments passed to del are keys
    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL(get) {
    // Require stream always returns true, the code should never get here
    assert(0);
    return false;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN(get) {
    get_command_context_t *get_command_context = (get_command_context_t*)protocol_context->command_context;

    if (get_command_context->has_error) {
        return true;
    }

    if (argument_index > 0) {
        get_command_context->has_error = true;
        snprintf(
                get_command_context->error_message,
                sizeof(get_command_context->error_message) - 1,
                "ERR wrong number of arguments for 'get' command");
        return true;
    }

    if (argument_length > NETWORK_PROTOCOL_REDIS_KEY_MAX_LENGTH) {
        get_command_context->has_error = true;
        snprintf(
                get_command_context->error_message,
                sizeof(get_command_context->error_message) - 1,
                "ERR The key has exceeded the allowed size of 64KB");
        return true;
    }

    get_command_context->key = NULL;
    get_command_context->key_copied_in_buffer = false;
    get_command_context->key_length = argument_length;
    get_command_context->key_offset = 0;

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA(get) {
    get_command_context_t *get_command_context = (get_command_context_t*)protocol_context->command_context;

    if (get_command_context->has_error) {
        return true;
    }

    // The scenario tested in this assert can't happen, but if it does happen it's a bug in the protocol parser and
    // there is no way to be sure that there is no corruption or data loss, so it's better to dramatically abort
    // (in debug mode)
    assert(get_command_context->key_offset + chunk_length <= get_command_context->key_length);

    if (!get_command_context->key_copied_in_buffer) {
        if (chunk_length != get_command_context->key_length) {
            get_command_context->key_copied_in_buffer = true;
            get_command_context->key = slab_allocator_mem_alloc(get_command_context->key_length);
        } else {
            // The argument stream end is ALWAYS processed right after the last argument stream data so if the size of the
            // data of the chunk matches the size of the key then it was received in one go and can be used directly without
            // copying it around
            get_command_context->key_copied_in_buffer = false;
            get_command_context->key = chunk_data;
        }
    }

    if (get_command_context->key_copied_in_buffer) {
        memcpy(get_command_context->key, chunk_data, chunk_length);
        get_command_context->key_offset += chunk_length;
    }

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END(get) {
    storage_db_entry_index_status_t old_status;
    get_command_context_t *get_command_context = (get_command_context_t*)protocol_context->command_context;

    if (get_command_context->has_error) {
        goto end;
    }

    storage_db_entry_index_t *entry_index = storage_db_get_entry_index(
            db,
            get_command_context->key,
            get_command_context->key_length);

    if (likely(entry_index)) {
        // Try to acquire a reader lock until it's successful or the entry index has been marked as deleted
        storage_db_entry_index_status_increase_readers_counter(
                entry_index,
                &old_status);

        if (unlikely(old_status.deleted)) {
            entry_index = NULL;
        }
    }

    get_command_context->entry_index = entry_index;

end:
    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_END(get) {
    bool res;
    char *send_buffer = NULL;
    bool return_res = false;
    storage_db_entry_index_t *entry_index = NULL;
    storage_db_chunk_info_t *chunk_info = NULL;

    get_command_context_t *get_command_context = (get_command_context_t*)protocol_context->command_context;
    entry_index = get_command_context->entry_index;

    if (get_command_context->has_error) {
        char error_send_buffer[256], *error_send_buffer_start, *error_send_buffer_end;
        size_t error_send_buffer_length;
        error_send_buffer_length = sizeof(error_send_buffer);
        error_send_buffer_start = error_send_buffer;
        error_send_buffer_end = error_send_buffer_start + error_send_buffer_length;

        error_send_buffer_start = protocol_redis_writer_write_simple_error(
                error_send_buffer_start,
                error_send_buffer_end - error_send_buffer_start,
                get_command_context->error_message,
                (int)strlen(get_command_context->error_message));

        if (error_send_buffer_start == NULL) {
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }

        if (network_send(
                channel,
                error_send_buffer,
                error_send_buffer_start - error_send_buffer) != NETWORK_OP_RESULT_OK) {
            goto end;
        }

        goto end;
    }

    if (likely(entry_index)) {
        // Check if the value is small enough to be contained in 1 single chunk and if it would fit in a memory single
        // memory allocation leaving enough space for the protocol begin and end signatures themselves
        if (entry_index->value_chunks_count == 1 && entry_index->value_length < SLAB_OBJECT_SIZE_MAX - 32) {
            char *send_buffer_start, *send_buffer_end;
            size_t send_buffer_length;

            send_buffer_length = min(entry_index->value_length + 32, STORAGE_DB_CHUNK_MAX_SIZE);
            send_buffer_start = send_buffer = slab_allocator_mem_alloc(send_buffer_length);
            send_buffer_end = send_buffer_start + send_buffer_length;

            // Prepend the blog start in the buffer which is never sent at the very beginning because there will always be
            // a chunk big enough to hold always at least the necessary protocol data and 1 database chunk.
            send_buffer_start = protocol_redis_writer_write_argument_blob_start(
                    send_buffer_start,
                    send_buffer_length,
                    false,
                    (int)entry_index->value_length);

            chunk_info = storage_db_entry_value_chunk_get(entry_index, 0);

            res = storage_db_entry_chunk_read(
                    db,
                    chunk_info,
                    send_buffer_start);

            if (!res) {
                LOG_E(
                        TAG,
                        "[REDIS][GET] Critical error, unable to read chunk <%u> long <%u> bytes",
                        0,
                        chunk_info->chunk_length);
                goto end;
            }

            send_buffer_start += chunk_info->chunk_length;

            // At this stage the entry_index is not accessed further therefore the readers counter can be decreased. The
            // entry_index has to be set to null to avoid that it's freed again at the end of the function
            storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);
            get_command_context->entry_index = entry_index = NULL;

            send_buffer_start = protocol_redis_writer_write_argument_blob_end(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start);

            if (network_send(
                    channel,
                    send_buffer,
                    send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
                LOG_E(
                        TAG,
                        "[REDIS][GET] Critical error, unable to send blob argument terminator");

                goto end;
            }
        } else {
            char protocol_send_buffer[64] = { 0 };
            size_t protocol_send_buffer_length = sizeof(protocol_send_buffer);
            char *protocol_send_buffer_start;

            protocol_send_buffer_start = protocol_redis_writer_write_argument_blob_start(
                    protocol_send_buffer,
                    protocol_send_buffer_length,
                    false,
                    (int)entry_index->value_length);

            if (network_send(
                    channel,
                    protocol_send_buffer,
                    protocol_send_buffer_start - protocol_send_buffer) != NETWORK_OP_RESULT_OK) {
                LOG_E(TAG, "[REDIS][GET] Critical error, unable to send argument blob start");

                goto end;
            }

            // Build the chunks for the value
            for(storage_db_chunk_index_t chunk_index = 0; chunk_index < entry_index->value_chunks_count; chunk_index++) {
                char *buffer_to_send;
                size_t buffer_to_send_length;

                chunk_info = storage_db_entry_value_chunk_get(entry_index, chunk_index);

                if (storage_db_entry_chunk_can_read_from_memory(db, chunk_info)) {
                    buffer_to_send = storage_db_entry_chunk_read_fast_from_memory(db, chunk_info);
                    buffer_to_send_length = chunk_info->chunk_length;
                } else {
                    if (send_buffer == NULL) {
                        send_buffer = slab_allocator_mem_alloc(STORAGE_DB_CHUNK_MAX_SIZE);
                    }

                    res = storage_db_entry_chunk_read(
                            db,
                            chunk_info,
                            send_buffer);

                    if (!res) {
                        LOG_E(
                                TAG,
                                "[REDIS][GET] Critical error, unable to read chunk <%u> long <%u> bytes",
                                chunk_index,
                                chunk_info->chunk_length);

                        goto end;
                    }

                    buffer_to_send = send_buffer;
                    buffer_to_send_length = chunk_info->chunk_length;
                }

                if (network_send(
                        channel,
                        buffer_to_send,
                        buffer_to_send_length) != NETWORK_OP_RESULT_OK) {
                    LOG_E(
                            TAG,
                            "[REDIS][GET] Critical error, unable to send chunk <%u> long <%u> bytes",
                            chunk_index,
                            chunk_info->chunk_length);

                    goto end;
                }
            }

            // At this stage the entry index is not accessed further therefore the readers counter can be decreased. The
            // entry_index has to be set to null to avoid that it's freed again at the end of the function
            storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);
            get_command_context->entry_index = entry_index = NULL;

            protocol_send_buffer_start = protocol_redis_writer_write_argument_blob_end(
                    protocol_send_buffer,
                    protocol_send_buffer_length);

            if (network_send(
                    channel,
                    protocol_send_buffer,
                    protocol_send_buffer_start - protocol_send_buffer) != NETWORK_OP_RESULT_OK) {
                LOG_E(TAG, "[REDIS][GET] Critical error, unable to send argument blob terminator");

                goto end;
            }
        }
    } else {
        char error_send_buffer[64] = { 0 }, *error_send_buffer_start, *error_send_buffer_end;
        size_t error_send_buffer_length;

        error_send_buffer_length = sizeof(error_send_buffer);
        error_send_buffer_start = error_send_buffer;
        error_send_buffer_end = error_send_buffer_start + error_send_buffer_length;

        if (protocol_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2) {
            error_send_buffer_start = protocol_redis_writer_write_blob_string_null(
                    error_send_buffer_start,
                    error_send_buffer_end - error_send_buffer_start);
        } else {
            error_send_buffer_start = protocol_redis_writer_write_null(
                    error_send_buffer_start,
                    error_send_buffer_end - error_send_buffer_start);
        }

        if (network_send(
                channel,
                error_send_buffer,
                error_send_buffer_start - error_send_buffer) != NETWORK_OP_RESULT_OK) {
            LOG_E(
                    TAG,
                    "[REDIS][GET] Critical error, unable to send blob argument");

            goto end;
        }
    }

    // return_res is set to false at the beginning and switched to true only at this stage, this helps to avoid code
    // duplication for the cleanup
    return_res = true;

end:
    if (send_buffer) {
        slab_allocator_mem_free(send_buffer);
    }

    return return_res;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_FREE(get) {
    if (!protocol_context->command_context) {
        return true;
    }

    get_command_context_t *get_command_context = (get_command_context_t*)protocol_context->command_context;

    if (get_command_context->entry_index) {
        storage_db_entry_index_status_decrease_readers_counter(get_command_context->entry_index, NULL);
        get_command_context->entry_index = NULL;
    }

    if (get_command_context->key_copied_in_buffer) {
        slab_allocator_mem_free(get_command_context->key);
        get_command_context->key_copied_in_buffer = false;
    }

    slab_allocator_mem_free(protocol_context->command_context);
    protocol_context->command_context = NULL;

    return true;
}
