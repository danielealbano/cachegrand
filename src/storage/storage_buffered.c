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
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
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

storage_buffered_channel_buffer_data_t *storage_buffered_write_buffer_acquire_slice(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t slice_length) {
    // Ensure that the slice requested can fit into the buffer and that there isn't already a slice acquired
    assert(slice_length <= storage_buffered_channel->buffers.write.buffer.length);
    assert(storage_buffered_channel->buffers.write.slice_acquired_length == 0);

    // Check if there is enough space on the buffer, if not flush it
    if (unlikely(storage_buffered_channel->buffers.write.buffer.data_size + slice_length >
        storage_buffered_channel->buffers.write.buffer.length)) {
        if (unlikely(!storage_buffered_flush_write(storage_buffered_channel))) {
            return NULL;
        }
    }

#if DEBUG == 1
    storage_buffered_channel->buffers.write.slice_acquired_length = slice_length;
#endif

    return storage_buffered_channel->buffers.write.buffer.data +
        storage_buffered_channel->buffers.write.buffer.data_offset;
}

void storage_buffered_write_buffer_release_slice(
        storage_buffered_channel_t *storage_buffered_channel,
        size_t slice_used_length) {
    // Ensure that when the slice is released, the amount of data used is always the same or smaller than the length
    // acquired. Also ensure that there was a slice acquired.
    assert(storage_buffered_channel->buffers.write.slice_acquired_length > 0);
    assert(slice_used_length <= storage_buffered_channel->buffers.write.slice_acquired_length);

    storage_buffered_channel->buffers.write.buffer.data_size += slice_used_length;
    storage_buffered_channel->buffers.write.buffer.data_offset += slice_used_length;

#if DEBUG == 1
    storage_buffered_channel->buffers.write.slice_acquired_length = 0;
#endif
}
