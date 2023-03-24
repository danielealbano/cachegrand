/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <liburing.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "log/log.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "fiber/fiber.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
#include "support/io_uring/io_uring_support.h"
#include "config.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "storage/channel/storage_channel_iouring.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "fiber/fiber_scheduler.h"
#include "worker/worker_iouring_op.h"
#include "worker/worker_iouring.h"
#include "worker/storage/worker_storage_op.h"
#include "worker/storage/worker_storage_iouring_op.h"

bool worker_storage_iouring_complete_op_simple() {
    // Switch the execution back to the scheduler
    fiber_scheduler_switch_back();

    // When the fiber continues the execution, it has to fetch the return value
    io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

    if (cqe->res < 0) {
        fiber_scheduler_set_error(-cqe->res);
        return false;
    }

    return true;
}

storage_channel_t* worker_storage_iouring_op_storage_open(
        char *path,
        storage_io_common_open_flags_t flags,
        storage_io_common_open_mode_t mode) {
    storage_channel_iouring_t *storage_channel_iouring;
    worker_iouring_context_t *context = worker_iouring_context_get();

    fiber_scheduler_reset_error();

    if (!io_uring_support_sqe_enqueue_openat(
            context->ring,
            0,
            path,
            flags,
            mode,
            0,
            (uintptr_t)fiber_scheduler_get_current())) {
        fiber_scheduler_set_error(ENOMEM);
        return NULL;
    }

    // Switch the execution back to the scheduler
    fiber_scheduler_switch_back();

    // When the fiber continues the execution, it has to fetch the return value
    io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

    if (cqe->res < 0) {
        fiber_scheduler_set_error(-cqe->res);
        return NULL;
    }

    storage_io_common_fd_t fd = cqe->res;

    storage_channel_iouring = storage_channel_iouring_new();
    storage_channel_iouring->has_mapped_fd = false;
    storage_channel_iouring->fd = storage_channel_iouring->wrapped_channel.fd = fd;
    storage_channel_iouring->wrapped_channel.path = path;
    storage_channel_iouring->wrapped_channel.path_len = strlen(path);

    // TODO: should map the fd

    return (storage_channel_t*)storage_channel_iouring;
}

int32_t worker_storage_iouring_op_storage_read(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset) {
    int res;
    worker_iouring_context_t *context = worker_iouring_context_get();

    fiber_scheduler_reset_error();

    do {
        if (!io_uring_support_sqe_enqueue_readv(
                context->ring,
                channel->fd,
                iov,
                iov_nr,
                offset,
                ((storage_channel_iouring_t*)channel)->base_sqe_flags,
                (uintptr_t)fiber_scheduler_get_current())) {
            fiber_scheduler_set_error(ENOMEM);
            return -ENOMEM;
        }

        // Switch the execution back to the scheduler
        fiber_scheduler_switch_back();

        // When the fiber continues the execution, it has to fetch the return value
        io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

        res = cqe->res;
    } while(res == -EAGAIN);

    if (res < 0) {
        fiber_scheduler_set_error(-res);
    }

    return res;
}

int32_t worker_storage_iouring_op_storage_write(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset) {
    int res;
    worker_iouring_context_t *context = worker_iouring_context_get();

    fiber_scheduler_reset_error();

    do {
        if (!io_uring_support_sqe_enqueue_writev(
                context->ring,
                channel->fd,
                iov,
                iov_nr,
                offset,
                ((storage_channel_iouring_t*)channel)->base_sqe_flags,
                (uintptr_t)fiber_scheduler_get_current())) {
            fiber_scheduler_set_error(ENOMEM);
            return -ENOMEM;
        }

        // Switch the execution back to the scheduler
        fiber_scheduler_switch_back();

        // When the fiber continues the execution, it has to fetch the return value
        io_uring_cqe_t *cqe = (io_uring_cqe_t*)((fiber_scheduler_get_current())->ret.ptr_value);

        res = cqe->res;
    } while(res == -EAGAIN);

    if (res < 0) {
        fiber_scheduler_set_error(-res);
    }

    return res;
}

bool worker_storage_iouring_op_storage_flush(
        storage_channel_t *channel) {
    worker_iouring_context_t *context = worker_iouring_context_get();

    fiber_scheduler_reset_error();

    if (!io_uring_support_sqe_enqueue_fsync(
            context->ring,
            channel->fd,
            0,
            ((storage_channel_iouring_t*)channel)->base_sqe_flags,
            (uintptr_t)fiber_scheduler_get_current())) {
        fiber_scheduler_set_error(ENOMEM);
        return false;
    }

    return worker_storage_iouring_complete_op_simple();
}

bool worker_storage_iouring_op_storage_fallocate(
        storage_channel_t *channel,
        int mode,
        off_t offset,
        off_t len) {
    worker_iouring_context_t *context = worker_iouring_context_get();

    fiber_scheduler_reset_error();

    if (!io_uring_support_sqe_enqueue_fallocate(
            context->ring,
            channel->fd,
            mode,
            offset,
            len,
            ((storage_channel_iouring_t*)channel)->base_sqe_flags,
            (uintptr_t)fiber_scheduler_get_current())) {
        fiber_scheduler_set_error(ENOMEM);
        return false;
    }

    return worker_storage_iouring_complete_op_simple();
}

bool worker_storage_iouring_op_storage_close(
        storage_channel_t *channel) {
    storage_channel_iouring_t *storage_channel_iouring = (storage_channel_iouring_t*)channel;
    fiber_scheduler_reset_error();

    bool res = storage_io_common_close(
            channel->fd);

    if (!res) {
        fiber_scheduler_set_error(errno);
    }

    if (storage_channel_iouring->has_mapped_fd) {
        worker_iouring_fds_map_remove(storage_channel_iouring->mapped_fd);
    }

    storage_channel_iouring_free(storage_channel_iouring);

    return res;
}

bool worker_storage_iouring_initialize(
        __attribute__((unused)) worker_context_t *worker_context) {
    return true;
}

bool worker_storage_iouring_cleanup(
        __attribute__((unused)) worker_context_t *worker_context) {
    return true;
}

bool worker_storage_iouring_op_register() {
    worker_op_storage_open = worker_storage_iouring_op_storage_open;
    worker_op_storage_read = worker_storage_iouring_op_storage_read;
    worker_op_storage_write = worker_storage_iouring_op_storage_write;
    worker_op_storage_flush = worker_storage_iouring_op_storage_flush;
    worker_op_storage_fallocate = worker_storage_iouring_op_storage_fallocate;
    worker_op_storage_close = worker_storage_iouring_op_storage_close;

    return true;
}
