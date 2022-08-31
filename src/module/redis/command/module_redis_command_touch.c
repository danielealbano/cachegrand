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
#include <strings.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "fiber.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"
#include "module/redis/module_redis_command.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "module_redis_command_touch"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(touch) {
    int touched_keys_count = 0;
    storage_db_entry_index_t *current_entry_index = NULL;
    module_redis_command_touch_context_t *context = connection_context->command.context;
    storage_db_op_rmw_status_t rmw_status = { 0 };

    for(int index = 0; index < context->key.count; index++) {
        if (unlikely(!storage_db_op_rmw_begin(
                connection_context->db,
                context->key.list[index].key,
                context->key.list[index].length,
                &rmw_status,
                &current_entry_index))) {
            return module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR expire failed");
        }

        if (likely(current_entry_index)) {
            touched_keys_count++;
        }

        storage_db_op_rmw_abort(connection_context->db, &rmw_status);
    }

    return module_redis_connection_send_number(connection_context, touched_keys_count);
}
