/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
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
#include "data_structures/hashtable/mcmp/hashtable_op_get.h"
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
#include "module/redis/module_redis_command.h"
#include "module/redis/module_redis_connection.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "module_redis_command_get"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(get) {
    bool return_res = false;
    storage_db_entry_index_t *entry_index = NULL;
    module_redis_command_get_context_t *context = connection_context->command.context;

    entry_index = storage_db_get_entry_index_for_read(
            connection_context->db,
            context->key.value.key,
            context->key.value.length);

    if (unlikely(!entry_index)) {
        return_res = module_redis_connection_send_string_null(connection_context);
        goto end;
    }

    return_res = module_redis_command_stream_entry(
            connection_context->network_channel,
            connection_context->db,
            entry_index);

    storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);
    entry_index = NULL;

end:

    return return_res;
}
