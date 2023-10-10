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
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_rwspinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
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
#include "network/network.h"
#include "worker/worker_op.h"
#include "helpers/module_redis_command_helper_save.h"

#define TAG "module_redis_command_bgsave"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(bgsave) {
    // Check if the snapshot is already running
    if (module_redis_command_helper_save_is_running(connection_context)) {
        return module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR A background save is already in progress");
    }

    module_redis_command_helper_save_request(connection_context);

    // Confirm the snapshot is complete
    module_redis_connection_send_ok(connection_context);
    if (network_flush_send_buffer(connection_context->network_channel) != NETWORK_OP_RESULT_OK) {
        return false;
    }

    return true;
}
