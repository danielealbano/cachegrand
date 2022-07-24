/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_op_delete.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "modules/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "fiber.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "modules/redis/module_redis.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#define TAG "module_redis_command_del"

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

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_BEGIN(del) {
    protocol_context->command_context = slab_allocator_mem_alloc(sizeof(del_command_context_t));

    del_command_context_t *del_command_context = (del_command_context_t*)protocol_context->command_context;
    del_command_context->has_error = false;
    del_command_context->deleted_keys = 0;

    return true;
}

MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM(del) {
    // All the arguments passed to del are keys
    return true;
}

MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL(del) {
    // Require stream always returns true, the code should never get here
    assert(0);
    return false;
}

MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN(del) {
    del_command_context_t *del_command_context = (del_command_context_t*)protocol_context->command_context;

    if (del_command_context->has_error) {
        return true;
    }

    if (module_redis_is_key_too_long(channel, argument_length)) {
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

MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA(del) {
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

MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END(del) {
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

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(del) {
    bool return_res = false;
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;

    del_command_context_t *del_command_context = (del_command_context_t*)protocol_context->command_context;

    // Don't report errors if keys have been deleted, the client need to know that a number of keys have been deleted
    if (del_command_context->deleted_keys == 0 && del_command_context->has_error) {
        size_t slice_length = sizeof(del_command_context->error_message) + 16;
        send_buffer = send_buffer_start = network_send_buffer_acquire_slice(channel, slice_length);
        if (send_buffer_start == NULL) {
            LOG_E(TAG, "Unable to acquire send buffer slice!");
            goto end;
        }

        send_buffer_start = protocol_redis_writer_write_simple_error(
                send_buffer_start,
                slice_length,
                del_command_context->error_message,
                (int)strlen(del_command_context->error_message));
        network_send_buffer_release_slice(
                channel,
                send_buffer_start ? send_buffer_start - send_buffer : 0);

        return_res = send_buffer_start != NULL;

        if (send_buffer_start == NULL) {
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        }

        goto end;
    }

    size_t slice_length = 32;
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(channel, slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        goto end;
    }

    send_buffer_start = protocol_redis_writer_write_number(
            send_buffer_start,
            slice_length,
            del_command_context->deleted_keys);

    network_send_buffer_release_slice(
            channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    return_res = true;

end:

    return return_res;
}

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_FREE(del) {
    if (!protocol_context->command_context) {
        return true;
    }

    slab_allocator_mem_free(protocol_context->command_context);
    protocol_context->command_context = NULL;

    return true;
}
