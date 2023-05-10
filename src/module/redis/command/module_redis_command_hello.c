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
#include <stdlib.h>
#include <errno.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "fiber/fiber.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"
#include "module/redis/module_redis_command.h"
#include "network/network.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "helpers/module_redis_command_helper_auth.h"
#include "helpers/module_redis_command_helper_hello.h"

#define TAG "module_redis_command_hello"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(hello) {
    module_redis_command_hello_context_t *context = connection_context->command.context;

    // Check if the command can be invoked (e.g. because auth not required, the client is auth-ed or the client is not
    // auth-ed but the request includes the AUTH token to try to authenticate)
    if (!module_redis_command_helper_hello_client_authorized_to_invoke_command(
            connection_context,
            context)) {
        return true;
    }

    if (!module_redis_command_helper_hello_client_trying_to_reauthenticate(connection_context, context)) {
        return true;
    }

    if (module_redis_connection_requires_authentication(connection_context) && context->auth_username_password.has_token) {
        if (!module_redis_connection_authenticate(
                connection_context,
                context->auth_username_password.value.username.value.short_string,
                context->auth_username_password.value.username.value.length,
                context->auth_username_password.value.password.value.short_string,
                context->auth_username_password.value.password.value.length)) {
            return module_redis_command_helper_auth_error_failed(connection_context);
        }
    }

    if (!module_redis_command_helper_hello_has_valid_proto_version(connection_context, context)) {
        return module_redis_command_helper_hello_send_error_invalid_proto_version(connection_context);
    }

    module_redis_command_helper_hello_try_fetch_proto_version(connection_context, context);
    module_redis_command_helper_hello_try_fetch_client_name(connection_context, context);

    return module_redis_command_helper_hello_send_response(connection_context);
}
