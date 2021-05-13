#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <assert.h>
#include <stdlib.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "support/io_uring/io_uring_support.h"
#include "network/protocol/redis/network_protocol_redis.h"

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
    bool invalid_protocol_error = false;

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

        NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR()
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

        NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR()

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

            NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR()

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

            NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR()
        }
    }

    return send_buffer_start;
}
