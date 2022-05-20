/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
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

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "config.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "network/protocol/redis/network_protocol_redis.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "network_protocol_redis_command_del"

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_END(del) {
    char send_buffer[256], *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;
    uint32_t deleted_keys = 0;

    send_buffer_length = sizeof(send_buffer);
    send_buffer_start = send_buffer;
    send_buffer_end = send_buffer_start + send_buffer_length;

    for(long i = 1; i < reader_context->arguments.count; i++) {
        bool deleted = storage_db_get(
                db,
                reader_context->arguments.list[1].value,
                reader_context->arguments.list[1].length);

        if (deleted) {
            deleted_keys++;
        }
    }

    send_buffer_start = protocol_redis_writer_write_number(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            deleted_keys);

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        slab_allocator_mem_free(send_buffer);
        return false;
    }

    if (network_send(
            channel,
            send_buffer,
            send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
        return false;
    }

    return true;
}
