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

#define TAG "module_redis_command_getset"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(getset) {
    bool return_res = false;
    storage_db_entry_index_t *previous_entry_index = NULL;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status = { 0 };

    module_redis_command_getset_context_t *context = connection_context->command.context;

    transaction_acquire(&transaction);

    if (unlikely(!storage_db_op_rmw_begin(
            connection_context->db,
            &transaction,
            context->key.value.key,
            context->key.value.length,
            &rmw_status,
            &previous_entry_index))) {
        transaction_release(&transaction);
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR getset failed");

        goto end;
    }

    if (likely(previous_entry_index)) {
        previous_entry_index = storage_db_op_rmw_current_entry_index_prep_for_read(
                connection_context->db,
                &rmw_status,
                previous_entry_index);
    }

    if (unlikely(!storage_db_op_rmw_commit_update(
            connection_context->db,
            &rmw_status,
            context->value.value.chunk_sequence,
            STORAGE_DB_ENTRY_NO_EXPIRY))) {
        transaction_release(&transaction);
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR set failed");
        goto end;
    }

    transaction_release(&transaction);

    if (likely(previous_entry_index)) {
        return_res = module_redis_command_stream_entry(
                connection_context->network_channel,
                connection_context->db,
                previous_entry_index);
    } else {
        return_res = module_redis_connection_send_string_null(connection_context);
    }

end:

    if (likely(previous_entry_index)) {
        storage_db_entry_index_status_decrease_readers_counter(previous_entry_index, NULL);
        previous_entry_index = NULL;
    }

    context->key.value.key = NULL;
    context->value.value.chunk_sequence = NULL;

    return return_res;
}
