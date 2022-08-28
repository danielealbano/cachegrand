/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "fatal.h"
#include "clock.h"
#include "spinlock.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"
#include "config.h"
#include "fiber.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "network/network.h"

#include "module_redis.h"
#include "module_redis_connection.h"
#include "module_redis_command.h"
#include "module_redis_commands.h"
#include "module_redis_autogenerated_commands_callbacks.h"
#include "module_redis_autogenerated_commands_arguments.h"
#include "module_redis_autogenerated_commands_info_map.h"

#define TAG "module_redis"

hashtable_spsc_t *module_redis_commands_hashtable = NULL;

FUNCTION_CTOR(module_redis_commands_ctor, {
    uint32_t command_infos_map_count = sizeof(command_infos_map) / sizeof(module_redis_command_info_t);

    module_redis_commands_hashtable = module_redis_commands_build_commands_hashtables(
            command_infos_map,
            command_infos_map_count);
    if (!module_redis_commands_hashtable) {
        FATAL(TAG, "Unable to generate the commands hashtable");
    }

    if (!module_redis_commands_build_commands_arguments_token_entries_hashtable(
            command_infos_map,
            command_infos_map_count)) {
        module_redis_commands_free_commands_arguments_token_entries_hashtable(
                command_infos_map,
                command_infos_map_count);

        FATAL(TAG, "Unable to generate the commands arguments tokens hashtables");
    }
});

FUNCTION_DTOR(module_redis_commands_dtor, {
    uint32_t command_infos_map_count = sizeof(command_infos_map) / sizeof(module_redis_command_info_t);

    if (module_redis_commands_hashtable) {
        hashtable_spsc_free(module_redis_commands_hashtable);
    }

    module_redis_commands_free_commands_arguments_token_entries_hashtable(
            command_infos_map,
            command_infos_map_count);
});

void module_redis_accept(
        network_channel_t *network_channel) {
    bool exit_loop = false;
    module_redis_connection_context_t connection_context = { 0 };

    module_redis_connection_context_init(
            &connection_context,
            worker_context_get()->db,
            network_channel,
            network_channel->module_config);

    do {
        if (unlikely(!network_buffer_has_enough_space(
                &connection_context.read_buffer,
                NETWORK_CHANNEL_MAX_PACKET_SIZE))) {
            module_redis_connection_error_message_printf_critical(
                    &connection_context,
                    "ERR command too long");
            module_redis_connection_send_error(&connection_context);

            exit_loop = true;
        }

        if (likely(!exit_loop)) {
            if (unlikely(network_buffer_needs_rewind(
                    &connection_context.read_buffer,
                    NETWORK_CHANNEL_MAX_PACKET_SIZE))) {
                network_buffer_rewind(&connection_context.read_buffer);
            }

            exit_loop = network_receive(
                    network_channel,
                    &connection_context.read_buffer,
                    NETWORK_CHANNEL_MAX_PACKET_SIZE) != NETWORK_OP_RESULT_OK;
        }

        if (likely(!exit_loop)) {
            exit_loop = !module_redis_process_data(
                    &connection_context,
                    &connection_context.read_buffer);
        }
    } while(!exit_loop);

    // Ensure that the command context is always freed if data are allocated when the peer closes the connection or
    // module_redis_process_data returns false and the receiving loop above terminates immediately
    module_redis_command_process_try_free(
            &connection_context);
    module_redis_connection_context_reset(
            &connection_context);
    module_redis_connection_context_cleanup(
            &connection_context);
}

