#include <stdint.h>
#include <stdbool.h>
#include <strings.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log.h"

#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "worker/worker.h"
#include "support/io_uring/io_uring_support.h"

#include "network_protocol_redis.h"

#define TAG "network_protocol_redis"

bool network_protocol_redis_recv(
        void *user_data,
        char *read_buffer_with_offset) {
    long data_read_len;
    network_channel_user_data_t *network_channel_user_data = (network_channel_user_data_t*)user_data;
    network_protocol_redis_context_t *protocol_context = &(network_channel_user_data->protocol.data.redis);
    protocol_redis_reader_context_t *reader_context = protocol_context->context;


    // TODO: this function should be part of network_protocol_redis

    bool send_response = false;

    do {
        // Keep reading till there are data in the buffer
        do {
            data_read_len = protocol_redis_reader_read(
                    read_buffer_with_offset,
                    network_channel_user_data->recv_buffer.data_size,
                    reader_context);

            if (data_read_len == -1) {
                continue;
            }

            read_buffer_with_offset += data_read_len;
            network_channel_user_data->recv_buffer.data_offset += data_read_len;
            network_channel_user_data->recv_buffer.data_size -= data_read_len;

            if (reader_context->arguments.current.index == -1) {
                continue;
            }

            // TODO: handle arguments!

            if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_NOP) {
                if (!reader_context->arguments.list[0].all_read) {
                    continue;
                }

                char* command_str = reader_context->arguments.list[0].value;

                // TODO: properly implement a command map
                if (strncasecmp("QUIT", command_str, 4) == 0) {
                    LOG_D(TAG, "[RECV][REDIS] QUIT command");
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_QUIT;
                } else if (strncasecmp("GET", command_str, 3) == 0) {
                    LOG_D(TAG, "[RECV][REDIS] SET command");
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_GET;
                } else if (strncasecmp("SET", command_str, 3) == 0) {
                    LOG_D(TAG, "[RECV][REDIS] SET command");
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_SET;
                } else if (strncasecmp("PING", command_str, 4) == 0) {
                    LOG_D(TAG, "[RECV][REDIS] PING command");
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_PING;
                } else {
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN;
                }
            }

            if (protocol_context->command != NETWORK_PROTOCOL_REDIS_COMMAND_NOP) {
                size_t len;
                char* buffer_end = NULL;
                char* buffer_start = network_channel_user_data->send_buffer.data +
                        network_channel_user_data->send_buffer.data_size;
                size_t buffer_avail_length =
                        network_channel_user_data->send_buffer.length -
                        network_channel_user_data->send_buffer.data_size;

                if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN) {
                    buffer_end = protocol_redis_writer_write_simple_error(
                            buffer_start,
                            buffer_avail_length,
                            "ERR unknown command",
                            19);

                    if (buffer_end == NULL) {
                        LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                        return false;
                    }

                    send_response = true;
                }

                else if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_PING) {
                    buffer_end = protocol_redis_writer_write_blob_string(
                            buffer_start,
                            buffer_avail_length,
                            "PONG",
                            4);

                    if (buffer_end == NULL) {
                        LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                        return false;
                    }

                    send_response = true;
                } else {
                    // TODO: handle the other supported commands
                }

                if (send_response) {
                    assert(buffer_end != NULL);
                    len = buffer_end - buffer_start;
                    network_channel_user_data->send_buffer.data_size += len;
                }
            }
        } while(
                data_read_len != -1 &&
                network_channel_user_data->recv_buffer.data_size > 0 &&
                reader_context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

        // TODO: handle parser error
        if (data_read_len == -1) {
            // TODO: need to send back an error message
            return false;
        }

        if (reader_context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED) {
            protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_NOP;
            protocol_redis_reader_context_reset(reader_context);
        }
    } while(data_read_len > 0 && network_channel_user_data->recv_buffer.data_size > 0);

    if (send_response) {
        network_channel_user_data->data_to_send_pending = true;
    }

    return true;
}
