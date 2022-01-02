/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <liburing.h>
#include <arpa/inet.h>
#include <dlfcn.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "config.h"
#include "fiber.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "slab_allocator.h"
#include "support/io_uring/io_uring_support.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "worker/worker_common.h"
#include "worker/worker_iouring.h"
#include "worker/worker_op.h"
#include "fiber_scheduler.h"
#include "worker_iouring_op.h"

#define TAG "worker_iouring_op"

bool worker_iouring_op_timer(
        long seconds,
        long long nanoseconds) {
    kernel_timespec_t kernel_timespec = {
            .tv_sec = seconds,
            .tv_nsec = nanoseconds
    };

    bool res = io_uring_support_sqe_enqueue_timeout(
            worker_iouring_context_get()->ring,
            0,
            &kernel_timespec,
            0,
            (uintptr_t) fiber_scheduler_get_current());

    if (res == false) {
        return false;
    }

    // Switch the execution back to the scheduler
    fiber_scheduler_switch_back();

    return true;
}

void worker_iouring_op_register() {
    worker_op_timer = worker_iouring_op_timer;
}
