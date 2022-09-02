/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "config.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "fiber.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"

#include "worker_context.h"

thread_local worker_context_t *thread_local_worker_context = NULL;

worker_context_t* worker_context_get() {
    return thread_local_worker_context;
}

void worker_context_set(
        worker_context_t *worker_context) {
    thread_local_worker_context = worker_context;
}

void worker_context_reset() {
    thread_local_worker_context = NULL;
}
