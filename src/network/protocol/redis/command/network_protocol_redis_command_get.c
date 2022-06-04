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
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "network/protocol/redis/network_protocol_redis.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "network_protocol_redis_command_get"

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_END(get) {
    char *send_buffer, *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;
    bool res;
    bool return_res = false;
    storage_db_entry_index_status_t old_status;
    storage_db_entry_index_t *entry_index = NULL;
    storage_db_chunk_info_t *chunk_info = NULL;

    send_buffer = slab_allocator_mem_alloc(STORAGE_DB_CHUNK_MAX_SIZE);
    send_buffer_length = STORAGE_DB_CHUNK_MAX_SIZE;
    send_buffer_start = send_buffer;
    send_buffer_end = send_buffer_start + send_buffer_length;

    entry_index = storage_db_get_entry_index(
            db,
            reader_context->arguments.list[1].value,
            reader_context->arguments.list[1].length);

    if (likely(entry_index)) {
        // Try to acquire a reader lock until it's successful or the entry index has been marked as deleted
        storage_db_entry_index_status_acquire_reader_lock(
                entry_index,
                &old_status);

        if (unlikely(!old_status.deleted)) {
            entry_index = NULL;
        }
    }

    if (likely(entry_index)) {
        // Prepend the blog start in the buffer which is never sent at the very beginning because there will always be
        // a chunk big enough to hold always at least the necessary protocol data and 1 database chunk.
        send_buffer_start = protocol_redis_writer_write_argument_blob_start(
                send_buffer_start,
                send_buffer_length,
                false,
                (int)entry_index->value_length);

        chunk_info = storage_db_entry_value_chunk_get(entry_index, 0);

        // Check if the first chunk fits into the buffer, will always be fails if there is more than one chunk
        if (chunk_info->chunk_length + (send_buffer_start - send_buffer) > send_buffer_length) {
            if (network_send(
                    channel,
                    send_buffer,
                    send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
                LOG_E(TAG, "[REDIS][GET] Critical error, unable to send argument blob start");

                goto fail;
            }

            send_buffer_start = send_buffer;
        }

        // Build the chunks for the value
        for(storage_db_chunk_index_t chunk_index = 0; chunk_index < entry_index->key_chunks_count; chunk_index++) {
            chunk_info = storage_db_entry_value_chunk_get(entry_index, chunk_index);
            res = storage_db_entry_chunk_read(
                    chunk_info,
                    send_buffer_start);

            if (!res) {
                LOG_E(
                        TAG,
                        "[REDIS][GET] Critical error, unable to read chunk <%u> at offset <%u> long <%u> bytes",
                        chunk_index,
                        chunk_info->chunk_offset,
                        chunk_info->chunk_length);

                goto fail;
            }

            send_buffer_start += chunk_info->chunk_length;

            // Check if it's the last chunk and if the the argument blob terminator (a \r\n) will fit in the current
            // buffer or not, if it's the case will skip the network send to avoid a fiber switch

            if (!(entry_index->value_chunks_count == chunk_index + 1 &&
                chunk_info->chunk_length + (send_buffer_start - send_buffer) + 2 > send_buffer_length)) {
                if (network_send(
                        channel,
                        send_buffer,
                        send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
                    LOG_E(
                            TAG,
                            "[REDIS][GET] Critical error, unable to send chunk <%u> at offset <%u> long <%u> bytes",
                            chunk_index,
                            chunk_info->chunk_offset,
                            chunk_info->chunk_length);

                    goto fail;
                }

                send_buffer_start = send_buffer;
            }
        }

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

            goto fail;
        }
    } else {
        if (protocol_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2) {
            send_buffer_start = protocol_redis_writer_write_blob_string_null(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start);
        } else {
            send_buffer_start = protocol_redis_writer_write_null(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start);
        }

        if (network_send(
                channel,
                send_buffer,
                send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
            LOG_E(
                    TAG,
                    "[REDIS][GET] Critical error, unable to send blob argument");

            goto fail;
        }
    }

    // return_res is set to false at the beginning and switched to true only at this stage, this helps to avoid code
    // duplication for the cleanup
    return_res = true;

fail:
    if (entry_index) {
        storage_db_entry_index_status_free_reader_lock(entry_index, NULL);
    }

    slab_allocator_mem_free(send_buffer);
    return return_res;
}