bool module_redis_process_data(
        module_redis_connection_context_t *connection_context,
        network_channel_buffer_t *read_buffer) {
    int32_t ops_found;
    bool return_result = false;
    protocol_redis_reader_op_t ops[8] = { 0 };
    uint8_t ops_size = (sizeof(ops) / sizeof(protocol_redis_reader_op_t));

    worker_context_t *worker_context = worker_context_get();

    // The loops below terminate if data_size is equals to zero, it should never happen that this function is invoked
    // with the read buffer empty.
    assert(read_buffer->data_size > 0);

    do {
        // Keep reading till there are data in the buffer
        do {
            network_channel_buffer_data_t *read_buffer_data_start = read_buffer->data + read_buffer->data_offset;
            ops_found = protocol_redis_reader_read(
                    read_buffer_data_start,
                    read_buffer->data_size,
                    &connection_context->reader_context,
                    ops,
                    ops_size);

            assert(ops_found < UINT8_MAX);

            if (unlikely(module_redis_connection_reader_has_error(connection_context))) {
                assert(ops_found == -1);
                module_redis_connection_set_error_message_from_reader(connection_context);
                break;
            }

            if (unlikely(ops_found == 0)) {
                break;
            }

            // ops_found has to be bigger than uint8_t because protocol_redis_reader_read must return -1 in case of
            // errors, but otherwise it will always return values that are contained in an uint8_t
            for (uint8_t op_index = 0; op_index < (uint8_t)ops_found; op_index++) {
                protocol_redis_reader_op_t *op = &ops[op_index];

                read_buffer->data_offset += op->data_read_len;
                read_buffer->data_size -= op->data_read_len;
                connection_context->command.data_length += op->data_read_len;

                if (unlikely(module_redis_connection_command_too_long(connection_context))) {
                    module_redis_connection_error_message_printf_critical(
                            connection_context,
                            "ERR the command length has exceeded '%u' bytes",
                            connection_context->network_channel->module_config->redis->max_command_length);
                    break;
                }

                // protocol_redis_reader_read will at best parse one command till the end therefore ops will never
                // contain data from other commands, therefore it's not necessary to check for COMMAND_END in the
                // ops to reset the status of the connection_context.
                // If the function changes the behaviour and will support parsing multiple commands then will be
                // essential to properly handle the COMMAND_END op to reset the protocol context
                if (connection_context->command.skip) {
                    // The for loop can't be interrupted, has to continue till the end, read_buffer->data_* have to be
                    // updated and the max command length has to be checked
                    continue;
                }

                if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA && op->data.argument.index == 0) {
                    bool last_op = op_index == (uint8_t) ops_found - 1;
                    bool op_followed_by_argument_end =
                            !last_op && (ops[op_index + 1].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END);

                    if (unlikely(last_op || !op_followed_by_argument_end)) {
                        // Set the reader_context state back to PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA and reset
                        // the current argument received_length
                        connection_context->reader_context.state = PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA;
                        connection_context->reader_context.arguments.current.received_length = 0;

                        // Roll back the buffer
                        read_buffer->data_offset -= op->data_read_len;
                        read_buffer->data_size += op->data_read_len;

                        // No need to continue the parsing, more data are needed
                        return_result = true;
                        goto end;
                    }

                    connection_context->current_argument_token_data_offset = op->data.argument.offset;
                } else if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END && op->data.argument.index == 0) {
                    // If the end of the first argument has been found then check if it's a known command
                    size_t command_length = op->data.argument.length;
                    char *command_data = read_buffer_data_start + connection_context->current_argument_token_data_offset;

                    // Set the current command to UNKNOWN
                    connection_context->command.info = hashtable_spsc_op_get_ci(
                            module_redis_commands_hashtable,
                            command_data,
                            command_length);

                    if (!connection_context->command.info) {
                        module_redis_connection_error_message_printf_noncritical(
                                connection_context,
                                "ERR unknown command `%.*s` with `%d` args",
                                (int)command_length,
                                command_data,
                                connection_context->reader_context.arguments.count - 1);
                        continue;
                    }

                    LOG_D(
                            TAG,
                            "[RECV][REDIS] <%s> command received",
                            connection_context->command.info->string);

                    // Check if the command has been found and if the required arguments are going to be provided else
                    if (unlikely(connection_context->command.info->required_arguments_count >
                        connection_context->reader_context.arguments.count - 1)) {
                        module_redis_connection_error_message_printf_noncritical(
                                connection_context,
                                "ERR wrong number of arguments for '%s' command",
                                connection_context->command.info->string);
                        continue;
                    } else if (unlikely(connection_context->reader_context.arguments.count - 1 >
                            connection_context->network_channel->module_config->redis->max_command_arguments)) {
                        module_redis_connection_error_message_printf_noncritical(
                                connection_context,
                                "ERR command '%s' has '%u' arguments but only '%u' allowed",
                                connection_context->command.info->string,
                                connection_context->reader_context.arguments.count - 1,
                                connection_context->network_channel->module_config->redis->max_command_arguments);
                        continue;
                    }

                    // Invoke the being function callback if it has been set
                    if (unlikely(!module_redis_command_process_begin(connection_context))) {
                        LOG_D(TAG, "[RECV][REDIS] Unable to allocate the command context, terminating connection");
                        goto end;
                    }

                    // If a command has been identified it's possible to move to the next op
                    continue;
                }

                bool is_argument_op =
                        op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN ||
                        op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA ||
                        op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END;

                if (is_argument_op && op->data.argument.index > 0) {
                    if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN) {
                        if (unlikely(!module_redis_command_process_argument_begin(
                                connection_context,
                                op->data.argument.length))) {
                            goto end;
                        }
                    } else {
                        bool require_stream = module_redis_command_process_argument_require_stream(
                                connection_context);

                        if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA) {
                            if (require_stream) {
                                size_t chunk_length = op->data.argument.data_length;
                                char *chunk_data = read_buffer_data_start + op->data.argument.offset;

                                if (unlikely(!module_redis_command_process_argument_stream_data(
                                        connection_context,
                                        chunk_data,
                                        chunk_length))) {
                                    goto end;
                                }
                            } else {
                                // If the require_stream flag is false, the argument_full callback will be called once all the data
                                // have been processed but to ensure that if the buffer gets rewind these data will not be lost
                                // the buffer pointer is moved back as well if there isn't another op or if op_index + 1 isn't an
                                // argument-end op.
                                bool last_op = op_index == (uint8_t) ops_found - 1;
                                bool op_followed_by_argument_end =
                                        !last_op &&
                                        (ops[op_index + 1].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END);

                                if (unlikely(last_op || !op_followed_by_argument_end)) {
                                    // Set the reader_context state back to PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA and reset
                                    // the current argument received_length
                                    connection_context->reader_context.state =
                                            PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA;
                                    connection_context->reader_context.arguments.current.received_length = 0;

                                    // Roll back the buffer
                                    read_buffer->data_offset -= op->data_read_len;
                                    read_buffer->data_size += op->data_read_len;

                                    // No need to continue the parsing, more data are needed
                                    return_result = true;
                                    goto end;
                                }

                                connection_context->current_argument_token_data_offset = op->data.argument.offset;
                            }
                        } else if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END) {
                            if (require_stream) {
                                if (unlikely(!module_redis_command_process_argument_stream_end(connection_context))) {
                                    goto end;
                                }
                            } else {
                                size_t chunk_length = op->data.argument.length;
                                char *chunk_data =
                                        read_buffer_data_start + connection_context->current_argument_token_data_offset;

                                if (unlikely(!module_redis_command_process_argument_full(
                                        connection_context,
                                        chunk_data,
                                        chunk_length))) {
                                    goto end;
                                }
                            }

                            if (unlikely(!module_redis_command_process_argument_end(connection_context))) {
                                goto end;
                            }
                        }
                    }
                } else if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END) {
                    if (unlikely(!module_redis_command_process_end(connection_context))) {
                        goto end;
                    }
                }
            }
        }
        while (
                read_buffer->data_size > 0 &&
                connection_context->reader_context.state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED &&
                connection_context->reader_context.error != PROTOCOL_REDIS_READER_ERROR_OK);

        if (unlikely(module_redis_connection_has_error(connection_context))) {
            if (!module_redis_connection_send_error(connection_context)) {
                goto end;
            }
        }

        if (unlikely(module_redis_connection_should_terminate_connection(connection_context))) {
            module_redis_connection_flush_and_close(connection_context);
            goto end;
        }

        if (connection_context->reader_context.state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED) {
            module_redis_command_process_try_free(connection_context);
            module_redis_connection_context_reset(connection_context);
        }
    } while(read_buffer->data_size > 0 && ops_found > 0);

    return_result = true;

end:

    if (likely(return_result)) {
        if (likely(network_should_flush_send_buffer(connection_context->network_channel))) {
            return_result =
                    network_flush_send_buffer(connection_context->network_channel) == NETWORK_OP_RESULT_OK;
        }
    }

    // No need to free up the command context or reset the connection context if the connection has to be closed because
    // this operation has to be carried out anyway by the caller to ensure that if the connection is closed by the peer
    // (and therefore this function is not invoked at all) the memory will always be freed up

    return return_result;
}
