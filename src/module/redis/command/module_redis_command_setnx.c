/**
 * Copyright (C) 2018-2022 Vito Castellano
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
#include "data_structures/small_circular_queue/small_circular_queue.h"
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

#define TAG "module_redis_command_setnx"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(setnx) {
    bool return_res = false;
    bool abort_rmw = false;
    bool release_transaction = true;
    bool key_and_value_owned = false;
    bool previous_entry_index_prepped_for_read = false;
    storage_db_entry_index_t *previous_entry_index = NULL;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status = { 0 };
    storage_db_expiry_time_ms_t expiry_time_ms = STORAGE_DB_ENTRY_NO_EXPIRY;

    module_redis_command_setnx_context_t *context = connection_context->command.context;

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
                "ERR setnx failed");

        goto end;
    }

    // Checks if the operation has to be aborted because a value exists
    if (likely(previous_entry_index)) {
        abort_rmw = true;
    }

    if (unlikely(abort_rmw)) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    } else {
        if (unlikely(!storage_db_op_rmw_commit_update(
                connection_context->db,
                &rmw_status,
                context->value.value.chunk_sequence,
                expiry_time_ms))) {
            return_res = module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR setnx failed");
            goto end;
        }

        key_and_value_owned = true;
    }

    transaction_release(&transaction);
    release_transaction = false;

    return_res = module_redis_connection_send_number(connection_context, unlikely(abort_rmw) ? 0 : 1);

end:

    if (release_transaction) {
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
