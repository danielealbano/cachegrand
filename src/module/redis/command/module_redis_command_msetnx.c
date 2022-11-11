/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_spsc/ring_bounded_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
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

#define TAG "module_redis_command_msetnx"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(msetnx) {
    int command_response = 0;
    bool error_found = false;
    bool return_res = false;
    bool can_commit_update = true;
    storage_db_op_rmw_status_t *rmw_statuses = NULL;
    transaction_t transaction = { 0 };
    uint32_t rmw_statuses_counter = 0;
    uint32_t rmw_statuses_processed_up_to = 0;
    module_redis_command_msetnx_context_t *context = connection_context->command.context;

    rmw_statuses = ffma_mem_alloc_zero(sizeof(storage_db_op_rmw_status_t) * context->key_value.count);

    transaction_acquire(&transaction);

    // First begin all the RMW on the passed keys checking if there is a value or not
    for (int index = 0; index < context->key_value.count; index++) {
        storage_db_entry_index_t *entry_index = NULL;
        module_redis_command_msetnx_context_subargument_key_value_t *key_value = &context->key_value.list[index];

        if (unlikely(!storage_db_op_rmw_begin(
                connection_context->db,
                &transaction,
                key_value->key.value.key,
                key_value->key.value.length,
                &rmw_statuses[index],
                &entry_index))) {
            return_res = module_redis_connection_error_message_printf_noncritical(connection_context, "ERR msetnx failed");
            can_commit_update = false;
            error_found = true;
            break;
        }

        rmw_statuses_counter++;

        // Check if the current entry index is NULL
        if (unlikely(entry_index != NULL)) {
            can_commit_update = false;
            break;
        }
    }

    // If the update can be committed loop over the keys again and carry out the update
    if (likely(can_commit_update)) {
        // It can always set the response to 1, in case of error it will not be taken in consideration avoiding an
        // useless branching
        command_response = 1;

        for (int index = 0; index < rmw_statuses_counter; index++) {
            module_redis_command_msetnx_context_subargument_key_value_t *key_value = &context->key_value.list[index];

            if (unlikely(!storage_db_op_rmw_commit_update(
                    connection_context->db,
                    &rmw_statuses[index],
                    STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STRING,
                    key_value->value.value.chunk_sequence,
                    STORAGE_DB_ENTRY_NO_EXPIRY))) {
                return_res = module_redis_connection_error_message_printf_noncritical(
                        connection_context,
                        "ERR msetnx failed");
                error_found = true;
                break;
            }

            // Mark both the key and the chunk_sequence as NULL as the storage db now owns them, we don't want them to be
            // automatically freed at the end of the execution, especially the key as the hashtable might not need to hold
            // a reference to it, it might have already been freed
            key_value->key.value.key = NULL;
            key_value->value.value.chunk_sequence = NULL;

            rmw_statuses_processed_up_to++;
        }
    }

    // Abort all the rmw operations that haven't been processed with a commit update which is always all if a value
    // already exist (as rmw_statuses_processed_up_to will be 0) or from rmw_statuses_processed_up_to onwards if the
    // commit update has failed
    for (uint32_t index = rmw_statuses_processed_up_to; index < rmw_statuses_counter; index++) {
        storage_db_op_rmw_abort(
                connection_context->db,
                &rmw_statuses[index]);
    }

    transaction_release(&transaction);

    if (likely(!error_found)) {
        return_res = module_redis_connection_send_number(connection_context, command_response);
    }

    return return_res;
}
