/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"

#define TAG "module_redis_command_append"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(append) {
    bool return_res = false;
    bool abort_rmw = true;
    bool release_transaction = true;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status = { 0 };
    storage_db_entry_index_t *current_entry_index = NULL;
    size_t destination_chunk_sequence_length = 0;
    storage_db_chunk_sequence_t *destination_chunk_sequence = NULL;
    storage_db_chunk_info_t *destination_chunk_info = NULL;
    storage_db_chunk_offset_t destination_chunk_offset = 0;
    storage_db_chunk_index_t destination_chunk_index = 0;
    module_redis_command_append_context_t *context = connection_context->command.context;
    storage_db_chunk_sequence_t *chunk_sequences_to_splice[2] = { 0 };
    storage_db_expiry_time_ms_t expiry_time_ms = STORAGE_DB_ENTRY_NO_EXPIRY;
    bool allocated_new_buffer = false;
    char *source_buffer = NULL;

    transaction_acquire(&transaction);

    if (unlikely(!storage_db_op_rmw_begin(
            connection_context->db,
            &transaction,
            context->key.value.key,
            context->key.value.length,
            &rmw_status,
            &current_entry_index))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR append failed");

        goto end;
    }

    if (likely(current_entry_index)) {
        current_entry_index = storage_db_op_rmw_current_entry_index_prep_for_read(
                connection_context->db,
                &rmw_status,
                current_entry_index);
    }

    if (likely(current_entry_index)) {
        destination_chunk_sequence_length += current_entry_index->value->size;
        chunk_sequences_to_splice[0] = current_entry_index->value;
        expiry_time_ms = current_entry_index->expiry_time_ms;
    }

    chunk_sequences_to_splice[1] = context->value.value.chunk_sequence;
    destination_chunk_sequence_length += context->value.value.chunk_sequence->size;

    destination_chunk_sequence = storage_db_chunk_sequence_allocate(
            connection_context->db,
            destination_chunk_sequence_length);
    destination_chunk_info = storage_db_chunk_sequence_get(
            destination_chunk_sequence,
            destination_chunk_index);

    for(int index = 0; index < ARRAY_SIZE(chunk_sequences_to_splice); index++) {
        storage_db_chunk_sequence_t *chunk_sequence_to_splice = chunk_sequences_to_splice[index];
        if (unlikely(chunk_sequence_to_splice == NULL)) {
            continue;
        }

        for(storage_db_chunk_index_t chunk_index = 0; chunk_index < chunk_sequence_to_splice->count; chunk_index++) {
            storage_db_chunk_info_t *source_chunk_info = storage_db_chunk_sequence_get(
                    chunk_sequence_to_splice,
                    chunk_index);
            assert(source_chunk_info);

            source_buffer = storage_db_get_chunk_data(
                    connection_context->db,
                    source_chunk_info,
                    &allocated_new_buffer);
            if (unlikely(source_buffer == NULL)) {
                return_res = module_redis_connection_error_message_printf_noncritical(
                        connection_context,
                        "ERR setrange failed");
                goto end;
            }

            size_t buffer_length = source_chunk_info->chunk_length;
            size_t buffer_offset = 0;
            do {
                size_t available_space = destination_chunk_info->chunk_length - destination_chunk_offset;
                size_t data_to_write_length = available_space > buffer_length
                        ? buffer_length
                        : available_space;
                char *data_to_write_buffer = source_buffer + buffer_offset;

                if (unlikely(!storage_db_chunk_write(
                        connection_context->db,
                        destination_chunk_info,
                        destination_chunk_offset,
                        data_to_write_buffer,
                        data_to_write_length))) {
                    return_res = module_redis_connection_error_message_printf_noncritical(
                            connection_context,
                            "ERR setrange failed");
                    goto end;
                }

                destination_chunk_offset += data_to_write_length;
                buffer_offset += data_to_write_length;
                buffer_length -= data_to_write_length;

                if (destination_chunk_offset == destination_chunk_info->chunk_length) {
                    destination_chunk_index++;

                    destination_chunk_info = storage_db_chunk_sequence_get(
                            destination_chunk_sequence,
                            destination_chunk_index);

                    destination_chunk_offset = 0;
                }
            } while(buffer_length > 0);

            if (unlikely(allocated_new_buffer)) {
                ffma_mem_free(source_buffer);
                source_buffer = NULL;
                allocated_new_buffer = false;
            }
        }
    }

    if (unlikely(!storage_db_op_rmw_commit_update(
            connection_context->db,
            &rmw_status,
            destination_chunk_sequence,
            expiry_time_ms))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR append failed");
        goto end;
    }

    transaction_release(&transaction);
    release_transaction = false;

    context->key.value.key = NULL;
    abort_rmw = false;

    return_res = module_redis_connection_send_number(
            connection_context,
            (int64_t)destination_chunk_sequence->size);

    destination_chunk_sequence = NULL;

end:

    if (unlikely(allocated_new_buffer)) {
        ffma_mem_free(source_buffer);
        source_buffer = NULL;
        allocated_new_buffer = false;
    }

    if (unlikely(destination_chunk_sequence)) {
        storage_db_chunk_sequence_free(connection_context->db, destination_chunk_sequence);
        destination_chunk_sequence = NULL;
    }

    if (unlikely(abort_rmw)) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    }

    if (unlikely(release_transaction)) {
        transaction_release(&transaction);
    }

    return return_res;
}
