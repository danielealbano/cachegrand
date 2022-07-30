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
#include "clock.h"
#include "config.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"
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
#include "module_redis_command.h"

#include "module_redis_connection.h"

#define TAG "module_redis_connection"

void module_redis_connection_context_init(
        module_redis_connection_context_t *connection_context,
        storage_db_t *db,
        network_channel_t *network_channel,
        config_module_t *config_module) {
    connection_context->resp_version = PROTOCOL_REDIS_RESP_VERSION_2,
    connection_context->db = db;
    connection_context->network_channel = network_channel;
    connection_context->read_buffer.data = (char *)slab_allocator_mem_alloc_zero(NETWORK_CHANNEL_RECV_BUFFER_SIZE);
    connection_context->read_buffer.length = NETWORK_CHANNEL_RECV_BUFFER_SIZE;
}

void module_redis_connection_context_cleanup(
        module_redis_connection_context_t *connection_context) {
    slab_allocator_mem_free(connection_context->read_buffer.data);
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

    if (connection_context->error.message != NULL) {
        slab_allocator_mem_free(connection_context->error.message);
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
            "ERR parsing error <%d>",
            connection_context->reader_context.error);
}

bool module_redis_connection_should_terminate_connection(
        module_redis_connection_context_t *connection_context) {
    return connection_context->terminate_connection;
}

void module_redis_connection_error_message_vprintf_internal(
        module_redis_connection_context_t *connection_context,
        bool override_previous_error,
        char *error_message,
        va_list args) {
    if (!override_previous_error) {
        // Can't set an error message on top of an existing one, something is wrong if that happens
        assert(connection_context->error.message == NULL);
    }

    if (connection_context->error.message != NULL) {
        slab_allocator_mem_free(connection_context->error.message);
    }

    // Calculate the total amount of memory needed
    va_list args_copy;
    va_copy(args_copy, args);
    ssize_t error_message_with_args_length = vsnprintf(NULL, 0, error_message, args_copy);
    va_end(args_copy);

    // The vsnprintf above should really never fail
    assert(error_message_with_args_length > 0);

    // Allocate the memory and run vsnprintf
    char *error_message_with_args = slab_allocator_mem_alloc(error_message_with_args_length + 1);

    if (error_message_with_args == NULL) {
        LOG_E(TAG, "Unable to allocate <%lu> bytes for the command error message", error_message_with_args_length + 1);
        return;
    }

    if (vsnprintf(
            error_message_with_args,
            error_message_with_args_length + 1,
            error_message,
            args) < 0) {
        LOG_E(TAG, "Failed to format string <%s> with the arguments requested", error_message);
        LOG_E_OS_ERROR(TAG);

        return;
    }

    connection_context->error.message = error_message_with_args;
    connection_context->command.skip = true;
}

void module_redis_connection_error_message_printf_noncritical(
        module_redis_connection_context_t *connection_context,
        char *error_message,
        ...) {
    va_list args;
    va_start(args, error_message);

    module_redis_connection_error_message_vprintf_internal(
            connection_context,
            false,
            error_message,
            args);

    va_end(args);
}

void module_redis_connection_error_message_printf_critical(
        module_redis_connection_context_t *connection_context,
        char *error_message,
        ...) {
    va_list args;
    va_start(args, error_message);

    module_redis_connection_error_message_vprintf_internal(
            connection_context,
            true,
            error_message,
            args);

    va_end(args);

    connection_context->terminate_connection = true;
}

bool module_redis_connection_has_error(
        module_redis_connection_context_t *connection_context) {
    return connection_context->error.message != NULL;
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
    slab_allocator_mem_free(connection_context->error.message);
    connection_context->error.message = NULL;

    if (send_buffer_start == NULL) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    return_result = true;

end:

    return return_result;
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

void module_redis_connection_try_free_command_context(
        module_redis_connection_context_t *connection_context) {
    if (connection_context->command.info == NULL || connection_context->command.context == NULL) {
        return;
    }

    module_redis_command_free_context(
            connection_context->command.info,
            connection_context->command.context);
}

bool module_redis_connection_command_too_long(
        module_redis_connection_context_t *connection_context) {
    return connection_context->command.data_length >
        connection_context->network_channel->module_config->redis->max_command_length;
}
