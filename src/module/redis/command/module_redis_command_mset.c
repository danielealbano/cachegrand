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

#define TAG "module_redis_command_mset"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(mset) {
    module_redis_command_mset_context_t *context = connection_context->command.context;

    for(int index = 0; index < context->key_value.count; index++) {
        module_redis_command_mset_context_subargument_key_value_t *key_value = &context->key_value.list[index];

        if (!storage_db_op_set(
                connection_context->db,
                key_value->key.value.key,
                key_value->key.value.length,
                key_value->value.value.chunk_sequence,
                STORAGE_DB_ENTRY_NO_EXPIRY)) {
            return module_redis_connection_error_message_printf_noncritical(connection_context, "ERR mset failed");
        }

        // Mark both the key and the chunk_sequence as NULL as the storage db now owns them, we don't want them to be
        // automatically freed at the end of the execution, especially the key as the hashtable might not need to hold
        // a reference to it, it might have already been freed
        key_value->key.value.key = NULL;
        key_value->value.value.chunk_sequence = NULL;
    }

    return module_redis_connection_send_ok(connection_context);
}
