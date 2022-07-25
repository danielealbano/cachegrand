/**
 * Copyright (C) 2018-2022 Vito Castellano
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
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "fiber.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "module_redis_command_mget"

//MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(mget) {
//    bool res;
//    bool return_res = false;
//    storage_db_entry_index_t *entry_index = NULL;
//
//    mget_command_context_t *mget_command_context = (mget_command_context_t*)protocol_context->command_context;
//
//    if (mget_command_context->has_error) {
//        char error_send_buffer[256], *error_send_buffer_start, *error_send_buffer_end;
//        size_t error_send_buffer_length;
//        error_send_buffer_length = sizeof(error_send_buffer);
//        error_send_buffer_start = error_send_buffer;
//        error_send_buffer_end = error_send_buffer_start + error_send_buffer_length;
//
//        error_send_buffer_start = protocol_redis_writer_write_simple_error(
//                error_send_buffer_start,
//                error_send_buffer_end - error_send_buffer_start,
//                mget_command_context->error_message,
//                (int)strlen(mget_command_context->error_message));
//
//        if (error_send_buffer_start == NULL) {
//            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
//            goto end;
//        }
//
//        return_res = network_send_buffered(
//                channel,
//                error_send_buffer,
//                error_send_buffer_start - error_send_buffer) == NETWORK_OP_RESULT_OK;
//
//        goto end;
//    }
//
//    char send_buffer[64], *send_buffer_start, *send_buffer_end;
//    size_t send_buffer_length;
//
//    send_buffer_length = sizeof(send_buffer);
//    send_buffer_start = send_buffer;
//    send_buffer_end = send_buffer_start + send_buffer_length;
//
//    send_buffer_start = protocol_redis_writer_write_array(
//            send_buffer_start,
//            send_buffer_end - send_buffer_start,
//            mget_command_context->keys_count);
//
//    if (send_buffer_start == NULL) {
//        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
//        goto end;
//    }
//
//    for(int key_index = 0; key_index < mget_command_context->keys_count; key_index++) {
//        storage_db_entry_index_status_t old_status;
//
//        entry_index = storage_db_get_entry_index(
//            db,
//            mget_command_context->keys[key_index]->key,
//            mget_command_context->keys[key_index]->key_length);
//
//        // Try to acquire a reader lock until it's successful or the entry index has been marked as deleted
//        if (likely(entry_index)) {
//            storage_db_entry_index_status_increase_readers_counter(
//                    entry_index,
//                    &old_status);
//
//            if (unlikely(old_status.deleted)) {
//                entry_index = NULL;
//            }
//        }
//
//        if (likely(entry_index)) {
//                send_buffer_start = protocol_redis_writer_write_argument_blob_start(
//                        send_buffer_start,
//                        send_buffer_length,
//                        false,
//                        (int)entry_index->value_length);
//
//                if (network_send_buffered(
//                        channel,
//                        send_buffer,
//                        send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
//                    goto end;
//                }
//
//                send_buffer_start = send_buffer;
//
//                // Build the chunks for the value
//                for(storage_db_chunk_index_t chunk_index = 0; chunk_index < entry_index->value.count; chunk_index++) {
//                    storage_db_chunk_info_t *chunk_info = storage_db_entry_value_chunk_get(entry_index, chunk_index);
//                    char *chunk_send_buffer = slab_allocator_mem_alloc(chunk_info->chunk_length);
//
//                    res = storage_db_entry_chunk_read(
//                            db,
//                            chunk_info,
//                            chunk_send_buffer);
//
//                    if (!res) {
//                        LOG_E(
//                                TAG,
//                                "[REDIS][MGET] Critical error, unable to read chunk <%u> long <%u> bytes",
//                                chunk_index,
//                                chunk_info->chunk_length);
//
//                        goto end;
//                    }
//
//                    if (network_send_buffered(
//                            channel,
//                            chunk_send_buffer,
//                            chunk_info->chunk_length) != NETWORK_OP_RESULT_OK) {
//                        slab_allocator_mem_free(chunk_send_buffer);
//                        goto end;
//                    }
//
//                    slab_allocator_mem_free(chunk_send_buffer);
//                }
//
//                // At this stage the entry index is not accessed further therefore the readers counter can be decreased. The
//                // entry_index has to be set to null to avoid that it's freed again at the end of the function
//                storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);
//                entry_index = NULL;
//
//                send_buffer_start = protocol_redis_writer_write_argument_blob_end(
//                        send_buffer_start,
//                        send_buffer_end - send_buffer_start);
//        } else {
//            if (protocol_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2) {
//                send_buffer_start = protocol_redis_writer_write_blob_string_null(
//                        send_buffer_start,
//                        send_buffer_end - send_buffer_start);
//            } else {
//                send_buffer_start = protocol_redis_writer_write_null(
//                        send_buffer_start,
//                        send_buffer_end - send_buffer_start);
//            }
//        }
//    }
//
//    if (network_send_buffered(
//            channel,
//            send_buffer,
//            send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
//        goto end;
//    }
//
//    // return_res is set to false at the beginning and switched to true only at this stage, this helps to avoid code
//    // duplication for the cleanup
//    return_res = true;
//
//end:
//    if (entry_index != NULL) {
//        storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);
//    }
//
//    return return_res;
//}

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(mget) {
    return false;
}
