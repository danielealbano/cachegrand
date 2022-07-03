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
#include "xalloc.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "fiber.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "network/protocol/redis/network_protocol_redis.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "network_protocol_redis_command_set"

struct set_command_context {
    char error_message[200];
    bool has_error;
    bool entry_index_saved;
    storage_db_entry_index_t *entry_index;
    storage_db_chunk_index_t chunk_index;
    off_t chunk_offset;
    char *key;
    size_t key_length;
    size_t key_offset;
};
typedef struct set_command_context set_command_context_t;

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_BEGIN(set) {
    // Initialize the database entry
    storage_db_entry_index_t *entry_index = storage_db_entry_index_ring_buffer_new(db);
    if (!entry_index) {
        LOG_E(TAG, "[REDIS][SET] Critical error, unable to allocate index entry in memory");
        return false;
    }

    protocol_context->command_context = slab_allocator_mem_alloc(sizeof(set_command_context_t));

    set_command_context_t *set_command_context = (set_command_context_t*)protocol_context->command_context;
    set_command_context->has_error = false;
    set_command_context->entry_index_saved = false;
    set_command_context->entry_index = entry_index;

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM(set) {
    // Only the two first arguments (key and value) require a stream, all the other ones doesn't and are expected to be
    // short and fit in the network buffer easily
    if (argument_index <= 1) {
        return true;
    }

    return false;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL(set) {
    if (argument_index <= 1) {
        // Should never happen, only argument_index >= 2 are non streamed
        assert(0);
        return false;
    }

    // TODO: add support to the SET optional arguments

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN(set) {
    set_command_context_t *set_command_context = (set_command_context_t*)protocol_context->command_context;

    if (set_command_context->has_error) {
        return true;
    }

    set_command_context->chunk_index = 0;
    set_command_context->chunk_offset = 0;

    // Check if it's the key
    if (argument_index == 0) {
        if (network_protocol_redis_is_key_too_long(channel, argument_length)) {
            set_command_context->has_error = true;
            snprintf(
                    set_command_context->error_message,
                    sizeof(set_command_context->error_message) - 1,
                    "ERR The key has exceeded the allowed size of <%u>",
                    channel->protocol_config->redis->max_key_length);

            return true;
        }

        set_command_context->key_length = argument_length;
        set_command_context->key_offset = 0;
        set_command_context->key = slab_allocator_mem_alloc(set_command_context->key_length);

        // If the backend is in memory it's not necessary to write the key to the storage because it will never be used as
        // the only case in which the keys are read from the storage is when the database gets loaded from the disk at the
        // startup
        if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
            bool res = storage_db_entry_index_allocate_key_chunks(
                    db,
                    set_command_context->entry_index,
                    argument_length);
            if (!res) {
                LOG_E(
                        TAG,
                        "[REDIS][SET] Critical error, unable to allocate database chunks for the key");
                return false;
            }
        }

    // Check if it's the value
    } else if (argument_index == 1) {
        bool res = storage_db_entry_index_allocate_value_chunks(
                db,
                set_command_context->entry_index,
                argument_length);
        if (!res) {
            LOG_E(
                    TAG,
                    "[REDIS][SET] Critical error, unable to allocate database chunks for the value");
            return false;
        }

    // All the other arguments will never be streamed
    } else {
        assert(0);
        return false;
    }

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA(set) {
    set_command_context_t *set_command_context = (set_command_context_t*)protocol_context->command_context;
    storage_db_entry_index_t *entry_index = set_command_context->entry_index;

    if (set_command_context->has_error) {
        return true;
    }

    if (argument_index == 0) {
        // The scenario tested in this assert can't happen, but if it does happen it's a bug in the protocol parser and
        // there is no way to be sure that there is no corruption or data loss, so it's better to dramatically abort
        // (in debug mode)
        assert(set_command_context->key_offset + chunk_length <= set_command_context->key_length);

        memcpy(set_command_context->key + set_command_context->key_offset, chunk_data, chunk_length);
        set_command_context->key_offset += chunk_length;

        // If the backend is in memory it's not necessary to write the key to the storage because it will never be used as
        // the only case in which the keys are read from the storage is when the database gets loaded from the disk at the
        // startup
        if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
            size_t written_data = 0;
            do {
                storage_db_chunk_info_t *chunk_info = storage_db_entry_key_chunk_get(
                        entry_index,
                        set_command_context->chunk_index);

                size_t chunk_available_size = chunk_info->chunk_length - set_command_context->chunk_offset;
                size_t chunk_data_to_write_length =
                        chunk_length > chunk_available_size ? chunk_available_size : chunk_length;

                bool res = storage_db_entry_chunk_write(
                        db,
                        chunk_info,
                        set_command_context->chunk_offset,
                        chunk_data,
                        chunk_data_to_write_length);

                if (!res) {
                    LOG_E(
                            TAG,
                            "[REDIS][SET] Critical error, unable to write key chunk <%u> long <%u> bytes",
                            set_command_context->chunk_index,
                            chunk_info->chunk_length);

                    storage_db_entry_index_free(db, entry_index);
                    return false;
                }

                written_data += chunk_data_to_write_length;
                set_command_context->chunk_offset += (off_t)chunk_data_to_write_length;

                if (set_command_context->chunk_offset == chunk_info->chunk_length) {
                    set_command_context->chunk_index++;
                }
            } while(written_data == chunk_length);
        }
    } else if (argument_index == 1) {
        size_t written_data = 0;
        do {
            storage_db_chunk_info_t *chunk_info = storage_db_entry_value_chunk_get(
                    entry_index,
                    set_command_context->chunk_index);

            size_t chunk_available_size = chunk_info->chunk_length - set_command_context->chunk_offset;
            size_t chunk_data_to_write_length =
                    chunk_length > chunk_available_size ? chunk_available_size : chunk_length;

            bool res = storage_db_entry_chunk_write(
                    db,
                    chunk_info,
                    set_command_context->chunk_offset,
                    chunk_data,
                    chunk_data_to_write_length);

            if (!res) {
                LOG_E(
                        TAG,
                        "[REDIS][SET] Critical error, unable to write value chunk <%u> long <%u> bytes",
                        set_command_context->chunk_index,
                        chunk_info->chunk_length);

                storage_db_entry_index_free(db, entry_index);
                return false;
            }

            written_data += chunk_data_to_write_length;
            set_command_context->chunk_offset += (off_t)chunk_data_to_write_length;

            if (set_command_context->chunk_offset == chunk_info->chunk_length) {
                set_command_context->chunk_index++;
            }
        } while(written_data < chunk_length);
    }

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END(set) {
    set_command_context_t *set_command_context = (set_command_context_t*)protocol_context->command_context;

    if (set_command_context->has_error) {
        return true;
    }

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_END(set) {
    char send_buffer[256] = { 0 }, *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;
    bool res;
    bool return_res = false;

    set_command_context_t *set_command_context = (set_command_context_t*)protocol_context->command_context;
    storage_db_entry_index_t *entry_index = set_command_context->entry_index;

    send_buffer_length = sizeof(send_buffer);
    send_buffer_start = send_buffer;
    send_buffer_end = send_buffer_start + send_buffer_length;

    if (set_command_context->has_error) {
        send_buffer_start = protocol_redis_writer_write_simple_error(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                set_command_context->error_message,
                (int)strlen(set_command_context->error_message));

        if (send_buffer_start == NULL) {
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }

        return_res = network_send(
                channel,
                send_buffer,
                send_buffer_start - send_buffer) == NETWORK_OP_RESULT_OK;

        goto end;
    }

    res = storage_db_set_entry_index(
            db,
            set_command_context->key,
            set_command_context->key_length,
            entry_index);

    if (!res) {
        storage_db_entry_index_free(db, entry_index);

        send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                "ERR set failed");
    } else  {
        set_command_context->entry_index_saved = true;

        send_buffer_start = protocol_redis_writer_write_simple_string(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            "OK",
            2);
    }

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    if (network_send(
            channel,
            send_buffer,
            send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
        goto end;
    }

    return_res = true;

end:

    return return_res;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_FREE(set) {
    if (!protocol_context->command_context) {
        return true;
    }

    set_command_context_t *set_command_context = (set_command_context_t*)protocol_context->command_context;

    if (!set_command_context->entry_index_saved) {
        storage_db_entry_index_free(db, set_command_context->entry_index);
        slab_allocator_mem_free(set_command_context->key);
        set_command_context->key = NULL;
    }

    slab_allocator_mem_free(protocol_context->command_context);
    protocol_context->command_context = NULL;

    return true;
}
