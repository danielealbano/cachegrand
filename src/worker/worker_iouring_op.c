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

#include "worker_iouring_op.h"

#define TAG "worker_iouring_op"

worker_iouring_op_context_t* worker_iouring_op_context_init(
        worker_iouring_op_wrapper_completion_cb_fp_t *completion_cb) {
    worker_iouring_op_context_t* op_context =
            slab_allocator_mem_alloc_zero(sizeof(worker_iouring_op_context_t));
    op_context->io_uring.completion_cb = completion_cb;

    return op_context;
}

bool worker_iouring_op_timer_completion_cb(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context,
        io_uring_cqe_t *cqe,
        bool *free_op_context) {
    *free_op_context = true;
    op_context = (worker_iouring_op_context_t*)cqe->user_data;

    bool res = op_context->user.completion_cb.timer(op_context->user.data);

    return res;
}

bool worker_iouring_op_timer(
        worker_iouring_context_t *context,
        worker_op_timer_completion_cb_fp_t *completion_cb,
        long seconds,
        long long nanoseconds,
        void* user_data) {
    worker_iouring_op_context_t *op_context = worker_iouring_op_context_init(
            worker_iouring_op_timer_completion_cb);
    op_context->user.completion_cb.timer = completion_cb;
    op_context->user.data = user_data;
    op_context->io_uring.timeout.ts.tv_sec = seconds;
    op_context->io_uring.timeout.ts.tv_nsec = nanoseconds;

    return io_uring_support_sqe_enqueue_timeout(
            context->ring,
            1,
            &(op_context->io_uring.timeout.ts),
            0,
            (uintptr_t)op_context);
}

bool worker_iouring_op_timer_wrapper(
        worker_op_timer_completion_cb_fp_t *cb,
        long seconds,
        long long nanoseconds,
        void* user_data) {
    return worker_iouring_op_timer(
            worker_iouring_context_get(),
            cb,
            seconds,
            nanoseconds,
            user_data);
}

void worker_iouring_op_register() {
    worker_op_timer = worker_iouring_op_timer_wrapper;
}


