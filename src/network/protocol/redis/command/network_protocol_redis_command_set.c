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
#include "xalloc.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_set.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "config.h"
#include "worker/worker_common.h"
#include "network/protocol/redis/network_protocol_redis.h"
#include "worker/network/worker_network.h"

#define TAG "network_protocol_redis_command_get"

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_END(set) {
    char *send_buffer, *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;

    // TODO: just an hack, the storage system is missing
    void* memptr_start, *memptr;
    memptr_start = memptr = slab_allocator_mem_alloc(
            sizeof(size_t) + reader_context->arguments.list[2].length);

    *(size_t*)memptr = reader_context->arguments.list[2].length;
    memptr += sizeof(size_t);
    memcpy(memptr, reader_context->arguments.list[2].value, reader_context->arguments.list[2].length);

    bool res = hashtable_mcmp_op_set(
            hashtable,
            reader_context->arguments.list[1].value,
            reader_context->arguments.list[1].length,
            (uintptr_t)memptr_start);

    if (!res) {
        slab_allocator_mem_free(memptr);
    }

    send_buffer_length = 64;
    send_buffer = send_buffer_start = slab_allocator_mem_alloc(send_buffer_length);
    send_buffer_end = send_buffer_start + send_buffer_length;

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

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        slab_allocator_mem_free(send_buffer);
        return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_ERROR;
    }

    if (worker_network_send(
            channel,
            send_buffer,
            send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
        return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_ERROR;
    }

    return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_STOP_WAIT_SEND_DATA;
}
