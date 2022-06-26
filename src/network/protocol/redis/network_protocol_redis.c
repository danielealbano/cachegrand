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
#include "clock.h"
#include "utils_string.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "slab_allocator.h"
#include "config.h"
#include "fiber.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "network/network.h"

#include "network_protocol_redis.h"

#define TAG "network_protocol_redis"

network_protocol_redis_command_info_t command_infos_map[] = {
        NETWORK_PROTOCOL_REDIS_COMMAND_ONLY_END_FUNCPTR(HELLO, "HELLO", hello, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_ONLY_END_FUNCPTR(PING, "PING", ping, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_ONLY_END_FUNCPTR(SHUTDOWN, "SHUTDOWN", shutdown, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_ONLY_END_FUNCPTR(QUIT, "QUIT", quit, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_ONLY_END_FUNCPTR(SET, "SET", set, 2),
        NETWORK_PROTOCOL_REDIS_COMMAND_ONLY_END_FUNCPTR(GET, "GET", get, 1),
        NETWORK_PROTOCOL_REDIS_COMMAND_ONLY_END_FUNCPTR(DEL, "DEL", del, 1),
};
uint32_t command_infos_map_count = sizeof(command_infos_map) / sizeof(network_protocol_redis_command_info_t);

// TODO: when processing the arguments, the in-loop command handling has to handle the arguments that require to be
//       streamed somewhere (ie. the value argument of the SET command has to be written somewhere)

typedef struct network_protocol_redis_client network_protocol_redis_client_t;
struct network_protocol_redis_client {
    network_channel_buffer_t read_buffer;
    network_channel_buffer_t send_buffer;
};

void network_protocol_redis_client_new(network_protocol_redis_client_t *network_protocol_redis_client) {
    // To speed up the performances the code takes advantage of SIMD operations that are built to operate on
    // specific amount of data, for example AVX/AVX2 in general operate on 256 bit (32 byte) of data at time.
    // Therefore, to avoid implementing ad hoc checks everywhere and at the same time to ensure that the code will
    // never ever read over the boundary of the allocated block of memory, the length of the read buffer will be
    // initialized to the buffer receive size minus 32.
    // TODO: 32 should be defined as magic const somewhere as it's going hard to track where "32" is in use if it has to
    //       be changed
    // TODO: create a test to ensure that the length of the read buffer is taking into account the 32 bytes of padding
    network_protocol_redis_client->read_buffer.data = (char *)slab_allocator_mem_alloc_zero(NETWORK_CHANNEL_RECV_BUFFER_SIZE);
    network_protocol_redis_client->read_buffer.length = NETWORK_CHANNEL_RECV_BUFFER_SIZE - 32;

    network_protocol_redis_client->send_buffer.data = (char *)slab_allocator_mem_alloc_zero(NETWORK_CHANNEL_SEND_BUFFER_SIZE);
    network_protocol_redis_client->send_buffer.length = NETWORK_CHANNEL_SEND_BUFFER_SIZE;
}

void network_protocol_redis_client_cleanup(network_protocol_redis_client_t *network_protocol_redis_client) {
    slab_allocator_mem_free(network_protocol_redis_client->read_buffer.data);
    slab_allocator_mem_free(network_protocol_redis_client->send_buffer.data);
}

void network_protocol_redis_accept(
        network_channel_t *channel) {
    bool exit_loop = false;
    network_protocol_redis_context_t protocol_context = { 0 };
    network_protocol_redis_client_t network_protocol_redis_client = { 0 };

    network_protocol_redis_client_new(&network_protocol_redis_client);

    do {
        if (!network_buffer_has_enough_space(
                &network_protocol_redis_client.read_buffer,
                NETWORK_CHANNEL_PACKET_SIZE)) {
            char *send_buffer, *send_buffer_start, *send_buffer_end;

            send_buffer = send_buffer_start = network_protocol_redis_client.send_buffer.data;
            send_buffer_end =
                    network_protocol_redis_client.send_buffer.data + network_protocol_redis_client.send_buffer.length;
            send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start,
                    "ERR command too long");

            if (send_buffer_start == NULL) {
                LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
            } else {
                network_send(
                        channel,
                        send_buffer,
                        send_buffer_start - send_buffer);

                network_close(channel, true);
            }

            exit_loop = true;
        }

        if (!exit_loop) {
            if (network_buffer_needs_rewind(
                    &network_protocol_redis_client.read_buffer,
                    NETWORK_CHANNEL_PACKET_SIZE)) {
                network_protocol_redis_read_buffer_rewind(
                        &network_protocol_redis_client.read_buffer,
                        &protocol_context);
                network_buffer_rewind(&network_protocol_redis_client.read_buffer);
            }

            exit_loop = network_receive(
                    channel,
                    &network_protocol_redis_client.read_buffer,
                    NETWORK_CHANNEL_PACKET_SIZE) != NETWORK_OP_RESULT_OK;
        }

        if (!exit_loop) {
            exit_loop = !network_protocol_redis_process_events(
                    channel,
                    &network_protocol_redis_client.read_buffer,
                    &protocol_context);
        }
    } while(!exit_loop);

    network_protocol_redis_client_cleanup(&network_protocol_redis_client);
}

bool network_protocol_redis_read_buffer_rewind(
        network_channel_buffer_t *read_buffer,
        network_protocol_redis_context_t *protocol_context) {
    if (protocol_context->reader_context.arguments.count == 0) {
        return true;
    }

    for(long index = 0; index <= protocol_context->reader_context.arguments.current.index; index++) {
        size_t data_received_length;
        protocol_redis_reader_context_argument_t *argument = &protocol_context->reader_context.arguments.list[index];
        if (argument->copied_from_buffer) {
            continue;
        }

        if (argument->all_read) {
            data_received_length = argument->length;
        } else {
            // If the command is not marked all_read it means that it's necessary to determine if it's a resp3 command
            // or not, resp3 commands provide the full length of the argument in advance and therefore argument->length
            // contains that.
            if (protocol_context->reader_context.protocol_type == PROTOCOL_REDIS_READER_PROTOCOL_TYPE_INLINE) {
                data_received_length = argument->length;
            } else {
                data_received_length = protocol_context->reader_context.arguments.resp_protocol.current.received_length;
            }
        }

        char* value = slab_allocator_mem_alloc(argument->length);
        if (!value) {
            return false;
        }

        memcpy(
                value,
                argument->value,
                data_received_length);

        argument->value = value;
        argument->copied_from_buffer = true;
    }

    return true;
}

void network_protocol_redis_reset_context(
        network_protocol_redis_context_t *protocol_context) {
    // Reset the reader_context to handle the next command in the buffer
    // TODO: move the reset code into its own function
    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_NOP;ss
    protocol_context->command_info = NULL;
    memset(&protocol_context->command_context, 0, sizeof(protocol_context->command_context));
    protocol_context->skip_command = false;

    protocol_redis_reader_context_reset(&protocol_context->reader_context);
}

bool network_protocol_redis_process_events(
        network_channel_t *channel,
        network_channel_buffer_t *read_buffer,
        network_protocol_redis_context_t *protocol_context) {
    size_t send_buffer_length;
    char *send_buffer, *send_buffer_start, *send_buffer_end;
    long data_read_len = 0;

    worker_context_t *worker_context = worker_context_get();
    storage_db_t *db = worker_context->db;
    protocol_redis_reader_context_t *reader_context = &protocol_context->reader_context;

    // TODO: need to handle stream mode, if stream mode is enabled data must not be copied to the value by
    //       protocol_redis_reader_read

    do {
        // Keep reading till there are data in the buffer
        do {
            if (read_buffer->data_size == 0) {
                data_read_len = 0;
                break;
            }

            data_read_len = protocol_redis_reader_read(
                    read_buffer->data + read_buffer->data_offset,
                    read_buffer->data_size,
                    reader_context);

            if (data_read_len == -1) {
                break;
            }

            read_buffer->data_offset += data_read_len;
            read_buffer->data_size -= data_read_len;

            if (reader_context->arguments.current.index == -1 || protocol_context->skip_command) {
                continue;
            }

            // Checks if the redis command has been identified, if not tries to identify the request
            if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_NOP) {
                // TODO: should check if the length of the first argument is greater than the length of the longest
                //       supported command, if it's the case the command is certainly not supported and can be skipped

                // Checks if the first argument has been entirely read, if not retries
                if (!reader_context->arguments.list[0].all_read) {
                    continue;
                }

                size_t command_len = reader_context->arguments.list[0].length;
                char* command_str = reader_context->arguments.list[0].value;

                // Set the current command to UNKNOWN
                protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN;

                // Search the command_str in the commands table
                for(uint32_t command_index = 0; command_index < command_infos_map_count; command_index++) {
                    network_protocol_redis_command_info_t *command_info = &command_infos_map[command_index];
                    if (command_len == command_info->length &&
                        strncasecmp(command_str, command_info->string, command_len) == 0) {
                        LOG_D(
                                TAG,
                                "[RECV][REDIS] <%s> command received",
                                command_info->string);
                        protocol_context->command = command_info->command;
                        protocol_context->command_info = command_info;

                        break;
                    }
                }

                // Check if the command has been found
                if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN) {
                    protocol_context->skip_command = true;
                } else if (protocol_context->command_info->begin_funcptr) {
                    // Invoke the being function callback if it has been set
                    if (!protocol_context->command_info->begin_funcptr(
                            channel,
                            db,
                            protocol_context,
                            reader_context,
                            &protocol_context->command_context)) {
                        LOG_D(TAG, "[RECV][REDIS] being function for command failed");
                        return false;
                    }
                }
            }

            // Check the command has to be processed, if the command has been identified and if any argument, apart from
            // the command, has been received. If not, continue the loop.
            if (protocol_context->skip_command == true ||
                protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_NOP &&
                reader_context->arguments.current.index == 0) {
                continue;
            }

            uint32_t argument_index = reader_context->arguments.current.index;

            if (reader_context->arguments.list[argument_index].all_read &&
                protocol_context->command_info->argument_processed_funcptr) {
                if (!protocol_context->command_info->argument_processed_funcptr(
                        channel,
                        db,
                        protocol_context,
                        reader_context,
                        &protocol_context->command_context,
                        argument_index)) {
                    LOG_D(TAG, "[RECV][REDIS] argument processed function for command failed");
                    return false;
                }
            }
        } while(read_buffer->data_size > 0 &&
                reader_context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

        if (data_read_len == -1) {
            send_buffer_length = 64;
            send_buffer = send_buffer_start = slab_allocator_mem_alloc(send_buffer_length);
            send_buffer_end = send_buffer_start + send_buffer_length;

            send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start,
                    "ERR parsing error <%d>",
                    reader_context->error);

            if (send_buffer_start == NULL) {
                LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                return false;
            }

            network_send(
                    channel,
                    send_buffer,
                    send_buffer_start - send_buffer);

            network_close(channel, true);

            return true;
        }

        if (reader_context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED) {
            if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN) {
                // In memory there are always at least 2 bytes after the arguments, the first byte after the command
                // can be converted into a null to insert it into the the error message
                *(reader_context->arguments.list[0].value + reader_context->arguments.list[0].length) = 0;

                send_buffer_length = 64 + reader_context->arguments.list[0].length;
                send_buffer = send_buffer_start = slab_allocator_mem_alloc(send_buffer_length);
                send_buffer_end = send_buffer_start + send_buffer_length;

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

                network_protocol_redis_reset_context(protocol_context);

                return network_send(
                        channel,
                        send_buffer,
                        send_buffer_start - send_buffer);
            } else if (protocol_context->command_info != NULL) {
                if (protocol_context->command_info->required_positional_arguments_count >
                    reader_context->arguments.count - 1) {
                    // In memory there are always at least 2 bytes after the arguments, the first byte after the command
                    // can be converted into a null to insert it into the the error message
                    *(reader_context->arguments.list[0].value + reader_context->arguments.list[0].length) = 0;

                    send_buffer_length = 64 + reader_context->arguments.list[0].length;
                    send_buffer = send_buffer_start = slab_allocator_mem_alloc(send_buffer_length);
                    send_buffer_end = send_buffer_start + send_buffer_length;

                    send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                            send_buffer_start,
                            send_buffer_end - send_buffer_start,
                            "ERR wrong number of arguments for '%s' command",
                            reader_context->arguments.list[0].value);

                    if (send_buffer_start == NULL) {
                        LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                        return false;
                    }

                    network_protocol_redis_reset_context(protocol_context);

                    return network_send(
                            channel,
                            send_buffer,
                            send_buffer_start - send_buffer);
                } else {
                    bool res = protocol_context->command_info->end_funcptr(
                            channel,
                            db,
                            protocol_context,
                            reader_context,
                            &protocol_context->command_context);
                    network_protocol_redis_reset_context(protocol_context);

                    if (!res) {
                        LOG_D(TAG, "[RECV][REDIS] argument processed function for command failed");
                        return false;
                    }
                }
            }
        }
    } while(data_read_len > 0 && read_buffer->data_size > 0);

    return true;
}
