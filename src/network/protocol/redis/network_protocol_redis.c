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

network_protocol_redis_command_info_t command_infos_map[] = {
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_ITEM(PING, "PING", ping, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_ITEM(QUIT, "QUIT", quit, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_ITEM(SET, "SET", set, 2),
        NETWORK_PROTOCOL_REDIS_COMMAND_INFO_MAP_ITEM(GET, "GET", get, 1),
};
uint32_t command_infos_map_count = sizeof(command_infos_map) / sizeof(network_protocol_redis_command_info_t);

NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_BEGIN(ping, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_ARGUMENT_PROCESSED(ping, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_END(ping, {
    NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR({
         send_buffer_start = protocol_redis_writer_write_blob_string(
                 send_buffer_start,
                 send_buffer_end - send_buffer_start,
                 "PONG",
                 4);
     })
})

NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_BEGIN(quit, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_ARGUMENT_PROCESSED(quit, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_END(quit, {
    // TODO: fake response - only for testing
    NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR({
        send_buffer_start = protocol_redis_writer_write_blob_string(
        send_buffer_start,
        send_buffer_end - send_buffer_start,
        "OK",
        2);
    })
})

NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_BEGIN(set, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_ARGUMENT_PROCESSED(set, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_END(set, {
    // TODO: fake response - only for testing
    NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR({
        send_buffer_start = protocol_redis_writer_write_blob_string(
        send_buffer_start,
        send_buffer_end - send_buffer_start,
        "OK",
        2);
    })
})

NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_BEGIN(get, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_ARGUMENT_PROCESSED(get, {})
NETWORK_PROTOCOL_REDIS_COMMAND_FUNC_END(get, {
    // TODO: fake response - only for testing
    NETWORK_PROTOCOL_REDIS_WRITE_ENSURE_NO_ERROR({
        send_buffer_start = protocol_redis_writer_write_blob_string(
            send_buffer_start,
            send_buffer_end - send_buffer_start,
            reader_context->arguments.list[1].value,
            reader_context->arguments.list[1].length);
    })
})

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
    protocol_redis_reader_context_t *reader_context = protocol_context->reader_context;

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

                size_t command_len = reader_context->arguments.list[0].length;
                char* command_str = reader_context->arguments.list[0].value;

                protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN;
                for(uint32_t command_index = 0; command_index < command_infos_map_count; command_index++) {
                    network_protocol_redis_command_info_t *command_info = &command_infos_map[command_index];
                    if (command_len == command_info->length && strncasecmp(command_str, command_info->string, command_len) == 0) {
                        LOG_D(
                                TAG,
                                "[RECV][REDIS] <%s> command received",
                                command_info->string);
                        protocol_context->command = command_info->command;
                        protocol_context->command_info = command_info;
                    }
                }

                if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN) {
                    protocol_context->skip_command = true;
                } else if (protocol_context->command_info->being_funcptr) {
                    if ((send_buffer_start = protocol_context->command_info->being_funcptr(
                            reader_context,
                            &protocol_context->command_context,
                            send_buffer_start,
                            send_buffer_end)) == NULL) {
                        LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                        return false;
                    }
                }
            }

            // TODO: need to handle stream mode

            if (
                    protocol_context->skip_command == false &&
                    protocol_context->command != NETWORK_PROTOCOL_REDIS_COMMAND_NOP &&
                    reader_context->arguments.current.index > 0) {
                uint32_t argument_index = reader_context->arguments.current.index;
                if (reader_context->arguments.list[argument_index].all_read) {
                    if ((send_buffer_start = protocol_context->command_info->argument_processed_funcptr(
                            reader_context,
                            &protocol_context->command_context,
                            send_buffer_start,
                            send_buffer_end,
                            argument_index)) == NULL) {
                        LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                        return false;
                    }
                }
            }

            // TODO: add an stream_data_to_storage to the reader_context and if it's true stream the data here and
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
            } else {
                if ((send_buffer_start = protocol_context->command_info->end_funcptr(
                        reader_context,
                        &protocol_context->command_context,
                        send_buffer_start,
                        send_buffer_end)) == NULL) {
                    LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                    return false;
                }
            }

            // Reset the reader_context to handle the next command in the buffer
            // TODO: move the reset code into its own function
            protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_NOP;
            protocol_context->command_info = NULL;
            memset(&protocol_context->command_context, 0, sizeof(protocol_context->command_context));
            protocol_context->skip_command = false;

            protocol_redis_reader_context_reset(reader_context);
        }
    } while(data_read_len > 0 && network_channel_user_data->recv_buffer.data_size > 0);

    // TODO: move the network channel user data update code into its own function, doesn't make sense to pass the entire
    //       network_channel to the recv & parse function, should just receive the recv_buffer and send_buffer and use
    //       some wrapper functions to update the status of the buffer
    if (send_buffer_start != send_buffer_base) {
        send_data_len = send_buffer_start - send_buffer_base;
        network_channel_user_data->send_buffer.data_size += send_data_len;
        network_channel_user_data->data_to_send_pending = true;
    }

    return true;
}
