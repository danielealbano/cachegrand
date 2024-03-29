/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
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
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
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

#define TAG "module_redis_command_flushdb"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(flushdb) {
    module_redis_command_flushdb_context_t *context = connection_context->command.context;

    if (context->condition.value.async_async.has_token) {
        module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR the async flushdb is currently unsupported");
        goto end;
    }

    if (!storage_db_op_flush_sync(connection_context->db, connection_context->database_number)) {
        module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR Failed to flush the database");
        goto end;
    }

    module_redis_connection_send_ok(connection_context);

end:
    return true;
}
