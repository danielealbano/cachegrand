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
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
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

#define TAG "module_redis_command_persist"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(persist) {
    bool abort_rmw = false;
    bool metadata_updated = false;
    storage_db_entry_index_t *current_entry_index = NULL;
    transaction_t transaction = { 0 };
    storage_db_op_rmw_status_t rmw_status = { 0 };

    module_redis_command_expire_context_t *context = connection_context->command.context;

    transaction_acquire(&transaction);

    if (unlikely(!storage_db_op_rmw_begin(
            connection_context->db,
            &transaction,
            context->key.value.key,
            context->key.value.length,
            &rmw_status,
            &current_entry_index))) {
        transaction_release(&transaction);
        return module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR persist failed");
    }

    if (unlikely(!current_entry_index)) {
        goto end;
    }

    if (unlikely(current_entry_index->expiry_time_ms == STORAGE_DB_ENTRY_NO_EXPIRY)) {
        abort_rmw = true;
    }

    if (unlikely(abort_rmw)) {
        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    } else {
        current_entry_index->expiry_time_ms = STORAGE_DB_ENTRY_NO_EXPIRY;
        metadata_updated = true;
        storage_db_op_rmw_commit_metadata(connection_context->db, &rmw_status);

        context->key.value.key = NULL;
        context->key.value.length = 0;
    }

end:

    transaction_release(&transaction);

    return module_redis_connection_send_number(
            connection_context,
            metadata_updated ? 1 : 0);
}
