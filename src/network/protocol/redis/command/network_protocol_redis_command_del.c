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
#include <liburing.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "support/io_uring/io_uring_support.h"
#include "network/protocol/redis/network_protocol_redis.h"

#define TAG "network_protocol_redis_command_del"

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_END(del) {
    // TODO: just an hack, the storage system is missing
    uint32_t deleted_keys = 0;
    for(long i = 1; i < reader_context->arguments.count; i++) {
        bool deleted = hashtable_mcmp_op_delete(
                hashtable,
                reader_context->arguments.list[i].value,
                reader_context->arguments.list[i].length);

        if (deleted) {
            deleted_keys++;
        }
    }

    send_buffer_start = protocol_redis_writer_write_number(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            deleted_keys);
    NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR()

    return send_buffer_start;
}
