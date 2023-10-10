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
#include "fatal.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "memory_allocator/ffma.h"
#include "config.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
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
#include "worker/worker_fiber.h"
#include "network/network.h"

#include "module_redis.h"
#include "module_redis_config.h"
#include "module_redis_connection.h"
#include "module_redis_command.h"
#include "module_redis_commands.h"

#include "module/redis/fiber/module_redis_fiber_storage_db_snapshot_rdb.h"

bool module_redis_program_ctor(
        config_module_t *config_module) {
    // Setup the hashtable with the list of disabled commands
    module_redis_commands_set_disabled_commands_hashtables(
            module_redis_commands_build_disabled_commands_hashtables(
                    config_module->redis->disabled_commands,
                    config_module->redis->disabled_commands_count));

    return true;
}

bool module_redis_program_dtor(
        config_module_t *config_module) {
    // Free the hashtable with the list of disabled commands and reset it
    module_redis_commands_free_disabled_commands_hashtables(
            module_redis_commands_get_disabled_commands_hashtables());
    module_redis_commands_set_disabled_commands_hashtables(NULL);

    return true;
}

bool module_redis_worker_ctor(
        config_module_t *config_module) {
    worker_context_t *worker_context = worker_context_get();

    // Register the fiber for the RDB snapshot
    if (!worker_fiber_register(
            worker_context,
            "module-redis-fiber-storage-db-snapshot-rdb",
            module_redis_fiber_storage_db_snapshot_rdb_fiber_entrypoint,
            NULL)) {
        return false;
    }

    return true;
}

FUNCTION_CTOR(module_redis_register_ctor, {
    module_register(
            "redis",
            module_redis_config_prepare,
            module_redis_config_validate_after_load,
            module_redis_program_ctor,
            module_redis_program_dtor,
            module_redis_worker_ctor,
            NULL,
            module_redis_connection_accept);
});
