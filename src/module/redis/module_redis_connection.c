/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
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
#include "spinlock.h"
#include "transaction.h"
#include "clock.h"
#include "config.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "network/network.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/redis/module_redis.h"
#include "module/redis/command/helpers/module_redis_command_helper_auth.h"

#include "module_redis_connection.h"
#include "module_redis_command.h"
#include "module_redis_commands.h"

#define TAG "module_redis_connection"

void module_redis_connection_context_init(
        module_redis_connection_context_t *connection_context,
        storage_db_t *db,
        config_t *config,
        network_channel_t *network_channel) {
    connection_context->resp_version = PROTOCOL_REDIS_RESP_VERSION_2;
    connection_context->db = db;
    connection_context->config = config;
    connection_context->network_channel = network_channel;
    connection_context->read_buffer.data = (char *)xalloc_alloc_zero(NETWORK_CHANNEL_RECV_BUFFER_SIZE);
    connection_context->read_buffer.length = NETWORK_CHANNEL_RECV_BUFFER_SIZE;
}

void module_redis_connection_context_cleanup(
        module_redis_connection_context_t *connection_context) {
    if (connection_context->client_name) {
        xalloc_free(connection_context->client_name);
    }
    xalloc_free(connection_context->read_buffer.data);
}

void module_redis_connection_context_reset(
        module_redis_connection_context_t *connection_context) {
    if (connection_context->command.command_string_with_container) {
        xalloc_free(connection_context->command.command_string_with_container);
    }

    // Reset the reader_context to handle the next command in the buffer, the resp_version isn't touched as it's
    // to be known all along the connection lifecycle
    connection_context->command.info = NULL;
    connection_context->command.context  = NULL;
    connection_context->command.skip = false;
    connection_context->command.data_length = 0;
    connection_context->command.arguments_offset = 0;
    connection_context->command.command_string_with_container = NULL;
    connection_context->command.command_string_with_container_length = 0;
    connection_context->terminate_connection = false;

    memset(&connection_context->command.parser_context, 0, sizeof(module_redis_command_parser_context_t));

    if (connection_context->error.message != NULL) {
        xalloc_free(connection_context->error.message);
        connection_context->error.message = NULL;
    }

    protocol_redis_reader_context_reset(&connection_context->reader_context);
}

bool module_redis_connection_reader_has_error(
        module_redis_connection_context_t *connection_context) {
    return connection_context->reader_context.error != PROTOCOL_REDIS_READER_ERROR_OK;
}

void module_redis_connection_set_error_message_from_reader(
        module_redis_connection_context_t *connection_context) {
    module_redis_connection_error_message_printf_critical(
            connection_context,
            "ERR parsing error '%d'",
            connection_context->reader_context.error);
}

bool module_redis_connection_should_terminate_connection(
        module_redis_connection_context_t *connection_context) {
    return connection_context->terminate_connection;
}

bool module_redis_connection_error_message_vprintf_internal(
        module_redis_connection_context_t *connection_context,
        bool override_previous_error,
        char *error_message,
        va_list args) {
    bool return_res = false;

    if (!override_previous_error) {
        // Can't set an error message on top of an existing one, something is wrong if that happens
        assert(connection_context->error.message == NULL);
    }

    if (connection_context->error.message != NULL) {
        xalloc_free(connection_context->error.message);
    }

    // Calculate the total amount of memory needed
    va_list args_copy;
    va_copy(args_copy, args);
    ssize_t error_message_with_args_length = vsnprintf(NULL, 0, error_message, args_copy);
    va_end(args_copy);

    // The vsnprintf above should really never fail
    assert(error_message_with_args_length > 0);

    // Allocate the memory and run vsnprintf
    char *error_message_with_args = xalloc_alloc(error_message_with_args_length + 1);

    if (error_message_with_args == NULL) {
        LOG_E(TAG, "Unable to allocate <%lu> bytes for the command error message", error_message_with_args_length + 1);
        goto end;
    }

    if (vsnprintf(
            error_message_with_args,
            error_message_with_args_length + 1,
            error_message,
            args) < 0) {
        LOG_E(TAG, "Failed to format string <%s> with the arguments requested", error_message);
        LOG_E_OS_ERROR(TAG);
        goto end;
    }

    connection_context->error.message = error_message_with_args;

    return_res = true;

end:

    connection_context->command.skip = true;
    if (!return_res) {
        connection_context->terminate_connection = true;
    }

    return return_res;
}

