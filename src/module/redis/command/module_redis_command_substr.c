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
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "memory_allocator/ffma.h"
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
#include "module/redis/module_redis_command.h"

#define TAG "module_redis_command_substr"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(substr) {
    bool return_res = false;
    storage_db_entry_index_t *entry_index = NULL;
    module_redis_command_substr_context_t *context = connection_context->command.context;

    entry_index = storage_db_get_entry_index_for_read(
            connection_context->db,
            connection_context->database_number,
            context->key.value.key,
            context->key.value.length);

    if (unlikely(!entry_index)) {
        return_res = module_redis_connection_send_string_null(connection_context);
        goto end;
    }

    off_t range_start = context->start.value;
    off_t range_end = context->end.value;

    if (range_start < 0) {
        range_start += (off_t)entry_index->value.size;
    }

    if (range_end < 0) {
        range_end += (off_t)entry_index->value.size;
    }

    if (unlikely(range_end < range_start)) {
        return_res = module_redis_connection_send_blob_string(connection_context, "", 0);
        goto end;
    }

    off_t range_length = range_end - range_start + 1;

    if (unlikely(range_start > entry_index->value.size || range_length <= 0)) {
        return_res = module_redis_connection_send_blob_string(connection_context, "", 0);
        goto end;
    }

    if (unlikely(range_start + range_length > entry_index->value.size)) {
        range_length = (off_t)entry_index->value.size - range_start;
    }

    return_res = module_redis_command_stream_entry_range(
            connection_context->network_channel,
            connection_context->db,
            entry_index,
            range_start,
            range_length);

end:

    if (likely(entry_index)) {
        storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);
        entry_index = NULL;
    }

    return return_res;
}
