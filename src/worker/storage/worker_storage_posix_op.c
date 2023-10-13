/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "fiber/fiber.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "memory_allocator/ffma.h"
#include "config.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "fiber/fiber_scheduler.h"
#include "worker/storage/worker_storage_op.h"
#include "worker/storage/worker_storage_posix_op.h"

storage_channel_t* worker_storage_posix_op_storage_open(
        char *path,
        storage_io_common_open_flags_t flags,
        storage_io_common_open_mode_t mode) {

    fiber_scheduler_reset_error();

    storage_io_common_fd_t fd = open(path, flags, mode);

    if (fd < 0) {
        fiber_scheduler_set_error(errno);
        return NULL;
    }

    storage_channel_t *storage_channel = storage_channel_new();
    storage_channel->fd = fd;
    storage_channel->path = path;
    storage_channel->path_len = strlen(path);

    return storage_channel;
}

storage_channel_t* worker_storage_posix_op_storage_open_fd(
        storage_io_common_fd_t fd) {
    char temp_fd_link_path[PATH_MAX];
    char temp_fd_path[PATH_MAX];

    // Read the path of the fd
    snprintf(temp_fd_link_path, PATH_MAX - 1, "/proc/self/fd/%d", fd);
    size_t res = readlink(temp_fd_link_path, temp_fd_path, PATH_MAX - 1);

    // If the fd is not valid, return NULL
    if (res == -1) {
        return NULL;
    }

    // Copy the path to a new buffer
    char *path = xalloc_alloc_zero(strlen(temp_fd_path) + 1);
    if (!path) {
        return NULL;
    }

    strcpy(path, temp_fd_path);

    storage_channel_t *storage_channel = storage_channel_new();
    storage_channel->fd = fd;
    storage_channel->path = path;
    storage_channel->path_len = strlen(path);

    return storage_channel;
}

int32_t worker_storage_posix_op_storage_read(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset) {
    int32_t res;

    fiber_scheduler_reset_error();

    do {
        res = (int32_t)preadv(channel->fd, iov, (int)iov_nr, offset);
    } while(res == -1 && errno == -EAGAIN);

    if (res < 0) {
        fiber_scheduler_set_error(errno);
        res = -errno;
    }

    return res;
}

int32_t worker_storage_posix_op_storage_write(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset) {
    int32_t res;

    fiber_scheduler_reset_error();

    do {
        res = (int32_t)pwritev(channel->fd, iov, (int)iov_nr, offset);
    } while(res == -1 && errno == -EAGAIN);

    if (res < 0) {
        fiber_scheduler_set_error(errno);
        res = -errno;
    }

    return res;
}

bool worker_storage_posix_op_storage_flush(
        storage_channel_t *channel) {
    fiber_scheduler_reset_error();

    bool res = fsync(channel->fd) == 0 ? true : false;

    if (!res) {
        fiber_scheduler_set_error(errno);
    }

    return res;
}

bool worker_storage_posix_op_storage_fallocate(
        storage_channel_t *channel,
        int mode,
        off_t offset,
        off_t len) {
    fiber_scheduler_reset_error();

    bool res = fallocate(channel->fd, mode, offset, len) == 0 ? true : false;

    if (!res) {
        fiber_scheduler_set_error(errno);
    }

    return res;
}

bool worker_storage_posix_op_storage_close(
        storage_channel_t *channel) {
    fiber_scheduler_reset_error();

    bool res = storage_io_common_close(channel->fd);

    if (!res) {
        fiber_scheduler_set_error(errno);
    }

    return res;
}

bool worker_storage_posix_initialize(
        __attribute__((unused)) worker_context_t *worker_context) {
    return true;
}

bool worker_storage_posix_cleanup(
        __attribute__((unused)) worker_context_t *worker_context) {
    // TODO: will need to iterate all over the opened files and flush the data to ensure they are stored on the disk
    return true;
}

bool worker_storage_posix_op_register() {
    worker_op_storage_open = worker_storage_posix_op_storage_open;
    worker_op_storage_open_fd = worker_storage_posix_op_storage_open_fd;
    worker_op_storage_read = worker_storage_posix_op_storage_read;
    worker_op_storage_write = worker_storage_posix_op_storage_write;
    worker_op_storage_flush = worker_storage_posix_op_storage_flush;
    worker_op_storage_fallocate = worker_storage_posix_op_storage_fallocate;
    worker_op_storage_close = worker_storage_posix_op_storage_close;

    return true;
}
