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

#define TAG "module_redis_command_psetex"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(psetex) {
    bool return_res = false;
    bool key_and_value_owned = false;
    module_redis_command_psetex_context_t *context = connection_context->command.context;

    if (context->milliseconds.value <= 0) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR invalid expire time in 'psetex' command");
        goto end;
    }

    storage_db_expiry_time_ms_t expiry_time_ms =
            clock_realtime_coarse_int64_ms() + (int64_t)context->milliseconds.value;

    if (unlikely(!storage_db_op_set(
            connection_context->db,
            context->key.value.key,
            context->key.value.length,
            STORAGE_DB_ENTRY_INDEX_VALUE_TYPE_STRING,
            context->value.value.chunk_sequence,
            expiry_time_ms))) {
        return_res = module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR psetex failed");
        goto end;
    }

    key_and_value_owned = true;
    return_res = module_redis_connection_send_ok(connection_context);

end:
    if (likely(key_and_value_owned)) {
        // Mark both the key and the chunk_sequence as NULL as the storage db now owns them, we don't want them to be
        // automatically freed at the end of the execution, especially the key as the hashtable might not need to hold
        // a reference to it, it might have already been freed
        context->key.value.key = NULL;
        context->value.value.chunk_sequence = NULL;
    }

    return return_res;
}