bool module_redis_connection_error_message_printf_noncritical(
        module_redis_connection_context_t *connection_context,
        char *error_message,
        ...) {
    va_list args;
    va_start(args, error_message);

    bool res = module_redis_connection_error_message_vprintf_internal(
            connection_context,
            false,
            error_message,
            args);

    va_end(args);

    return res;
}

bool module_redis_connection_error_message_printf_critical(
        module_redis_connection_context_t *connection_context,
        char *error_message,
        ...) {
    va_list args;
    va_start(args, error_message);

    bool res = module_redis_connection_error_message_vprintf_internal(
            connection_context,
            true,
            error_message,
            args);

    va_end(args);

    connection_context->terminate_connection = true;

    return res;
}

bool module_redis_connection_has_error(
        module_redis_connection_context_t *connection_context) {
    return connection_context->error.message != NULL;
}

bool module_redis_connection_send_number(
        module_redis_connection_context_t *connection_context,
        int64_t number) {
    bool return_result = false;
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;
    size_t slice_length = 48;

    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        goto end;
    }

    send_buffer_start = protocol_redis_writer_write_number(
            send_buffer_start,
            slice_length,
            number);

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    return_result = true;

end:

    return return_result;
}

bool module_redis_connection_send_array_header(
        module_redis_connection_context_t *connection_context,
        uint64_t array_length) {
    bool return_result = false;
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;
    size_t slice_length = 32;
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        goto end;
    }

    send_buffer_start = protocol_redis_writer_write_array(
            send_buffer_start,
            slice_length,
            array_length);

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    return_result = true;

end:

    return return_result;
}

bool module_redis_connection_send_ok(
        module_redis_connection_context_t *connection_context) {
    bool return_result = false;
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;
    size_t slice_length = 32;
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        goto end;
    }

    send_buffer_start = protocol_redis_writer_write_simple_string(
            send_buffer_start,
            slice_length,
            "OK",
            2);

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    return_result = true;

    end:

    return return_result;
}

bool module_redis_connection_send_error(
        module_redis_connection_context_t *connection_context) {
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;
    bool return_result = false;
    size_t error_message_length = strlen(connection_context->error.message);
    size_t slice_length = error_message_length + 16;

    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        goto end;
    }

    send_buffer_start = protocol_redis_writer_write_simple_error(
            send_buffer_start,
            slice_length,
            connection_context->error.message,
            (int)error_message_length);
    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    return_result = send_buffer_start != NULL;

    // Free up the error message and set it to null to avoid sending the same error message multiple times
    // while the command is still being parsed if the connection is not closed
    xalloc_free(connection_context->error.message);
    connection_context->error.message = NULL;

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    return_result = true;

end:

    return return_result;
}

bool module_redis_connection_send_string_null(
        module_redis_connection_context_t *connection_context) {
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;
    size_t slice_length = 16;
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        return false;
    }

    if (connection_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2) {
        send_buffer_start = protocol_redis_writer_write_blob_string_null(
                send_buffer_start,
                slice_length);
    } else {
        send_buffer_start = protocol_redis_writer_write_null(
                send_buffer_start,
                slice_length);
    }

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    return true;
}

bool module_redis_connection_send_blob_string(
        module_redis_connection_context_t *connection_context,
        char *string,
        size_t string_length) {
    network_channel_buffer_data_t *send_buffer, *send_buffer_start, *send_buffer_end;
    size_t slice_length = 32;

    if (likely(string_length < NETWORK_CHANNEL_MAX_PACKET_SIZE - slice_length)) {
        slice_length += string_length;
        send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
                connection_context->network_channel,
                slice_length);
        if (send_buffer_start == NULL) {
            LOG_E(TAG, "Unable to acquire send buffer slice!");
            return false;
        }

        send_buffer_start = protocol_redis_writer_write_blob_string(
                send_buffer_start,
                slice_length,
                string,
                (int)string_length);

        network_send_buffer_release_slice(
                connection_context->network_channel,
                send_buffer_start ? send_buffer_start - send_buffer : 0);
    } else {
        if (unlikely(!module_redis_command_acquire_slice_and_write_blob_start(
                connection_context->network_channel,
                slice_length,
                string_length,
                &send_buffer,
                &send_buffer_start,
                &send_buffer_end))) {
            return false;
        }

        for(size_t string_offset = 0; string_offset < string_length; string_offset += NETWORK_CHANNEL_MAX_PACKET_SIZE) {
            size_t total_string_to_send = string_length - string_offset;
            size_t string_length_to_send = total_string_to_send > NETWORK_CHANNEL_MAX_PACKET_SIZE
                    ? NETWORK_CHANNEL_MAX_PACKET_SIZE
                    : total_string_to_send;

            if (network_send_direct(
                    connection_context->network_channel,
                    string + string_offset,
                    string_length_to_send) != NETWORK_OP_RESULT_OK) {
                LOG_E(TAG, "Failed to send string chunk!");
                return false;
            }
        }

        send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
                connection_context->network_channel,
                slice_length);
        if (unlikely(send_buffer_start == NULL)) {
            LOG_E(TAG, "Unable to acquire send buffer slice!");
            return false;
        }

        send_buffer_start = protocol_redis_writer_write_argument_blob_end(
                send_buffer_start,
                slice_length);
        network_send_buffer_release_slice(
                connection_context->network_channel,
                send_buffer_start ? send_buffer_start - send_buffer : 0);
    }

    if (unlikely(send_buffer_start == NULL)) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        return false;
    }

    return true;
}

