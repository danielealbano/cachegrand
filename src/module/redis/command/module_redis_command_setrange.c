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
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/fast_fixed_memory_allocator.h"
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

#define TAG "module_redis_command_setrange"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(setrange) {
    bool return_res = false;
    bool abort_rmw = true;
    bool release_transaction = false;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status = { 0 };
    storage_db_entry_index_t *current_entry_index = NULL;
    storage_db_chunk_sequence_t *destination_chunk_sequence = NULL, *current_chunk_sequence = NULL;
    char *padding_memory_zeroed = NULL;
    size_t chunk_sequence_required_length = 0, requested_total_length = 0, current_chunk_sequence_length = 0;

    module_redis_command_setrange_context_t *context = connection_context->command.context;

    if (context->offset.value < 0) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR offset is out of range");
        goto end;
    }

    // TODO: optimize setrange to use the get operation (which will not lock), create the new chunk sequence and then
    //       only at the end start the transaction to update the destination key otherwise the execution of the setrange
    //       command can impact the overall performances

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
                "ERR setrange failed");
        goto end;
    }

    if (likely(current_entry_index)) {
        current_entry_index = storage_db_op_rmw_current_entry_index_prep_for_read(
                connection_context->db,
                &rmw_status,
                current_entry_index);
    }

    if (likely(current_entry_index)) {
        current_chunk_sequence = current_entry_index->value;
        current_chunk_sequence_length = current_chunk_sequence->size;
    }

    requested_total_length = context->offset.value + context->value.value.chunk_sequence->size;
    chunk_sequence_required_length =  requested_total_length > current_chunk_sequence_length
            ? requested_total_length
            : current_chunk_sequence_length;

    if (unlikely((destination_chunk_sequence = storage_db_chunk_sequence_allocate(
            connection_context->db,
            chunk_sequence_required_length)) == NULL)) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR setrange failed");
        goto end;
    }

    // Copy the data from the original chunk sequence or the new one for the range using a 3-way scrolling window
    storage_db_chunk_index_t destination_chunk_index = 0, current_chunk_index = 0, range_value_chunk_index = 0;
    storage_db_chunk_info_t *destination_chunk_info = NULL, *current_chunk_info = NULL, *range_value_chunk_info = NULL;
    size_t destination_offset = 0, destination_chunk_offset = 0, range_value_chunk_offset = 0;

    destination_chunk_info = storage_db_chunk_sequence_get(
            destination_chunk_sequence,
            destination_chunk_index);
    current_chunk_info = storage_db_chunk_sequence_get(
            current_chunk_sequence,
            current_chunk_index);
    range_value_chunk_info = storage_db_chunk_sequence_get(
            context->value.value.chunk_sequence,
            range_value_chunk_index);

    do {
        assert(destination_chunk_info != NULL);

        size_t data_to_write_length;
        char *data_to_write_buffer = NULL;
        char *allocated_buffer = NULL;
        bool allocated_new_buffer = false;

        if (destination_offset >= context->offset.value && destination_offset < requested_total_length) {
            // This assert will be triggered if this branch will be accessed after all the data have been copied as
            // the last storage_db_chunk_sequence_get will return NULL;
            assert(range_value_chunk_info != NULL);

            size_t can_write_length = range_value_chunk_info->chunk_length - range_value_chunk_offset;
            size_t available_space_in_chunk_length = destination_chunk_info->chunk_length - destination_chunk_offset;

            if (unlikely((allocated_buffer = storage_db_get_chunk_data(
                    connection_context->db,
                    range_value_chunk_info,
                    &allocated_new_buffer)) == NULL)) {
                return_res = module_redis_connection_error_message_printf_noncritical(
                        connection_context,
                        "ERR setrange failed");
                goto end;
            }

            data_to_write_length = can_write_length > available_space_in_chunk_length
                    ? available_space_in_chunk_length
                    : can_write_length;
            data_to_write_buffer = allocated_buffer + range_value_chunk_offset;

            range_value_chunk_offset += data_to_write_length;

            assert(range_value_chunk_offset <= range_value_chunk_info->chunk_length);

            if (range_value_chunk_offset == range_value_chunk_info->chunk_length) {
                // Destination and current chunks are fetched always together as the data have to copied from there is
                // the current offset doesn't fall within the range of the data to copy
                range_value_chunk_index++;

                range_value_chunk_info = storage_db_chunk_sequence_get(
                        context->value.value.chunk_sequence,
                        range_value_chunk_index);

                range_value_chunk_offset = 0;
            }
        } else {
            // If source_chunk_info is NULL it means that current_chunk_info was selected and it's empty, it's not
            // necessary to try to identify again the source_chunk_info until the range requested to be set is reached.
            if (unlikely(current_chunk_info == NULL)) {
                // When the source is null, an empty chunk of memory has to be written
                if (unlikely(padding_memory_zeroed == NULL)) {
                    padding_memory_zeroed = fast_fixed_memory_allocator_mem_alloc_zero(STORAGE_DB_CHUNK_MAX_SIZE);
                }

                // Calculate how much has to be written, if current_chunk_info is NULL there will be nothing to write
                // after the value passed in setrange is set, so the logic can be fairly simple and can be checked only
                // with an assert via testing
                off_t needed_padding = context->offset.value - (off_t)destination_offset;
                assert(needed_padding >= 0);

                data_to_write_length = needed_padding > STORAGE_DB_CHUNK_MAX_SIZE
                                       ? STORAGE_DB_CHUNK_MAX_SIZE
                                       : needed_padding;

                data_to_write_buffer = padding_memory_zeroed;
            } else {
                size_t can_write_length = current_chunk_info->chunk_length - destination_chunk_offset;
                size_t destination_offset_after_write = destination_offset + can_write_length;

                if (destination_offset < requested_total_length &&
                    destination_offset_after_write > context->offset.value) {
                    can_write_length = context->offset.value - destination_offset;
                }

                if (unlikely((allocated_buffer = storage_db_get_chunk_data(
                        connection_context->db,
                        current_chunk_info,
                        &allocated_new_buffer)) == NULL)) {
                    return_res = module_redis_connection_error_message_printf_noncritical(
                            connection_context,
                            "ERR setrange failed");
                    goto end;
                }

                data_to_write_length = can_write_length;
                data_to_write_buffer = allocated_buffer + destination_chunk_offset;
            }
        }

        if (unlikely(!storage_db_chunk_write(
                connection_context->db,
                destination_chunk_info,
                destination_chunk_offset,
                data_to_write_buffer,
                data_to_write_length))) {
            if (unlikely(allocated_new_buffer)) {
                fast_fixed_memory_allocator_mem_free(allocated_buffer);
                allocated_buffer = NULL;
                allocated_new_buffer = false;
            }

            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR setrange failed");
            goto end;
        }

        if (unlikely(allocated_new_buffer)) {
            fast_fixed_memory_allocator_mem_free(allocated_buffer);
            allocated_buffer = NULL;
            allocated_new_buffer = false;
        }

        destination_offset += data_to_write_length;
        destination_chunk_offset += data_to_write_length;

        if (destination_chunk_offset == destination_chunk_info->chunk_length) {
            // Destination and current chunks are fetched always together as the data have to copied from there is
            // the current offset doesn't fall within the range of the data to copy
            destination_chunk_index++;
            current_chunk_index++;

            destination_chunk_info = storage_db_chunk_sequence_get(
                    destination_chunk_sequence,
                    destination_chunk_index);
            current_chunk_info = storage_db_chunk_sequence_get(
                    current_chunk_sequence,
                    current_chunk_index);

            destination_chunk_offset = 0;
        } else if (unlikely(current_chunk_info && destination_chunk_offset == current_chunk_info->chunk_length)) {
            // If this branch is hit, it means that the setrange is trying to write after the end of the existing data
            // so current_chunk_info will be basically set to NULL.
            current_chunk_index++;
            current_chunk_info = storage_db_chunk_sequence_get(
                    current_chunk_sequence,
                    current_chunk_index);
        }
    } while(destination_chunk_index < destination_chunk_sequence->count);

    if (unlikely(!storage_db_op_rmw_commit_update(
            connection_context->db,
            &rmw_status,
            destination_chunk_sequence,
            STORAGE_DB_ENTRY_NO_EXPIRY))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR setrange failed");
        goto end;
    }

    transaction_release(&transaction);

    context->key.value.key = NULL;
    abort_rmw = false;
    release_transaction = false;

    return_res = module_redis_connection_send_number(
            connection_context,
            (int64_t)destination_chunk_sequence->size);

    destination_chunk_sequence = NULL;

end:

    if (unlikely(current_entry_index && abort_rmw)) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    }

    if (unlikely(release_transaction)) {
        transaction_release(&transaction);
    }

    if (unlikely(current_entry_index)) {
        storage_db_entry_index_status_decrease_readers_counter(current_entry_index, NULL);
    }

    if (unlikely(destination_chunk_sequence)) {
        storage_db_chunk_sequence_free(connection_context->db, destination_chunk_sequence);
    }

    return return_res;
}
