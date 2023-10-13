/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
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
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
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

#define TAG "module_redis_command_copy"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(copy) {
    bool return_res = false;
    bool error_found = false;
    bool value_copied = false;
    bool abort_rmw = true;
    bool release_transaction = true;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status = { 0 };
    storage_db_entry_index_t *entry_index_source = NULL;
    storage_db_entry_index_t *entry_index_destination_current = NULL;
    storage_db_chunk_sequence_t *chunk_sequence_source = NULL;
    storage_db_chunk_sequence_t chunk_sequence_destination = { 0 };
    bool allocated_new_buffer = false;
    char *source_chunk_data = NULL;

    module_redis_command_copy_context_t *context = connection_context->command.context;

    if (unlikely(context->db_destination_db.has_token)) {
        error_found = true;
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR the DB parameter it not yet supported");
        goto end;
    }

    // TODO: optimize setrange to use the get operation (which will not lock), create the new chunk sequence and then
    //       only at the end start the transaction to update the destination key otherwise the execution of the setrange
    //       command can impact the overall performances

    transaction_acquire(&transaction);

    if (unlikely(!storage_db_op_rmw_begin(
            connection_context->db,
            &transaction,
            connection_context->database_number,
            context->destination.value.key,
            context->destination.value.length,
            &rmw_status,
            &entry_index_destination_current))) {
        error_found = true;
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR copy failed");
        goto end;
    }

    if (unlikely(!context->replace_replace.has_token && entry_index_destination_current != NULL)) {
        return_res = true;
        goto end;
    }

    entry_index_source = storage_db_get_entry_index_for_read(
            connection_context->db,
            connection_context->database_number,
            context->source.value.key,
            context->source.value.length);

    if (unlikely(!entry_index_source)) {
        return_res = true;
        goto end;
    }

    chunk_sequence_source = &entry_index_source->value;
    if (unlikely(!storage_db_chunk_sequence_allocate(
            connection_context->db,
            &chunk_sequence_destination,
            entry_index_source->value.size))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR copy failed");
        goto end;
    }

    off_t destination_chunk_offset = 0;
    storage_db_chunk_index_t destination_chunk_index = 0;
    storage_db_chunk_info_t *destination_chunk_info = storage_db_chunk_sequence_get(
            &chunk_sequence_destination,
            destination_chunk_index);
    for(storage_db_chunk_index_t source_chunk_index = 0; source_chunk_index < chunk_sequence_source->count; source_chunk_index++) {
        size_t source_chunk_written_data = 0;
        storage_db_chunk_info_t *source_chunk_info = storage_db_chunk_sequence_get(
                chunk_sequence_source,
                source_chunk_index);
        source_chunk_data = storage_db_get_chunk_data(
                connection_context->db,
                source_chunk_info,
                &allocated_new_buffer);
        if (unlikely(source_chunk_data == NULL)) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR copy failed");
            goto end;
        }

        do {
            assert(destination_chunk_info != NULL);

            size_t chunk_length_to_write = source_chunk_info->chunk_length - source_chunk_written_data;
            size_t chunk_available_size = destination_chunk_info->chunk_length - destination_chunk_offset;
            size_t chunk_data_to_write_length =
                    chunk_length_to_write > chunk_available_size ? chunk_available_size : chunk_length_to_write;

            // There should always be something to write
            assert(chunk_length_to_write > 0);
            assert(chunk_data_to_write_length > 0);

            bool res = storage_db_chunk_write(
                    connection_context->db,
                    destination_chunk_info,
                    destination_chunk_offset,
                    source_chunk_data + source_chunk_written_data,
                    chunk_data_to_write_length);

            if (unlikely(!res)) {
                LOG_E(
                        TAG,
                        "Unable to write value chunk <%u> long <%u> bytes",
                        source_chunk_index,
                        source_chunk_info->chunk_length);
                error_found = true;
                return_res = false;
                goto end;
            }

            source_chunk_written_data += chunk_data_to_write_length;
            destination_chunk_offset += (off_t)chunk_data_to_write_length;

            if (destination_chunk_offset == destination_chunk_info->chunk_length) {
                destination_chunk_index++;
                destination_chunk_offset = 0;
                destination_chunk_info = storage_db_chunk_sequence_get(
                        &chunk_sequence_destination,
                        destination_chunk_index);
            }
        } while(source_chunk_written_data < source_chunk_info->chunk_length);

        if (unlikely(allocated_new_buffer)) {
            xalloc_free(source_chunk_data);
            allocated_new_buffer = false;
        }
    }

    if (unlikely(!storage_db_op_rmw_commit_update(
            connection_context->db,
            &rmw_status,
            entry_index_source->value_type,
            &chunk_sequence_destination,
            STORAGE_DB_ENTRY_NO_EXPIRY))) {
        // entry_index_destination_new is freed by storage_db_op_rmw_commit_update if the operation fails
        error_found = true;
        chunk_sequence_destination.sequence = NULL;
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR copy failed");
        goto end;
    }

    transaction_release(&transaction);
    release_transaction = false;
    abort_rmw = false;
    value_copied = true;
    return_res = true;

    // The key is now owned by the storage db and the entry index is assigned
    context->destination.value.key = NULL;
    context->destination.value.length = 0;
    chunk_sequence_destination.sequence = NULL;

end:

    if (unlikely(abort_rmw)) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    }

    if (unlikely(release_transaction)) {
        transaction_release(&transaction);
    }

    if (unlikely(allocated_new_buffer)) {
        xalloc_free(source_chunk_data);
        allocated_new_buffer = false;
    }

    if (likely(entry_index_source)) {
        storage_db_entry_index_status_decrease_readers_counter(entry_index_source, NULL);
        entry_index_source = NULL;
    }

    if (unlikely(chunk_sequence_destination.sequence)) {
        storage_db_chunk_sequence_free_chunks(connection_context->db, &chunk_sequence_destination);
        chunk_sequence_destination.sequence = NULL;
    }

    if (unlikely(!error_found)) {
        module_redis_connection_send_number(connection_context, value_copied ? 1 : 0);
    }

    return return_res;
}
