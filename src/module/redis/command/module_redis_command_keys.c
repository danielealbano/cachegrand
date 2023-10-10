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
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_rwspinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"

#define TAG "module_redis_command_keys"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(keys) {
    bool return_res = false;
    uint64_t keys_count = 0;
    uint64_t cursor_next = 0;
    module_redis_command_keys_context_t *context = connection_context->command.context;

    storage_db_key_and_key_length_t *keys = storage_db_op_get_keys(
            connection_context->db,
            connection_context->database_number,
            0,
            0,
            context->pattern.value.pattern,
            context->pattern.value.length,
            &keys_count,
            &cursor_next);

    assert(cursor_next == 0);

    if (unlikely(!module_redis_connection_send_array_header(connection_context, keys_count))) {
        goto end;
    }

    for(uint64_t index = 0; index < keys_count; index++) {
        if (!module_redis_connection_send_blob_string(
                connection_context,
                keys[index].key,
                keys[index].key_size)) {
            goto end;
        }
    }

    return_res = true;

end:

    storage_db_free_key_and_key_length_list(keys, keys_count);

    return return_res;
}
