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
#include "network_protocol_redis_commands.h"

#define TAG "network_protocol_redis"

network_protocol_redis_command_info_t command_infos_map[] = {
        NETWORK_PROTOCOL_REDIS_COMMAND_NO_STREAM(HELLO, "HELLO", hello, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_NO_ARGS(PING, "PING", ping, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_NO_ARGS(SHUTDOWN, "SHUTDOWN", shutdown, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND_NO_ARGS(QUIT, "QUIT", quit, 0),
        NETWORK_PROTOCOL_REDIS_COMMAND(SET, "SET", set, 2),
        NETWORK_PROTOCOL_REDIS_COMMAND(GET, "GET", get, 1),
        NETWORK_PROTOCOL_REDIS_COMMAND(DEL, "DEL", del, 1),
};
uint32_t command_infos_map_count = sizeof(command_infos_map) / sizeof(network_protocol_redis_command_info_t);

typedef struct network_protocol_redis_client network_protocol_redis_client_t;
struct network_protocol_redis_client {
    network_channel_buffer_t read_buffer;
};

void network_protocol_redis_client_new(
        network_protocol_redis_client_t *network_protocol_redis_client,
        config_network_protocol_t *config_network_protocol) {
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
}

void network_protocol_redis_client_cleanup(
        network_protocol_redis_client_t *network_protocol_redis_client) {
    slab_allocator_mem_free(network_protocol_redis_client->read_buffer.data);
}

void network_protocol_redis_accept(
        network_channel_t *channel) {
    bool exit_loop = false;
    network_protocol_redis_context_t protocol_context = { 0 };
    network_protocol_redis_client_t network_protocol_redis_client = { 0 };

    protocol_context.resp_version = PROTOCOL_REDIS_RESP_VERSION_2;

    network_protocol_redis_client_new(
            &network_protocol_redis_client,
            channel->protocol_config);

    do {
        if (!network_buffer_has_enough_space(
                &network_protocol_redis_client.read_buffer,
                NETWORK_CHANNEL_PACKET_SIZE)) {
            char send_buffer[64], *send_buffer_start, *send_buffer_end;
            size_t send_buffer_length;

            send_buffer_length = sizeof(send_buffer);
            send_buffer_start = send_buffer;
            send_buffer_end = send_buffer_start + send_buffer_length;

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
            }

            exit_loop = true;
        }

        if (!exit_loop) {
            if (network_buffer_needs_rewind(
                    &network_protocol_redis_client.read_buffer,
                    NETWORK_CHANNEL_PACKET_SIZE)) {
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

void network_protocol_redis_reset_context(
        network_protocol_redis_context_t *protocol_context) {
    // Reset the reader_context to handle the next command in the buffer, the resp_version isn't touched as it's
    // to be known all along the connection lifecycle
    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_NOP;
    protocol_context->command_info = NULL;
    protocol_context->command_context  = NULL;
    protocol_context->skip_command = false;
    protocol_context->command_length = 0;

    protocol_redis_reader_context_reset(&protocol_context->reader_context);
}

bool network_protocol_redis_process_events(
        network_channel_t *channel,
        network_channel_buffer_t *read_buffer,
        network_protocol_redis_context_t *protocol_context) {
    char send_buffer[256], *send_buffer_start, *send_buffer_end;
    size_t send_buffer_length;
    int32_t ops_found;
    bool return_result = false;
    protocol_redis_reader_op_t ops[8] = { 0 };
    uint8_t ops_size = (sizeof(ops) / sizeof(protocol_redis_reader_op_t));

    send_buffer_length = sizeof(send_buffer);
    send_buffer_start = send_buffer;
    send_buffer_end = send_buffer_start + send_buffer_length;

    worker_context_t *worker_context = worker_context_get();
    storage_db_t *db = worker_context->db;
    protocol_redis_reader_context_t *reader_context = &protocol_context->reader_context;

    do {
        // Keep reading till there are data in the buffer
        do {
            if (read_buffer->data_size == 0) {
                break;
            }

            network_channel_buffer_data_t *read_buffer_data_start = read_buffer->data + read_buffer->data_offset;
            ops_found = protocol_redis_reader_read(
                    read_buffer_data_start,
                    read_buffer->data_size,
                    reader_context,
                    ops,
                    ops_size);

            if (reader_context->error != PROTOCOL_REDIS_READER_ERROR_OK) {
                break;
            }

            // ops_found has to be bigger than uint8_t because protocol_redis_reader_read must return -1 in case of
            // errors but otherwise it will always return values that are contained in an uint8_t
            for (uint8_t op_index = 0; op_index < (uint8_t)ops_found; op_index++) {
                protocol_redis_reader_op_t *op = &ops[op_index];

                read_buffer->data_offset += op->data_read_len;
                read_buffer->data_size -= op->data_read_len;
                protocol_context->command_length += op->data_read_len;

                if (protocol_context->command_length > channel->protocol_config->redis->max_command_length) {
                    send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                            send_buffer_start,
                            send_buffer_end - send_buffer_start,
                            "ERR the command length has exceeded <%u> bytes",
                            channel->protocol_config->redis->max_command_length);

                    if (send_buffer_start == NULL) {
                        LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                        goto end;
                    }

                    network_send(
                            channel,
                            send_buffer,
                            send_buffer_start - send_buffer);

                    network_close(channel, true);

                    goto end;
                }

                // protocol_redis_reader_read will at best parse one command till the end therefore ops will never
                // contain data from other commands and therefore it's not necessary to check for COMMAND_END in the
                // ops to reset the status of the protocol_context.
                // If the function will change the behaviour and will support parsing multiple commands then will be
                // essential to properly handle the COMMAND_END op to reset the protocol context
                if (protocol_context->skip_command) {
                    // The for loop can't be interrupted, has to continue till the end, read_buffer->data_* have to be
                    // updated
                    continue;
                }

                // If the end of the first argument has been found then check if it's a known command
                if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END && op->data.argument.index == 0) {
                    size_t command_length = op->data.argument.length;
                    char *command_data = read_buffer_data_start + op->data.argument.offset;

                    // Set the current command to UNKNOWN
                    protocol_context->command = NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN;

                    // Search the command_data in the commands table
                    for (uint32_t command_index = 0; command_index < command_infos_map_count; command_index++) {
                        network_protocol_redis_command_info_t *command_info = &command_infos_map[command_index];
                        if (command_length == command_info->length &&
                            strncasecmp(command_data, command_info->string, command_length) == 0) {
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
                        // Command unknown, mark it to be skipped
                        protocol_context->skip_command = true;

                        // In memory there are always at least 2 bytes after the arguments, the first byte after the
                        // command can be converted into a null to insert it into the error message
                        *(command_data + command_length) = 0;

                        send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                                send_buffer_start,
                                send_buffer_end - send_buffer_start,
                                "ERR unknown command `%s` with `%d` args",
                                command_data,
                                reader_context->arguments.count - 1);

                        if (send_buffer_start == NULL) {
                            LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                            goto end;
                        }

                        if (!network_send(
                                channel,
                                send_buffer,
                                send_buffer_start - send_buffer)) {
                            goto end;
                        }
                    } else {
                        if (protocol_context->command_info->required_positional_arguments_count >
                            reader_context->arguments.count - 1) {
                            protocol_context->skip_command = true;
                            continue;
                        }

                        if (protocol_context->command_info->command_begin_funcptr) {
                            // Invoke the being function callback if it has been set
                            if (!protocol_context->command_info->command_begin_funcptr(
                                    channel,
                                    db,
                                    protocol_context,
                                    reader_context)) {
                                LOG_D(TAG, "[RECV][REDIS] command callback <command begin> failed");
                                goto end;
                            }

                            // If a command has been identified it's possible to move to the next op
                            continue;
                        }
                    }
                }

                bool is_argument_op =
                        op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN ||
                        op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA ||
                        op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END;

                if (is_argument_op && op->data.argument.index > 0) {
                    bool require_stream = false;
                    if (protocol_context->command_info->argument_require_stream_funcptr) {
                        require_stream = protocol_context->command_info->argument_require_stream_funcptr(
                                channel,
                                db,
                                protocol_context,
                                reader_context,
                                op->data.argument.index - 1);
                    }

                    if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA) {
                        // If the require_stream flag is false, the argument_full callback will be called once all the data
                        // have been processed but to ensure that if the buffer gets rewind these data will not be lost
                        // the buffer pointer is moved back as well if there isn't another op or if op_index + 1 isn't an
                        // argument-end op.
                        bool last_op = op_index == (uint8_t) ops_found - 1;
                        bool op_followed_by_argument_end =
                                !last_op && (ops[op_index + 1].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END);

                        if (!require_stream && (last_op || !op_followed_by_argument_end)) {
                            read_buffer->data_offset -= op->data_read_len;
                            read_buffer->data_size += op->data_read_len;

                            // No need to continue the parsing
                            break;
                        }
                    }

                    if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN && require_stream) {
                        if (protocol_context->command_info->argument_stream_begin_funcptr) {
                            if (!protocol_context->command_info->argument_stream_begin_funcptr(
                                    channel,
                                    db,
                                    protocol_context,
                                    reader_context,
                                    op->data.argument.index - 1,
                                    op->data.argument.length)) {
                                LOG_D(TAG, "[RECV][REDIS] command callback <argument stream begin> failed");
                                goto end;
                            }
                        }
                    } else if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA && require_stream) {
                        if (protocol_context->command_info->argument_stream_data_funcptr) {
                            size_t chunk_length = op->data.argument.data_length;
                            char *chunk_data = read_buffer_data_start + op->data.argument.offset;

                            if (!protocol_context->command_info->argument_stream_data_funcptr(
                                    channel,
                                    db,
                                    protocol_context,
                                    reader_context,
                                    op->data.argument.index - 1,
                                    chunk_data,
                                    chunk_length)) {
                                LOG_D(TAG, "[RECV][REDIS] command callback <argument stream data> failed");
                                goto end;
                            }
                        }
                    } else if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END && require_stream) {
                        if (protocol_context->command_info->argument_stream_end_funcptr) {
                            if (!protocol_context->command_info->argument_stream_end_funcptr(
                                    channel,
                                    db,
                                    protocol_context,
                                    reader_context,
                                    op->data.argument.index - 1)) {
                                LOG_D(TAG, "[RECV][REDIS] command callback <argument stream end> failed");
                                goto end;
                            }
                        }
                    } else if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END && !require_stream) {
                        if (protocol_context->command_info->argument_full_funcptr) {
                            size_t chunk_length = op->data.argument.length;
                            char *chunk_data = read_buffer_data_start + op->data.argument.offset;

                            if (!protocol_context->command_info->argument_full_funcptr(
                                    channel,
                                    db,
                                    protocol_context,
                                    reader_context,
                                    op->data.argument.index - 1,
                                    chunk_data,
                                    chunk_length)) {
                                LOG_D(TAG, "[RECV][REDIS] command callback <argument full> failed");
                                goto end;
                            }
                        }
                    }
                } else if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END) {
                    if (!protocol_context->command_info->command_end_funcptr(
                            channel,
                            db,
                            protocol_context,
                            reader_context)) {
                        LOG_D(TAG, "[RECV][REDIS] command callback <command end> failed");
                        goto end;
                    }
                }
            }
        }
        while (read_buffer->data_size > 0 &&
               reader_context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

        if (reader_context->error != PROTOCOL_REDIS_READER_ERROR_OK) {
            send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                    send_buffer_start,
                    send_buffer_end - send_buffer_start,
                    "ERR parsing error <%d>",
                    reader_context->error);

            if (send_buffer_start == NULL) {
                LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                goto end;
            }

            network_send(
                    channel,
                    send_buffer,
                    send_buffer_start - send_buffer);

            network_close(channel, true);

            goto end;
        } else if (reader_context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED) {
            if (protocol_context->command == NETWORK_PROTOCOL_REDIS_COMMAND_UNKNOWN) {
                // The context can be reset only once the ops have all been parsed and the command has been fully read
                network_protocol_redis_reset_context(protocol_context);
                break;
            }

            assert(protocol_context->command_info != NULL);

            if (protocol_context->command_info->required_positional_arguments_count >
                reader_context->arguments.count - 1) {
                send_buffer_start = protocol_redis_writer_write_simple_error_printf(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        "ERR wrong number of arguments for '%s' command",
                        protocol_context->command_info->string);

                network_protocol_redis_reset_context(protocol_context);

                if (send_buffer_start == NULL) {
                    LOG_D(TAG, "[RECV][REDIS] Unable to write the response into the buffer");
                    goto end;
                }

                return_result = network_send(
                        channel,
                        send_buffer,
                        send_buffer_start - send_buffer);
                goto end;
            }

            // If the command is all parsed the command data in protocol context can be freed
            if (!protocol_context->command_info->command_free_funcptr(
                    channel,
                    db,
                    protocol_context,
                    reader_context)) {
                network_protocol_redis_reset_context(protocol_context);
                LOG_D(TAG, "[RECV][REDIS] command callback <command free> failed");
                goto end;
            }

            network_protocol_redis_reset_context(protocol_context);
        }
    } while(read_buffer->data_size > 0);

    return_result = true;
end:
    if (!return_result) {
        // If the data processing failed it might be necessary to free up the resources
        if (protocol_context->command_info != NULL) {
            // Potentially command free might be double invoked, the callback must be idempotent to ensure that no
            // memory will try to be get freed
            if (!protocol_context->command_info->command_free_funcptr(
                    channel,
                    db,
                    protocol_context,
                    reader_context)) {
                LOG_D(TAG, "[RECV][REDIS] command callback <command free> failed");
            }
        }

        network_protocol_redis_reset_context(protocol_context);
    }

    return return_result;
}

bool network_protocol_redis_is_key_too_long(
        network_channel_t *channel,
        size_t key_length) {
    if (unlikely(key_length > channel->protocol_config->redis->max_key_length)) {
        return true;
    }

    return false;
}