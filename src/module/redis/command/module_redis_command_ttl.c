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
#include <math.h>
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

#define TAG "module_redis_command_ttl"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(ttl) {
    int64_t response = -2;
    storage_db_entry_index_t *entry_index = NULL;
    module_redis_command_ttl_context_t *context = connection_context->command.context;

    entry_index = storage_db_get_entry_index(
            connection_context->db,
            context->key.value.key,
            context->key.value.length);

    if (entry_index) {
        if (entry_index->expiry_time_ms > 0) {
            int64_t now = clock_realtime_coarse_int64_ms();
            response = (int64_t)ceilf((float)(entry_index->expiry_time_ms - now) / 1000.0f);
        } else {
            response = -1;
        }
    }

    return module_redis_connection_send_number(connection_context, response);
}
