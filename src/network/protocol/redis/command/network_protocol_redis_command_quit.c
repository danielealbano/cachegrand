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

#define TAG "network_protocol_redis_command_quit"

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_END(quit) {
    char *send_buffer, *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;

    send_buffer_length = 64;
    send_buffer = send_buffer_start = slab_allocator_mem_alloc(send_buffer_length);
    send_buffer_end = send_buffer_start + send_buffer_length;

    send_buffer_start = protocol_redis_writer_write_blob_string(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            "OK",
            2);

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

    worker_network_close(channel);

    return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_STOP_WAIT_SEND_DATA;
}
