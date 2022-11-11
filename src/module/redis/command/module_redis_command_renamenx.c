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
#include "module/redis/module_redis_command.h"

#define TAG "module_redis_command_renamenx"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(renamenx) {
    bool return_res = false;
    bool abort_rmw = true;
    bool release_transaction = true;
    transaction_t transaction = { 0 };
    bool rmw_status_source_started = false;
    bool rmw_status_destination_started = false;
    storage_db_entry_index_t *source_entry_index = NULL, *destination_entry_index = NULL;
    module_redis_command_renamenx_context_t *context = connection_context->command.context;
    storage_db_op_rmw_status_t rmw_status_source = { 0 }, rmw_status_destination = { 0 };

    if (context->key.value.length == context->newkey.value.length &&
        strncmp(
                context->key.value.key,
                context->newkey.value.key,
                MIN(context->key.value.length, context->newkey.value.length)) == 0) {
        return module_redis_connection_send_number(connection_context, 0);
    }

    transaction_acquire(&transaction);

    if (unlikely(!storage_db_op_rmw_begin(
            connection_context->db,
            &transaction,
            context->key.value.key,
            context->key.value.length,
            &rmw_status_source,
            &source_entry_index))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR renamenx failed");

        goto end;
    }

    rmw_status_source_started = true;

    if (unlikely(!source_entry_index)) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR no such key");

        goto end;
    }

    if (unlikely(!storage_db_op_rmw_begin(
            connection_context->db,
            &transaction,
            context->newkey.value.key,
            context->newkey.value.length,
            &rmw_status_destination,
            &destination_entry_index))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR renamenx failed");

        goto end;
    }

    if (unlikely(destination_entry_index)) {
        return_res = module_redis_connection_send_number(connection_context, 0);
        goto end;
    }

    rmw_status_destination_started = true;

    storage_db_op_rmw_commit_rename(
            connection_context->db,
            &rmw_status_source,
            &rmw_status_destination);
    transaction_release(&transaction);

    context->newkey.value.key = NULL;
    abort_rmw = false;
    release_transaction = false;
    return_res = module_redis_connection_send_number(connection_context, 1);

end:

    if (unlikely(abort_rmw)) {
        if (rmw_status_source_started) {
            storage_db_op_rmw_abort(connection_context->db, &rmw_status_source);
        }

        if (rmw_status_destination_started) {
            storage_db_op_rmw_abort(connection_context->db, &rmw_status_destination);
        }
    }

    if (unlikely(release_transaction)) {
        transaction_release(&transaction);
    }

    return return_res;
}
