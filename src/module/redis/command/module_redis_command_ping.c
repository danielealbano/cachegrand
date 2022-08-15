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

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
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
#include "fiber.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "module_redis_command_ping"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(ping) {
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;
    module_redis_command_ping_context_t *context = connection_context->command.context;
    char *string = context->message.value.short_string ? context->message.value.short_string : "PONG";
    int string_length = context->message.value.short_string ? (int)context->message.value.length : 4;

    // The string gets truncated if string_length + 32 > 64kb, not really great but no person should EVER use a string
    // so long for a ping as it would be devastating for performances so we don't really care and trucate it if needed
    string_length = string_length + 32 > 64 * 1024 ? (64 * 1024) - 32 : string_length;

    size_t slice_length = 32 + string_length;

    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        return false;
    }

    send_buffer_start = protocol_redis_writer_write_blob_string(
            send_buffer_start,
            slice_length,
            string,
            string_length);

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        return false;
    }

    return true;
}
