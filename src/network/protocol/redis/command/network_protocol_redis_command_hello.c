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
#include <stdlib.h>

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

#define TAG "network_protocol_redis_command_hello"

typedef struct hello_response_item hello_response_item_t;
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

NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_END(hello) {
    char *send_buffer, *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;
    bool invalid_protocol_error = false;

    send_buffer_length = 256;
    send_buffer = send_buffer_start = slab_allocator_mem_alloc(send_buffer_length);
    send_buffer_end = send_buffer_start + send_buffer_length;

    if (reader_context->arguments.count == 2) {
        // Covert the argument into a number
        long version = 0;
        char* endptr = NULL;

        version = strtol(reader_context->arguments.list[1].value, &endptr, 10);

        if (endptr == reader_context->arguments.list[1].value || version < 2 || version > 3) {
            invalid_protocol_error = true;
        } else {
            protocol_context->resp_version = version == 2
                    ? PROTOCOL_REDIS_RESP_VERSION_2
                    : PROTOCOL_REDIS_RESP_VERSION_3;
        }
    }

    if (invalid_protocol_error) {
        send_buffer_start = protocol_redis_writer_write_simple_error(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                "NOPROTO unsupported protocol version",
                36);

        if (send_buffer_start == NULL) {
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            slab_allocator_mem_free(send_buffer);
            return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_ERROR;
        }
    } else {
        if (protocol_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2) {
            send_buffer_start = protocol_redis_writer_write_array(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start,
                    14);
        } else {
            send_buffer_start = protocol_redis_writer_write_map(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start,
                    7);
        }

        if (send_buffer_start == NULL) {
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            slab_allocator_mem_free(send_buffer);
            return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_ERROR;
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

        for(int i = 0; i < sizeof(hello_responses) / sizeof(hello_response_item_t); i++) {
            hello_response_item_t hello_response = hello_responses[i];
            send_buffer_start = protocol_redis_writer_write_blob_string(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start,
                    hello_response.key,
                    strlen(hello_response.key));

            if (send_buffer_start == NULL) {
                LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
                slab_allocator_mem_free(send_buffer);
                return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_ERROR;
            }

            switch(hello_response.value_type) {
                case PROTOCOL_REDIS_TYPE_SIMPLE_STRING:
                    send_buffer_start = protocol_redis_writer_write_blob_string(
                            send_buffer_start,
                            send_buffer_end - send_buffer_start,
                            (char*)hello_response.value.string,
                            strlen(hello_response.value.string));
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
                LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
                slab_allocator_mem_free(send_buffer);
                return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_ERROR;
            }
        }
    }

    if (worker_network_send(
            channel,
            send_buffer,
            send_buffer_start - send_buffer) != NETWORK_OP_RESULT_OK) {
        return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_ERROR;
    }

    return NETWORK_PROTOCOL_REDIS_COMMAND_FUNCPTR_RETVAL_STOP_WAIT_SEND_DATA;
}
