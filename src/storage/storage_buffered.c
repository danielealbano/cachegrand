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
#include <assert.h>

#include "misc.h"
#include "log/log.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/channel/storage_buffered_channel.h"
#include "storage/storage.h"

#include "storage_buffered.h"

#define TAG "storage_buffered"

off_t storage_buffered_get_offset(
        storage_buffered_channel_t *storage_buffered_channel) {
    return storage_buffered_channel->offset;
}

void storage_buffered_set_offset(
        storage_buffered_channel_t *storage_buffered_channel,
        off_t offset) {
    storage_buffered_channel->offset = offset;
}

bool storage_buffered_read_ahead(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t data_size_to_read) {
    size_t data_offset = storage_buffered_channel->buffers.read.buffer.data_offset;
    size_t data_available = storage_buffered_channel->buffers.read.buffer.data_size - data_offset;

    // Move the data in the buffer at the beginning and try to fill out the remaining
    if (unlikely(data_available > storage_buffered_channel->buffers.read.buffer.length >> 1)) {
        // Uses memmove if there will be overlap
        memmove(
                storage_buffered_channel->buffers.read.buffer.data,
                storage_buffered_channel->buffers.read.buffer.data + data_offset,
                data_available);
    } else {
        memcpy(
                storage_buffered_channel->buffers.read.buffer.data,
                storage_buffered_channel->buffers.read.buffer.data + data_offset,
                data_available);
    }

    // Rounds up the data that needs to be read to a full 4KB page and add an extra 4kB page
    data_size_to_read =
            data_size_to_read - (data_size_to_read % STORAGE_BUFFERED_PAGE_SIZE) + (STORAGE_BUFFERED_PAGE_SIZE * 2);

    // Try to read the requested size from the disk, returns what has been found
    size_t read_len = storage_read_try(
            storage_buffered_channel->storage_channel,
            storage_buffered_channel->buffers.read.buffer.data + data_available,
            data_size_to_read,
            storage_buffered_channel->offset);

    storage_buffered_channel->buffers.read.buffer.data_size = data_available + read_len;
    storage_buffered_channel->buffers.read.buffer.data_offset = 0;

    return read_len > 0;
}

bool storage_buffered_flush_write(
        storage_buffered_channel_t *storage_buffered_channel) {
    assert(storage_buffered_channel->buffers.write.slice_acquired_length == 0);

    if (!storage_write(
            storage_buffered_channel->storage_channel,
            storage_buffered_channel->buffers.write.buffer.data,
            storage_buffered_channel->buffers.write.buffer.data_size,
            storage_buffered_channel->offset)) {
        LOG_E(
                TAG,
                "Failed to write buffer with offset <%ld> long <%lu> bytes (path <%s>)",
                storage_buffered_channel->offset,
                storage_buffered_channel->buffers.write.buffer.data_size,
                storage_buffered_channel->storage_channel->path);

        return false;
    }

    storage_buffered_channel->offset += (off_t)storage_buffered_channel->buffers.write.buffer.data_size;
    storage_buffered_channel->buffers.write.buffer.data_size = 0;
    storage_buffered_channel->buffers.write.buffer.data_offset = 0;

    return true;
}
