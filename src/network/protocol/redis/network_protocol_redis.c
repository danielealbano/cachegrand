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

// TODO: need an hook to track when the buffer is copied around, the data need to be cloned onto memory
// TODO: when processing the arguments, the in-loop command handling has to handle the arguments that require to be
//       streamed somewhere (ie. the value argument of the SET command has to be written somewhere)

bool network_protocol_redis_recv(
        void *user_data,
        char *read_buffer_with_offset) {
    long data_read_len;
    size_t send_data_len;
    network_channel_user_data_t *network_channel_user_data = (network_channel_user_data_t*)user_data;
    network_protocol_redis_context_t *protocol_context = &(network_channel_user_data->protocol.data.redis);
    protocol_redis_reader_context_t *reader_context = protocol_context->context;

    char* send_buffer_base = network_channel_user_data->send_buffer.data;
    char* send_buffer_start = send_buffer_base + network_channel_user_data->send_buffer.data_size;
    char* send_buffer_end = send_buffer_base + network_channel_user_data->send_buffer.length;

    // TODO: need to handle data copy if the buffer has to be flushed to make room to new data

    do {
        // Keep reading till there are data in the buffer
        do {
            data_read_len = protocol_redis_reader_read(
                    read_buffer_with_offset,
                    network_channel_user_data->recv_buffer.data_size,
                    reader_context);

            if (data_read_len == -1) {
                break;
            }

            read_buffer_with_offset += data_read_len;
            network_channel_user_data->recv_buffer.data_offset += data_read_len;
            network_channel_user_data->recv_buffer.data_size -= data_read_len;

            if (reader_context->arguments.current.index == -1 || protocol_context->skip_command) {
                continue;
            }

            // TODO: handle arguments!

            if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_NOP) {
                if (!reader_context->arguments.list[0].all_read) {
                    continue;
                }

                int command_len = reader_context->arguments.list[0].length;
                char* command_str = reader_context->arguments.list[0].value;

                // TODO: properly implement a command map
                if (command_len == 4 && strncasecmp("QUIT", command_str, 4) == 0) {
                    LOG_D(TAG, "[RECV][REDIS] QUIT command");
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_QUIT;
                } else if (command_len == 3 && strncasecmp("GET", command_str, 3) == 0) {
                    LOG_D(TAG, "[RECV][REDIS] SET command");
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_GET;
                } else if (command_len == 3 && strncasecmp("SET", command_str, 3) == 0) {
                    LOG_D(TAG, "[RECV][REDIS] SET command");
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_SET;
                } else if (command_len == 4 && strncasecmp("PING", command_str, 4) == 0) {
                    LOG_D(TAG, "[RECV][REDIS] PING command");
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_PING;
                } else {
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN;
                }
            }

            if (protocol_context->command != NETWORK_PROTOCOL_REDIS_COMMAND_NOP) {
                if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN) {
                    protocol_context->skip_command = true;
                }

                else if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_SET) {
                    // TODO: check if the index of the parameter matching the value to stream it to the storage backend
                }
            }

            // TODO: add an stream_data_to_storage to the protocol_context and if it's true stream the data here and
            //       get the pointer to them if it's the first invocation
        } while(network_channel_user_data->recv_buffer.data_size > 0 &&
                reader_context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

        if (data_read_len == -1) {
            send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start,
                    "ERR parsing error <%d>",
                    reader_context->error);

            if (send_buffer_start == NULL) {
                LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                return false;
            }

            network_channel_user_data->close_connection_on_send = true;
            break;
        }

        if (reader_context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED) {
            if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN) {
                // In memory there are always at least 2 bytes after the arguments, the first byte after the command
                // can be converted into a null to insert it into the the error message
                *(reader_context->arguments.list[0].value + reader_context->arguments.list[0].length) = 0;

                send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        "ERR unknown command `%s` with `%d` args",
                        reader_context->arguments.list[0].value,
                        reader_context->arguments.count - 1);

                if (send_buffer_start == NULL) {
                    LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                    return false;
                }

                protocol_context->skip_command = true;
            }

            else if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_PING) {
                send_buffer_start = protocol_redis_writer_write_blob_string(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        "PONG",
                        4);

                if (send_buffer_start == NULL) {
                    LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                    return false;
                }
            }

            // Reset the protocol_context to handle the next command in the buffer
            protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_NOP;
            protocol_context->skip_command = false;

            protocol_redis_reader_context_reset(reader_context);
        }
    } while(data_read_len > 0 && network_channel_user_data->recv_buffer.data_size > 0);

    if (send_buffer_start != send_buffer_base) {
        send_data_len = send_buffer_start - send_buffer_base;
        network_channel_user_data->send_buffer.data_size += send_data_len;
        network_channel_user_data->data_to_send_pending = true;
    }

    return true;
}