bool module_redis_connection_send_simple_string(
        module_redis_connection_context_t *connection_context,
        char *string,
        size_t string_length) {
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;
    size_t slice_length = 32;

    // Makes no sense to support sending out simple strings longer than 32kb so report it as a failure
    if (unlikely(string_length > NETWORK_CHANNEL_MAX_PACKET_SIZE - slice_length)) {
        LOG_E(
                TAG,
                "String too long to be sent as simple string, the max simple string length is %lu",
                NETWORK_CHANNEL_MAX_PACKET_SIZE - slice_length);
        return false;
    }

    slice_length += string_length;
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        return false;
    }

    send_buffer_start = protocol_redis_writer_write_simple_string(
            send_buffer_start,
            slice_length,
            string,
            (int)string_length);

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    if (unlikely(send_buffer_start == NULL)) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        return false;
    }

    return true;
}

bool module_redis_connection_send_array(
        module_redis_connection_context_t *connection_context,
        uint32_t count) {
    network_channel_buffer_data_t *send_buffer, *send_buffer_start;
    size_t slice_length = 32;
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        return false;
    }

    send_buffer_start = protocol_redis_writer_write_array(
            send_buffer_start,
            slice_length,
            count);

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    return true;
}

bool module_redis_connection_flush_and_close(
        module_redis_connection_context_t *connection_context) {
    if (network_flush_send_buffer(
            connection_context->network_channel) != NETWORK_OP_RESULT_OK) {
        return false;
    }

    if (network_close(
            connection_context->network_channel, true) != NETWORK_OP_RESULT_OK) {
        return false;
    }

    return true;
}

bool module_redis_connection_command_too_long(
        module_redis_connection_context_t *connection_context) {
    return connection_context->command.data_length >
        connection_context->network_channel->module_config->redis->max_command_length;
}

bool module_redis_connection_authenticate(
        module_redis_connection_context_t *connection_context,
        char *client_username,
        size_t client_username_len,
        char *client_password,
        size_t client_password_len) {
    char *config_username_default = "default";
    char *config_username = connection_context->network_channel->module_config->redis->username;
    char *config_password = connection_context->network_channel->module_config->redis->password;

    if (config_username == NULL) {
        config_username = config_username_default;
    }

    if (strncmp(config_username, client_username, client_username_len) != 0) {
        return false;
    }

    if (strncmp(config_password, client_password, client_password_len) != 0) {
        return false;
    }

    connection_context->authenticated = true;
    return true;
}

bool module_redis_connection_requires_authentication(
        module_redis_connection_context_t *connection_context) {
    return connection_context->network_channel->module_config->redis->require_authentication;
}

bool module_redis_connection_is_authenticated(
        module_redis_connection_context_t *connection_context) {
    return
            connection_context->network_channel->module_config->redis->require_authentication == false
        ||
        connection_context->authenticated;
}

