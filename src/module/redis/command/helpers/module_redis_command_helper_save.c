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
#include <arpa/inet.h>
#include <stdlib.h>
#include <math.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "utils_string.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"
#include "worker/worker_op.h"

#include "module_redis_command_helper_save.h"

#define TAG "module_redis_command_helper_save"

bool module_redis_command_helper_save_is_running(
        module_redis_connection_context_t *connection_context) {
    MEMORY_FENCE_LOAD();
    return connection_context->db->snapshot.running;
}

uint64_t module_redis_command_helper_save_request(
        module_redis_connection_context_t *connection_context) {
    // Request the background save to start
    uint64_t now = clock_monotonic_int64_ms();
    connection_context->db->snapshot.next_run_time_ms = now - 1;
    MEMORY_FENCE_STORE();

    return now;
}

bool module_redis_command_helper_save_wait(
        module_redis_connection_context_t *connection_context,
        uint64_t start_time_ms) {
    // Wait for the snapshot to complete or fail, it simply check if next_run_time_ms changes
    do {
        // Wait 1 ms
        if (!worker_op_wait_ms(1)) {
            // If the wait fails something is really wrong with io_uring, terminate the connection
            return false;
        }

        MEMORY_FENCE_LOAD();
    } while (connection_context->db->snapshot.next_run_time_ms <= start_time_ms);

    return true;
}
