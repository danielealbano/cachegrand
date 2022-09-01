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

#define TAG "module_redis_command_mget"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(mget) {
    module_redis_command_mget_context_t *context = connection_context->command.context;

    if (unlikely(!module_redis_connection_send_array(connection_context, context->key.count))) {
        return false;
    }

    for(int index = 0; index < context->key.count; index++) {
        storage_db_entry_index_t *entry_index = storage_db_get_entry_index_for_read(
                connection_context->db,
                context->key.list[index].key,
                context->key.list[index].length);

        if (unlikely(!entry_index)) {
            if (!module_redis_connection_send_string_null(connection_context)) {
                return false;
            }
        } else {
            bool res = module_redis_command_stream_entry(
                    connection_context->network_channel,
                    connection_context->db,
                    entry_index);

            storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);

            if (!res) {
                return false;
            }
        }
    }

    return true;
}