void module_redis_connection_accept(
        network_channel_t *network_channel) {
    bool exit_loop = false;
    module_redis_connection_context_t connection_context = { 0 };

    module_redis_connection_context_init(
            &connection_context,
            worker_context_get()->db,
            worker_context_get()->config,
            network_channel);

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
            exit_loop = !module_redis_connection_process_data(
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

bool module_redis_connection_process_data(
        module_redis_connection_context_t *connection_context,
        network_channel_buffer_t *read_buffer) {
    int32_t ops_found;
    bool return_result = false;
    protocol_redis_reader_op_t ops[8] = { 0 };
    uint8_t ops_size = (sizeof(ops) / sizeof(protocol_redis_reader_op_t));

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

                if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA && op->data.argument.index - connection_context->command.arguments_offset == 0) {
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
                } else if (op->type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END && op->data.argument.index - connection_context->command.arguments_offset == 0) {
                    // If the end of the first argument has been found then check if it's a known command
                    size_t command_length = op->data.argument.length;
                    char *command_data = read_buffer_data_start + connection_context->current_argument_token_data_offset;
                    char *command_data_to_search = command_data;
                    size_t command_data_to_search_length = command_length;

                    if (connection_context->command.command_string_with_container) {
                        strncpy(
                                connection_context->command.command_string_with_container + connection_context->command.command_string_with_container_length,
                                command_data,
                                MIN(command_length, MODULE_REDIS_COMMAND_MAX_LENGTH));
                        command_data_to_search = connection_context->command.command_string_with_container;
                        command_data_to_search_length =
                                connection_context->command.command_string_with_container_length + command_length;

                        *(command_data_to_search + command_data_to_search_length) = '\0';
                    }

                    // Try to fetch the current command
                    connection_context->command.info = hashtable_spsc_op_get_ci(
                            module_redis_commands_hashtable,
                            command_data_to_search,
                            command_data_to_search_length);

                    if (!connection_context->command.info) {
                        module_redis_connection_error_message_printf_noncritical(
                                connection_context,
                                "ERR unknown command `%.*s` with `%d` args",
                                (int)command_data_to_search_length,
                                command_data_to_search,
                                connection_context->reader_context.arguments.count - 1 - connection_context->command.arguments_offset);
                        continue;
                    }

                    if (unlikely(module_redis_commands_is_command_disabled(
                            connection_context->command.info->string,
                            connection_context->command.info->string_len))) {
                        module_redis_connection_error_message_printf_noncritical(
                                connection_context,
                                "ERR command `%.*s` is disabled",
                                (int)command_data_to_search_length,
                                command_data_to_search);
                        continue;
                    }

                    if (connection_context->command.info->requires_authentication &&
                        !module_redis_connection_is_authenticated(connection_context)) {
                        connection_context->command.info = NULL;
                        module_redis_command_helper_auth_error_not_authenticated(connection_context);
                        continue;
                    }

                    if (connection_context->command.info->is_container) {
                        connection_context->command.arguments_offset++;
                        connection_context->command.info = NULL;

                        size_t current_length = connection_context->command.command_string_with_container_length;
                        connection_context->command.command_string_with_container_length += command_length + 1;
                        connection_context->command.command_string_with_container = xalloc_realloc(
                                connection_context->command.command_string_with_container,
                                connection_context->command.command_string_with_container_length + MODULE_REDIS_COMMAND_MAX_LENGTH + 1);

                        // Append the container command
                        strncpy(
                                connection_context->command.command_string_with_container + current_length,
                                command_data,
                                command_length);

                        // Add the space
                        *(connection_context->command.command_string_with_container + current_length + command_length) = ' ';

                        continue;
                    }

                    LOG_D(
                            TAG,
                            "[RECV][REDIS] <%s> command received",
                            connection_context->command.info->string);

                    // Check if the command has been found and if the required arguments are going to be provided else
                    if (unlikely(connection_context->command.info->required_arguments_count >
                                 connection_context->reader_context.arguments.count - 1 - connection_context->command.arguments_offset)) {
                        module_redis_connection_error_message_printf_noncritical(
                                connection_context,
                                "ERR wrong number of arguments for '%s' command",
                                connection_context->command.info->string);
                        continue;
                    } else if (unlikely(connection_context->reader_context.arguments.count - 1 - connection_context->command.arguments_offset >
                                        connection_context->network_channel->module_config->redis->max_command_arguments)) {
                        module_redis_connection_error_message_printf_noncritical(
                                connection_context,
                                "ERR command '%s' has '%u' arguments but only '%u' allowed",
                                connection_context->command.info->string,
                                connection_context->reader_context.arguments.count - 1 - connection_context->command.arguments_offset,
                                connection_context->network_channel->module_config->redis->max_command_arguments);
                        continue;
                    }

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

                if (is_argument_op && ((int64_t)op->data.argument.index - (int64_t)connection_context->command.arguments_offset) > 0) {
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
