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

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_END(set) {
    char send_buffer[64] = { 0 }, *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;
    void* key_ptr;
    bool res;

    send_buffer_length = sizeof(send_buffer);
    send_buffer_start = send_buffer;
    send_buffer_end = send_buffer_start + send_buffer_length;

    // Initialize the database entry
    storage_db_entry_index_t *entry_index = storage_db_entry_index_ring_buffer_new(db);
    if (!entry_index) {
        LOG_E(TAG, "[REDIS][SET] Critical error, unable to allocate index entry in memory");
        return false;
    }

    res = storage_db_entry_index_allocate_key_chunks(
            db,
            entry_index,
            reader_context->arguments.list[1].length);
    if (!res) {
        LOG_E(
                TAG,
                "[REDIS][SET] Critical error, unable to allocate database chunks for the key");
        storage_db_entry_index_free(entry_index);
        return false;
    }

    res = storage_db_entry_index_allocate_value_chunks(
            db,
            entry_index,
            reader_context->arguments.list[2].length);
    if (!res) {
        LOG_E(
                TAG,
                "[REDIS][SET] Critical error, unable to allocate database chunks for the value");
        storage_db_entry_index_free(entry_index);
        return false;
    }

    // Write the chunks for the key
    key_ptr = reader_context->arguments.list[1].value;
    for(storage_db_chunk_index_t chunk_index = 0; chunk_index < entry_index->key_chunks_count; chunk_index++) {
        storage_db_chunk_info_t *chunk_info = storage_db_entry_key_chunk_get(entry_index, chunk_index);
        res = storage_db_entry_chunk_write(
                chunk_info,
                key_ptr + (chunk_index * STORAGE_DB_CHUNK_MAX_SIZE));

        if (!res) {
            LOG_E(
                    TAG,
                    "[REDIS][SET] Critical error, unable to write key chunk <%u> at offset <%u> long <%u> bytes",
                    chunk_index,
                    chunk_info->chunk_offset,
                    chunk_info->chunk_length);

            storage_db_entry_index_free(entry_index);
            return false;
        }
    }

    // Build the chunks for the value
    void* value_ptr = reader_context->arguments.list[2].value;
    for(storage_db_chunk_index_t chunk_index = 0; chunk_index < entry_index->key_chunks_count; chunk_index++) {
        storage_db_chunk_info_t *chunk_info = storage_db_entry_value_chunk_get(entry_index, chunk_index);
        res = storage_db_entry_chunk_write(
                chunk_info,
                value_ptr + (chunk_index * STORAGE_DB_CHUNK_MAX_SIZE));

        if (!res) {
            LOG_E(
                    TAG,
                    "[REDIS][SET] Critical error, unable to write value chunk <%u> at offset <%u> long <%u> bytes",
                    chunk_index,
                    chunk_info->chunk_offset,
                    chunk_info->chunk_length);

            storage_db_entry_index_free(entry_index);
            return false;
        }
    }

    res = storage_db_set_entry_index(
            db,
            key_ptr,
            reader_context->arguments.list[1].length,
            entry_index);

    if (!res) {
        storage_db_entry_index_free(entry_index);
    }

    if (res) {
        send_buffer_start = protocol_redis_writer_write_blob_string(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            "OK",
            2);
    } else {
        send_buffer_start = protocol_redis_writer_write_simple_error_printf(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            "ERR set failed");
    }

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        slab_allocator_mem_free(send_buffer);
        return false;
    }

    if (network_send(
            channel,
            send_buffer,
            send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
        return false;
    }

    return true;
}
