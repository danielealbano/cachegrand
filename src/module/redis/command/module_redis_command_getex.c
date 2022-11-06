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

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_spsc/ring_bounded_spsc.h"
#include "data_structures/double_linked_list/double_linked_list.h"
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

#define TAG "module_redis_command_getex"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(getex) {
    bool abort_rmw = true;
    bool release_transaction = true;
    bool return_res = false;
    storage_db_entry_index_t *current_entry_index = NULL;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status = { 0 };

    module_redis_command_getex_context_t *context = connection_context->command.context;

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
                "ERR getex failed");

        goto end;
    }

    if (likely(current_entry_index)) {
        current_entry_index = storage_db_op_rmw_current_entry_index_prep_for_read(
                connection_context->db,
                &rmw_status,
                current_entry_index);
    }

    if (unlikely(!current_entry_index)) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
        transaction_release(&transaction);
        return module_redis_connection_send_string_null(connection_context);
    }

    storage_db_expiry_time_ms_t expiry_time_ms = current_entry_index->expiry_time_ms;

    if (context->expiration.value.pxat_unix_time_milliseconds.has_token) {
        if (context->expiration.value.pxat_unix_time_milliseconds.value <= 0) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR invalid expire time in 'getex' command");
            goto end;
        }

        expiry_time_ms = (int64_t)context->expiration.value.pxat_unix_time_milliseconds.value;
    } else if (context->expiration.value.exat_unix_time_seconds.has_token) {
        if (context->expiration.value.exat_unix_time_seconds.value <= 0) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR invalid expire time in 'getex' command");
            goto end;
        }

        expiry_time_ms = (int64_t)context->expiration.value.exat_unix_time_seconds.value * 1000;
    } else if (context->expiration.value.px_milliseconds.has_token) {
        if (context->expiration.value.px_milliseconds.value <= 0) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR invalid expire time in 'getex' command");
            goto end;
        }

        expiry_time_ms = clock_realtime_coarse_int64_ms() + (int64_t)context->expiration.value.px_milliseconds.value;
    } else if (context->expiration.value.ex_seconds.has_token) {
        if (context->expiration.value.ex_seconds.value <= 0) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR invalid expire time in 'getex' command");
            goto end;
        }

        expiry_time_ms = clock_realtime_coarse_int64_ms() + ((int64_t)context->expiration.value.ex_seconds.value * 1000);
    } else if (context->expiration.value.persist_persist.has_token) {
        expiry_time_ms =  STORAGE_DB_ENTRY_NO_EXPIRY;
    }

    current_entry_index->expiry_time_ms = expiry_time_ms;

    storage_db_op_rmw_commit_metadata(connection_context->db, &rmw_status);
    transaction_release(&transaction);

    abort_rmw = false;
    release_transaction = false;

    context->key.value.key = NULL;
    context->key.value.length = 0;

    return_res = module_redis_command_stream_entry(
            connection_context->network_channel,
            connection_context->db,
            current_entry_index);

    storage_db_entry_index_status_decrease_readers_counter(current_entry_index, NULL);
    current_entry_index = NULL;

end:

    if (abort_rmw) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    }

    if (release_transaction) {
        transaction_release(&transaction);
    }

    return return_res;
}
