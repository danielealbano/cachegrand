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
#include <stdlib.h>
#include <errno.h>

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

#define TAG "module_redis_command_hello"

struct hello_response_item {
    char* key;
    protocol_redis_types_t value_type;
    union {
        const char* string;
        long number;
        struct {
            void* list;
            long count;
        } array;
    } value;
};
typedef struct hello_response_item hello_response_item_t;

struct hello_command_context {
    char error_message[200];
    bool has_error;
};
typedef struct hello_command_context hello_command_context_t;

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_BEGIN(hello) {
    protocol_context->command_context = slab_allocator_mem_alloc(sizeof(hello_command_context_t));
    ((hello_command_context_t*)protocol_context->command_context)->has_error = false;

    return true;
}

MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL(hello) {
    hello_command_context_t *hello_command_context = (hello_command_context_t*)protocol_context->command_context;

    if (argument_index == 0) {
        char *endptr = NULL;
        long version;

        version = strtol(chunk_data, &endptr, 10);

        // Validate the parameters
        if (errno == ERANGE || endptr != chunk_data + chunk_length) {
            hello_command_context->has_error = true;
            snprintf(
                    hello_command_context->error_message,
                    sizeof(hello_command_context->error_message) - 1,
                    "ERR Protocol version is not an integer or out of range");
        } else if (version < 2 || version > 3) {
            hello_command_context->has_error = true;
            snprintf(
                    hello_command_context->error_message,
                    sizeof(hello_command_context->error_message) - 1,
                    "NOPROTO unsupported protocol version");
        } else  {
            protocol_context->resp_version = version == 2
                                             ? PROTOCOL_REDIS_RESP_VERSION_2
                                             : PROTOCOL_REDIS_RESP_VERSION_3;
        }
    } else {
        if (!hello_command_context->has_error) {
            char *error_msg_format = "ERR Syntax error in HELLO option '%.*s'";
            hello_command_context->has_error = true;
            snprintf(
                    hello_command_context->error_message,
                    sizeof(hello_command_context->error_message) - 1,
                    error_msg_format,
                    sizeof(hello_command_context->error_message) - 1 - strlen(error_msg_format) + 4,
                    chunk_data);
        }
    }

    return true;
}

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(hello) {
    bool return_res = false;
    network_channel_buffer_data_t *send_buffer, *send_buffer_start, *send_buffer_end;

    hello_command_context_t *hello_command_context = (hello_command_context_t*)protocol_context->command_context;

    if (hello_command_context->has_error) {
        size_t slice_length = sizeof(hello_command_context->error_message) + 16;
        send_buffer = send_buffer_start = network_send_buffer_acquire_slice(channel, slice_length);
        if (send_buffer_start == NULL) {
            LOG_E(TAG, "Unable to acquire send buffer slice!");
            goto end;
        }

        send_buffer_start = protocol_redis_writer_write_simple_error(
                send_buffer_start,
                slice_length,
                hello_command_context->error_message,
                (int)strlen(hello_command_context->error_message));
        network_send_buffer_release_slice(
                channel,
                send_buffer_start ? send_buffer_start - send_buffer : 0);

        return_res = send_buffer_start != NULL;

        if (send_buffer_start == NULL) {
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        }

        goto end;
    }

    hello_response_item_t hello_responses[7] = {
            {
                    .key = "server",
                    .value_type = PROTOCOL_REDIS_TYPE_SIMPLE_STRING,
                    .value.string = CACHEGRAND_CMAKE_CONFIG_NAME
            },
            {
                    .key = "version",
                    .value_type = PROTOCOL_REDIS_TYPE_SIMPLE_STRING,
                    .value.string = CACHEGRAND_CMAKE_CONFIG_VERSION_GIT
            },
            {
                    .key = "proto",
                    .value_type = PROTOCOL_REDIS_TYPE_NUMBER,
                    .value.number = protocol_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2
                            ? 2
                            : 3
            },
            {
                    .key = "id",
                    .value_type = PROTOCOL_REDIS_TYPE_NUMBER,
                    .value.number = 0
            },
            {
                    .key = "mode",
                    .value_type = PROTOCOL_REDIS_TYPE_SIMPLE_STRING,
                    .value.string = "standalone"
            },
            {
                    .key = "role",
                    .value_type = PROTOCOL_REDIS_TYPE_SIMPLE_STRING,
                    .value.string = "master"
            },
            {
                    .key = "modules",
                    .value_type = PROTOCOL_REDIS_TYPE_ARRAY,
                    .value.array.list = NULL,
                    .value.array.count = 0
            },
    };

    size_t slice_length = 512;
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(channel, slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        goto end;
    }

    send_buffer_end = send_buffer + slice_length;

    if (protocol_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2) {
        send_buffer_start = protocol_redis_writer_write_array(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                sizeof(hello_responses) / sizeof(hello_response_item_t) * 2);
    } else {
        send_buffer_start = protocol_redis_writer_write_map(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                sizeof(hello_responses) / sizeof(hello_response_item_t));
    }

    if (send_buffer_start == NULL) {
        network_send_buffer_release_slice(channel, 0);
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    for(int i = 0; i < sizeof(hello_responses) / sizeof(hello_response_item_t); i++) {
        hello_response_item_t hello_response = hello_responses[i];
        send_buffer_start = protocol_redis_writer_write_blob_string(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                hello_response.key,
                (int)strlen(hello_response.key));

        if (send_buffer_start == NULL) {
            network_send_buffer_release_slice(channel, 0);
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }

        switch(hello_response.value_type) {
            case PROTOCOL_REDIS_TYPE_SIMPLE_STRING:
                send_buffer_start = protocol_redis_writer_write_blob_string(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        (char*)hello_response.value.string,
                        (int)strlen(hello_response.value.string));
                break;
            case PROTOCOL_REDIS_TYPE_NUMBER:
                send_buffer_start = protocol_redis_writer_write_number(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        hello_response.value.number);
                break;
            case PROTOCOL_REDIS_TYPE_ARRAY:
                // not implemented, will simply report an empty array with the count set to 0
                assert(hello_response.value.array.count == 0);

                send_buffer_start = protocol_redis_writer_write_array(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        hello_response.value.array.count);
                break;
            default:
                assert(0);
        }

        if (send_buffer_start == NULL) {
            network_send_buffer_release_slice(channel, 0);
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }
    }

    network_send_buffer_release_slice(
            channel,
            send_buffer_start - send_buffer);

    return_res = true;
end:

    return return_res;
}

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_FREE(hello) {
    if (!protocol_context->command_context) {
        return true;
    }

    slab_allocator_mem_free(protocol_context->command_context);
    protocol_context->command_context = NULL;

    return true;
}
