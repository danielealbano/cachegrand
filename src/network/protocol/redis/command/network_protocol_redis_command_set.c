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
#include "xalloc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "support/io_uring/io_uring_support.h"
#include "network/protocol/redis/network_protocol_redis.h"

#define TAG "network_protocol_redis_command_get"

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_END(set) {
    // TODO: just an hack, the storage system is missing
    void* memptr_start, *memptr;
    memptr_start = memptr = xalloc_alloc(sizeof(size_t) + reader_context->arguments.list[2].length);

    *(size_t*)memptr = reader_context->arguments.list[2].length;
    memptr += sizeof(size_t);
    memcpy(memptr, reader_context->arguments.list[2].value, reader_context->arguments.list[2].length);

    bool res = hashtable_mcmp_op_set(
            hashtable,
            reader_context->arguments.list[1].value,
            reader_context->arguments.list[1].length,
            (uintptr_t)memptr_start);

    if (!res) {
        xalloc_free(memptr);
    }

    if (res) {
        send_buffer_start = protocol_redis_writer_write_blob_string(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            "OK",
            2);
    } else {
        send_buffer_start = protocol_redis_writer_write_simple_error_printf(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            "ERR set failed");
    }

    NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR()

    return send_buffer_start;
}
