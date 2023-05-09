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
#include "transaction_spinlock.h"
#include "utils_string.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"
#include "worker/worker_op.h"

#include "module_redis_command_helper_auth.h"

#define TAG "module_redis_command_helper_auth"

bool module_redis_command_helper_auth_try_positional_parameters(
        module_redis_connection_context_t *connection_context,
        char *parameter_position_1,
        size_t parameter_position_1_len,
        char *parameter_position_2,
        size_t parameter_position_2_len) {
    char *client_username = "default";
    size_t client_username_len = 0;
    char *client_password = NULL;
    size_t client_password_len = 0;

    // Although this behaviour is different from the one in use by Redis, it's more secure
    if (module_redis_connection_is_authenticated(connection_context)) {
        module_redis_command_helper_auth_error_already_authenticated(connection_context);
        return false;
    }

    // The order of the parameters for AUTH is inverted in the generated scaffolding, the username is optional
    // and comes after the password so if it's present the username is actually the password and the password
    // field is the username
    if (parameter_position_2) {
        client_username = parameter_position_1;
        client_username_len = parameter_position_1_len;
        client_password = parameter_position_2;
        client_password_len = parameter_position_2_len;
    } else {
        client_password = parameter_position_1;
        client_password_len = parameter_position_1_len;
    }

    if (!module_redis_connection_authenticate(
            connection_context,
            client_username,
            client_username_len,
            client_password,
            client_password_len)) {
        module_redis_command_helper_auth_error_failed(connection_context);
        return false;
    }

    return true;
}

bool module_redis_command_helper_auth_error_failed(
        module_redis_connection_context_t *connection_context) {
    module_redis_connection_error_message_printf_noncritical(
            connection_context,
            "AUTH failed: WRONGPASS invalid username-password pair or user is disabled.");
    connection_context->terminate_connection = true;
    return true;
}

bool module_redis_command_helper_auth_error_already_authenticated(
        module_redis_connection_context_t *connection_context) {
    module_redis_connection_error_message_printf_noncritical(
            connection_context,
            "AUTH failed: already authenticated.");
    connection_context->terminate_connection = true;
    return true;
}

bool module_redis_command_helper_auth_error_not_authenticated(
        module_redis_connection_context_t *connection_context) {
    module_redis_connection_error_message_printf_noncritical(
            connection_context,
            "NOAUTH Authentication required.");
    connection_context->terminate_connection = true;
    return true;
}
