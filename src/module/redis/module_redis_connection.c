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
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "clock.h"
#include "config.h"
#include "data_structures/ring_bounded_spsc/ring_bounded_spsc.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
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
#include "network/network.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/redis/module_redis.h"

#include "module_redis_connection.h"
#include "module_redis_command.h"

#define TAG "module_redis_connection"

void module_redis_connection_context_init(
        module_redis_connection_context_t *connection_context,
        storage_db_t *db,
        network_channel_t *network_channel,
        config_module_t *config_module) {
    connection_context->resp_version = PROTOCOL_REDIS_RESP_VERSION_2,
    connection_context->db = db;
    connection_context->network_channel = network_channel;
    connection_context->read_buffer.data = (char *)ffma_mem_alloc_zero(NETWORK_CHANNEL_RECV_BUFFER_SIZE);
    connection_context->read_buffer.length = NETWORK_CHANNEL_RECV_BUFFER_SIZE;
}

void module_redis_connection_context_cleanup(
        module_redis_connection_context_t *connection_context) {
    if (connection_context->client_name) {
        ffma_mem_free(connection_context->client_name);
    }
    ffma_mem_free(connection_context->read_buffer.data);
}

void module_redis_connection_context_reset(
        module_redis_connection_context_t *connection_context) {
    // Reset the reader_context to handle the next command in the buffer, the resp_version isn't touched as it's
    // to be known all along the connection lifecycle
    connection_context->command.info = NULL;
    connection_context->command.context  = NULL;
    connection_context->command.skip = false;
    connection_context->command.data_length = 0;
    connection_context->terminate_connection = false;

    memset(&connection_context->command.parser_context, 0, sizeof(module_redis_command_parser_context_t));

    if (connection_context->error.message != NULL) {
        ffma_mem_free(connection_context->error.message);
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
        ffma_mem_free(connection_context->error.message);
    }

    // Calculate the total amount of memory needed
    va_list args_copy;
    va_copy(args_copy, args);
    ssize_t error_message_with_args_length = vsnprintf(NULL, 0, error_message, args_copy);
    va_end(args_copy);

    // The vsnprintf above should really never fail
    assert(error_message_with_args_length > 0);

    // Allocate the memory and run vsnprintf
    char *error_message_with_args = ffma_mem_alloc(error_message_with_args_length + 1);

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
    ffma_mem_free(connection_context->error.message);
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
