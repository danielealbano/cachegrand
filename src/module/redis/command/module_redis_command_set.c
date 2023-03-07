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

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
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
#include "module/redis/module_redis_command.h"

#define TAG "module_redis_command_set"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(set) {
    bool use_rmw = false;
    bool abort_rmw = true;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status = { 0 };
    bool release_transaction = true;
    bool return_res = false;
    bool key_and_value_owned = false;
    bool previous_entry_index_prepped_for_read = false;
    storage_db_entry_index_t *previous_entry_index = NULL;
    module_redis_command_set_context_t *context = connection_context->command.context;

    storage_db_expiry_time_ms_t expiry_time_ms = STORAGE_DB_ENTRY_NO_EXPIRY;

    if (context->expiration.value.pxat_unix_time_milliseconds.has_token) {
        if (context->expiration.value.pxat_unix_time_milliseconds.value <= 0) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR invalid expire time in 'set' command");
            goto end;
        }

        expiry_time_ms = (int64_t)context->expiration.value.pxat_unix_time_milliseconds.value;
    } else if (context->expiration.value.exat_unix_time_seconds.has_token) {
        if (context->expiration.value.exat_unix_time_seconds.value <= 0) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR invalid expire time in 'set' command");
            goto end;
        }

        expiry_time_ms = (int64_t)context->expiration.value.exat_unix_time_seconds.value * 1000;
    } else if (context->expiration.value.px_milliseconds.has_token) {
        if (context->expiration.value.px_milliseconds.value <= 0) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR invalid expire time in 'set' command");
            goto end;
        }

        expiry_time_ms = clock_realtime_coarse_int64_ms() + (int64_t)context->expiration.value.px_milliseconds.value;
    } else if (context->expiration.value.ex_seconds.has_token) {
        if (context->expiration.value.ex_seconds.value <= 0) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR invalid expire time in 'set' command");
            goto end;
        }

        expiry_time_ms = clock_realtime_coarse_int64_ms() + ((int64_t)context->expiration.value.ex_seconds.value * 1000);
    }

    use_rmw =
            context->expiration.value.keepttl_keepttl.has_token ||
            context->condition.value.nx_nx.has_token ||
            context->condition.value.xx_xx.has_token ||
            context->get_get.has_token;

    if (likely(!use_rmw)) {
        if (unlikely(!storage_db_op_set(
                connection_context->db,
                context->key.value.key,
                context->key.value.length,
                STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STRING,
                context->value.value.chunk_sequence,
                expiry_time_ms))) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR set failed");
            goto end;
        }

        key_and_value_owned = true;
        return_res = module_redis_connection_send_ok(connection_context);
    } else {
        transaction_acquire(&transaction);

        if (unlikely(!storage_db_op_rmw_begin(
                connection_context->db,
                &transaction,
                context->key.value.key,
                context->key.value.length,
                &rmw_status,
                &previous_entry_index))) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR set failed");

            goto end;
        }

        // If the current value has to be returned, the entry index needs to be prepped for read
        if (unlikely(previous_entry_index && context->get_get.has_token)) {
            previous_entry_index_prepped_for_read = true;
            previous_entry_index = storage_db_op_rmw_current_entry_index_prep_for_read(
                    connection_context->db,
                    &rmw_status,
                    previous_entry_index);
        }

        // Checks if the operation has to be aborted because the NX flag is set but a value exists or because the XX
        // flag is set but a value doesn't exist
        abort_rmw =
                (context->condition.value.nx_nx.has_token || context->condition.value.xx_xx.has_token) && (
                        (context->condition.value.nx_nx.has_token && previous_entry_index) ||
                        (context->condition.value.xx_xx.has_token && !previous_entry_index)
                );

        if (unlikely(abort_rmw)) {
            storage_db_op_rmw_abort(connection_context->db, &rmw_status);
        } else {
            if (unlikely(previous_entry_index && context->expiration.value.keepttl_keepttl.has_token)) {
                expiry_time_ms = previous_entry_index->expiry_time_ms;
            }

            if (unlikely(!storage_db_op_rmw_commit_update(
                    connection_context->db,
                    &rmw_status,
                    STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STRING,
                    context->value.value.chunk_sequence,
                    expiry_time_ms))) {
                return_res = module_redis_connection_error_message_printf_noncritical(
                        connection_context,
                        "ERR set failed");
                goto end;
            }

            // Once the
            key_and_value_owned = true;
        }

        transaction_release(&transaction);
        release_transaction = false;

        // previous_entry_index might have been set to null by the return value of
        // storage_db_get_entry_index_prep_for_read if it had been deleted or if it's expired, it's necessary to check
        // again
        if (unlikely(context->get_get.has_token && previous_entry_index)) {
            return_res = module_redis_command_stream_entry(
                    connection_context->network_channel,
                    connection_context->db,
                    previous_entry_index);
        } else if (unlikely(abort_rmw || (context->get_get.has_token && !previous_entry_index))) {
            return_res = module_redis_connection_send_string_null(connection_context);
        } else {
            return_res = module_redis_connection_send_ok(connection_context);
        }
    }

end:

    if (use_rmw && release_transaction) {
        transaction_release(&transaction);
    }

    if (unlikely(previous_entry_index && previous_entry_index_prepped_for_read)) {
        storage_db_entry_index_status_decrease_readers_counter(previous_entry_index, NULL);
        previous_entry_index = NULL;
    }

    if (likely(key_and_value_owned)) {
        // Mark both the key and the chunk_sequence as NULL as the storage db now owns them, we don't want them to be
        // automatically freed at the end of the execution, especially the key as the hashtable might not need to hold
        // a reference to it, it might have already been freed
        context->key.value.key = NULL;
        context->value.value.chunk_sequence = NULL;
    }

    return return_res;
}
