/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "fiber/fiber.h"
#include "fiber/fiber_scheduler.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "worker/storage/worker_storage_op.h"
#include "worker/worker_stats.h"

#include "storage.h"

#define TAG "storage"

storage_channel_t* storage_open(
        char *path,
        storage_io_common_open_flags_t flags,
        storage_io_common_open_mode_t mode) {
    storage_channel_t *res = worker_op_storage_open(path, flags, mode);

    if (likely(res)) {
        worker_stats_t *stats = worker_stats_get_internal_current();
        stats->storage.open_files++;
    } else {
        int error_number = fiber_scheduler_get_error();
        LOG_E(
                TAG,
                "[OPEN] Error <%s (%d)> opening file <%s> with flags <%d> and mode <%d>",
                strerror(error_number),
                error_number,
                path,
                flags,
                mode);
    }

    return res;
}

storage_channel_t* storage_open_fd(
        storage_io_common_fd_t fd) {
    storage_channel_t *res = worker_op_storage_open_fd(fd);

    if (likely(res)) {
        worker_stats_t *stats = worker_stats_get_internal_current();
        stats->storage.open_files++;
    } else {
        int error_number = fiber_scheduler_get_error();
        LOG_E(
                TAG,
                "[OPEN] Error <%s (%d)> opening fd <%d>",
                strerror(error_number),
                error_number,
                fd);
    }

    return res;
}

int32_t storage_readv_internal(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset) {
    int32_t read_len = (int32_t)worker_op_storage_read(
            channel,
            iov,
            iov_nr,
            offset);

    if (unlikely(read_len < 0)) {
        int error_number = -read_len;
        LOG_E(
                TAG,
                "[FD:%5d][READV] Error <%s (%d)> reading from file <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->path);

        return read_len;
    }

    worker_stats_t *stats = worker_stats_get_internal_current();
    stats->storage.read_data += read_len;
    stats->storage.read_iops++;

    return read_len;
}

bool storage_readv(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        size_t expected_read_len,
        off_t offset) {
    int32_t read_len = (int32_t)storage_readv_internal(
            channel,
            iov,
            iov_nr,
            offset);

    if (unlikely(read_len != expected_read_len)) {
        LOG_E(
                TAG,
                "[FD:%5d][READV] Expected to read <%lu> from <%s>, actually read <%lu>",
                channel->fd,
                expected_read_len,
                channel->path,
                (size_t)read_len);

        return false;
    }

    return read_len >= 0;
}

bool storage_read(
        storage_channel_t *channel,
        char *buffer,
        size_t buffer_len,
        off_t offset) {
    storage_io_common_iovec_t iov[1] = {
            {
                    .iov_base = buffer,
                    .iov_len = buffer_len,
            },
    };

    return storage_readv(channel, iov, 1, buffer_len, offset);
}

int32_t storage_read_try(
        storage_channel_t *channel,
        char *buffer,
        size_t buffer_len,
        off_t offset) {
    storage_io_common_iovec_t iov[1] = {
            {
                    .iov_base = buffer,
                    .iov_len = buffer_len,
            },
    };

    return storage_readv_internal(channel, iov, 1, offset);
}

bool storage_writev(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        size_t expected_write_len,
        off_t offset) {
    int32_t write_len = worker_op_storage_write(
            channel,
            iov,
            iov_nr,
            offset);

    if (unlikely(write_len < 0)) {
        int error_number = -write_len;
        LOG_E(
                TAG,
                "[FD:%5d][WRITEV] Error <%s (%d)> writing to file <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->path);

        return false;
    } else if (unlikely(write_len != expected_write_len)) {
        LOG_E(
                TAG,
                "[FD:%5d][WRITEV] Expected to write <%lu> from <%s>, actually written <%lu>",
                channel->fd,
                expected_write_len,
                channel->path,
                (size_t)write_len);

        return false;
    }

    worker_stats_t *stats = worker_stats_get_internal_current();
    stats->storage.written_data += write_len;
    stats->storage.write_iops++;

    return true;
}

bool storage_write(storage_channel_t *channel,
                          char *buffer,
                          size_t buffer_len,
                          off_t offset) {
    storage_io_common_iovec_t iov[1] = {
            {
                .iov_base = buffer,
                .iov_len = buffer_len,
            },
    };

    return storage_writev(channel, iov, 1, buffer_len, offset);
}

bool storage_flush(
        storage_channel_t *channel) {
    bool res = worker_op_storage_flush(channel);

    if (unlikely(!res)) {
        int error_number = fiber_scheduler_get_error();
        LOG_E(
                TAG,
                "[FD:%5d][FLUSH] Error <%s (%d)> flushing file <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->path);
    }

    return res;
}

bool storage_fallocate(
        storage_channel_t *channel,
        int mode,
        off_t offset,
        off_t len) {
    bool res = worker_op_storage_fallocate(channel, mode, offset, len);

    if (unlikely(!res)) {
        int error_number = fiber_scheduler_get_error();
        LOG_E(
                TAG,
                "[FD:%5d][FALLOCATE] Error <%s (%d)> extending/shrinking file <%s> with mode <%#010x>, offset <%lu> and len <%lu>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->path,
                mode,
                offset,
                len);
    }

    return res;
}

bool storage_close(
        storage_channel_t *channel) {
    bool res = worker_op_storage_close(channel);

    if (likely(res)) {
        worker_stats_t *stats = worker_stats_get_internal_current();
        stats->storage.open_files--;
    } else {
        int error_number = fiber_scheduler_get_error();
        LOG_E(
                TAG,
                "[FD:%5d][CLOSE] Error <%s (%d)> flushing file <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->path);
    }

    return res;
}
