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
#include <string.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "fiber.h"
#include "fiber_scheduler.h"
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
        worker_stats_t *stats = worker_stats_get();
        stats->storage.total.open_files++;
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

bool storage_readv(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        size_t expected_read_len,
        off_t offset) {
    int32_t read_len = (int32_t)worker_op_storage_read(
            channel,
            iov,
            iov_nr,
            offset);

    if (unlikely(read_len< 0)) {
        int error_number = -read_len;
        LOG_E(
                TAG,
                "[FD:%5d][READV] Error <%s (%d)> reading from file <%s>",
                channel->fd,
                strerror(error_number),
                error_number,
                channel->path);

        return false;
    } else if (unlikely(read_len != expected_read_len)) {
        LOG_E(
                TAG,
                "[FD:%5d][READV] Expected to read <%lu> from <%s>, actually read <%lu>",
                channel->fd,
                expected_read_len,
                channel->path,
                (size_t)read_len);

        return false;
    }

    LOG_D(
            TAG,
            "[FD:%5d][READV] Received <%u> bytes from path <%s>",
            channel->fd,
            read_len,
            channel->path);

    worker_stats_t *stats = worker_stats_get();
    stats->storage.per_second.read_data += read_len;
    stats->storage.total.read_data += read_len;
    stats->storage.per_second.read_iops++;
    stats->storage.total.read_iops++;

    return true;
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

    LOG_D(
            TAG,
            "[FD:%5d][WRITEV] Written <%u> bytes to path <%s>",
            channel->fd,
            write_len,
            channel->path);

    worker_stats_t *stats = worker_stats_get();
    stats->storage.per_second.written_data += write_len;
    stats->storage.total.written_data += write_len;
    stats->storage.per_second.write_iops++;
    stats->storage.total.write_iops++;

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
        worker_stats_t *stats = worker_stats_get();
        stats->storage.total.open_files--;
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
