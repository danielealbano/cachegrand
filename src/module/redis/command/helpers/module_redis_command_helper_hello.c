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
#include <arpa/inet.h>
#include <stdlib.h>
#include <math.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "utils_string.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "config.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/network.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"
#include "worker/worker_op.h"
#include "module_redis_command_helper_auth.h"

#include "module_redis_command_helper_hello.h"

#define TAG "module_redis_command_helper_hello"

bool module_redis_command_helper_hello_has_valid_proto_version(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context) {
    if (connection_context->reader_context.arguments.count > 1) {
        if (context->protover.value < 2 || context->protover.value > 3) {
            return false;
        }
    }

    // No proto version specified, default to 2 so it's valid
    return true;
}

bool module_redis_command_helper_hello_client_authorized_to_invoke_command(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context) {
    // If the authentication is required and the client is not authenticated and it's not trying to authenticate, return
    // an error as per Redis behaviour
    if (module_redis_connection_requires_authentication(connection_context)) {
        if (
                !module_redis_connection_is_authenticated(connection_context) &&
                !context->auth_username_password.has_token) {
            module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "NOAUTH HELLO must be called with the client already authenticated, otherwise the "
                    "HELLO AUTH <user> <pass> option can be used to authenticate the client and select the RESP "
                    "protocol version at the same time");
            connection_context->terminate_connection = true;
            return false;
        }
    }

    // If the client doesn't require authentication, is already authenticated or is trying to authenticate, then
    // it's authorized to invoke the command
    return true;
}

bool module_redis_command_helper_hello_client_trying_to_reauthenticate(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context) {
    if (context->auth_username_password.has_token) {
        return module_redis_command_helper_auth_client_trying_to_reauthenticate(connection_context);
    }

    return true;
}

void module_redis_command_helper_hello_try_fetch_proto_version(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context) {
    if (connection_context->reader_context.arguments.count > 1) {
        connection_context->resp_version = context->protover.value == 2
                                           ? PROTOCOL_REDIS_RESP_VERSION_2
                                           : PROTOCOL_REDIS_RESP_VERSION_3;
    }
}

void module_redis_command_helper_hello_try_fetch_client_name(
        module_redis_connection_context_t *connection_context,
        module_redis_command_hello_context_t *context) {
    if (context->setname_clientname.has_token) {
        connection_context->client_name =
                xalloc_alloc(context->setname_clientname.value.length + 1);
        strncpy(
                context->setname_clientname.value.short_string,
                connection_context->client_name,
                context->setname_clientname.value.length);
        connection_context->client_name[context->setname_clientname.value.length] = 0;
    }
}

bool module_redis_command_helper_hello_send_error_invalid_proto_version(
        module_redis_connection_context_t *connection_context) {
    module_redis_connection_error_message_printf_noncritical(
            connection_context,
            "NOPROTO unsupported protocol version");
    return true;
}

bool module_redis_command_helper_hello_send_response(
        module_redis_connection_context_t *connection_context,
        module_redis_command_helper_hello_response_item_t *hello_items,
        size_t hello_items_count) {
    bool return_res = false;
    network_channel_buffer_data_t *send_buffer, *send_buffer_start, *send_buffer_end;

    size_t slice_length = 512;
    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            connection_context->network_channel,
            slice_length);
    if (send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        goto end;
    }

    send_buffer_end = send_buffer + slice_length;

    if (connection_context->resp_version == PROTOCOL_REDIS_RESP_VERSION_2) {
        send_buffer_start = protocol_redis_writer_write_array(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                hello_items_count * 2);
    } else {
        send_buffer_start = protocol_redis_writer_write_map(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                hello_items_count);
    }

    if (send_buffer_start == NULL) {
        network_send_buffer_release_slice(
                connection_context->network_channel,
                0);
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    for(int i = 0; i < hello_items_count; i++) {
        module_redis_command_helper_hello_response_item_t *hello_item = &hello_items[i];
        send_buffer_start = protocol_redis_writer_write_blob_string(
                send_buffer_start,
                send_buffer_end - send_buffer_start,
                hello_item->key,
                (int)strlen(hello_item->key));

        if (send_buffer_start == NULL) {
            network_send_buffer_release_slice(
                    connection_context->network_channel,
                    0);
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }

        switch(hello_item->value_type) {
            case PROTOCOL_REDIS_TYPE_SIMPLE_STRING:
                send_buffer_start = protocol_redis_writer_write_blob_string(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        (char*)hello_item->value.string,
                        (int)strlen(hello_item->value.string));
                break;
            case PROTOCOL_REDIS_TYPE_NUMBER:
                send_buffer_start = protocol_redis_writer_write_number(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        hello_item->value.number);
                break;
            case PROTOCOL_REDIS_TYPE_ARRAY:
                // not implemented, will simply report an empty array with the count set to 0
                assert(hello_item->value.array.count == 0);

                send_buffer_start = protocol_redis_writer_write_array(
                        send_buffer_start,
                        send_buffer_end - send_buffer_start,
                        hello_item->value.array.count);
                break;
            default:
                assert(0);
        }

        if (send_buffer_start == NULL) {
            network_send_buffer_release_slice(
                    connection_context->network_channel,
                    0);
            LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
            goto end;
        }
    }

    network_send_buffer_release_slice(
            connection_context->network_channel,
            send_buffer_start - send_buffer);

    return_res = true;
    end:

    return return_res;
}
