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
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "fiber.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "network/protocol/redis/network_protocol_redis.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "network_protocol_redis_command_del"

struct del_command_context {
    char error_message[200];
    bool has_error;
    uint32_t deleted_keys;
    char *key;
    bool key_copied_in_buffer;
    size_t key_length;
    size_t key_offset;
};
typedef struct del_command_context del_command_context_t;

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_BEGIN(del) {
    protocol_context->command_context = slab_allocator_mem_alloc(sizeof(del_command_context_t));

    del_command_context_t *del_command_context = (del_command_context_t*)protocol_context->command_context;
    del_command_context->has_error = false;
    del_command_context->deleted_keys = 0;

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM(del) {
    // All the arguments passed to del are keys
    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL(del) {
    // Require stream always returns true, the code should never get here
    assert(0);
    return false;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN(del) {
    del_command_context_t *del_command_context = (del_command_context_t*)protocol_context->command_context;

    if (del_command_context->has_error) {
        return true;
    }

    if (network_protocol_redis_is_key_too_long(channel, argument_length)) {
        del_command_context->has_error = true;
        snprintf(
                del_command_context->error_message,
                sizeof(del_command_context->error_message) - 1,
                "ERR The key has exceeded the allowed size of <%u>",
                channel->protocol_config->redis->max_key_length);
        del_command_context->has_error = true;
        return true;
    }

    del_command_context->key = NULL;
    del_command_context->key_copied_in_buffer = false;
    del_command_context->key_length = argument_length;
    del_command_context->key_offset = 0;

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA(del) {
    del_command_context_t *del_command_context = (del_command_context_t*)protocol_context->command_context;

    if (del_command_context->has_error) {
        return true;
    }

    // The scenario tested in this assert can't happen, but if it does happen it's a bug in the protocol parser and
    // there is no way to be sure that there is no corruption or data loss, so it's better to dramatically abort
    // (in debug mode)
    assert(del_command_context->key_offset + chunk_length <= del_command_context->key_length);

    if (!del_command_context->key_copied_in_buffer) {
        if (chunk_length != del_command_context->key_length) {
            del_command_context->key_copied_in_buffer = true;
            del_command_context->key = slab_allocator_mem_alloc(del_command_context->key_length);
        } else {
            // The argument stream end is ALWAYS processed right after the last argument stream data so if the size of the
            // data of the chunk matches the size of the key then it was received in one go and can be used directly without
            // copying it around
            del_command_context->key_copied_in_buffer = false;
            del_command_context->key = chunk_data;
        }
    }

    if (del_command_context->key_copied_in_buffer) {
        memcpy(del_command_context->key, chunk_data, chunk_length);
        del_command_context->key_offset += chunk_length;
    }

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END(del) {
    del_command_context_t *del_command_context = (del_command_context_t*)protocol_context->command_context;

    if (del_command_context->has_error) {
        return true;
    }

    bool deleted = storage_db_delete_entry_index(
            db,
            del_command_context->key,
            del_command_context->key_length);

    if (deleted) {
        del_command_context->deleted_keys++;
    }

end:
    if (del_command_context->key_copied_in_buffer) {
        slab_allocator_mem_free(del_command_context->key);
        del_command_context->key_copied_in_buffer = false;
    }

    return true;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_END(del) {
    bool return_res = false;
    char send_buffer[256], *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;

    del_command_context_t *del_command_context = (del_command_context_t*)protocol_context->command_context;

    send_buffer_length = sizeof(send_buffer);
    send_buffer_start = send_buffer;
    send_buffer_end = send_buffer_start + send_buffer_length;

    if (del_command_context->has_error) {
        send_buffer_start = protocol_redis_writer_write_simple_error(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                del_command_context->error_message,
                (int)strlen(del_command_context->error_message));

        if (send_buffer_start == NULL) {
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }

        return_res = network_send(
                channel,
                send_buffer,
                send_buffer_start - send_buffer) == NETWORK_OP_RESULT_OK;

        goto end;
    }

    send_buffer_start = protocol_redis_writer_write_number(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            del_command_context->deleted_keys);

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        slab_allocator_mem_free(send_buffer);
        goto end;
    }

    if (network_send(
            channel,
            send_buffer,
            send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
        goto end;
    }

    return_res = true;

end:

    return return_res;
}

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_COMMAND_FREE(del) {
    if (!protocol_context->command_context) {
        return true;
    }

    slab_allocator_mem_free(protocol_context->command_context);
    protocol_context->command_context = NULL;

    return true;
}
