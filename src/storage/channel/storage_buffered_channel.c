/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"

#include "storage_buffered_channel.h"

storage_buffered_channel_t* storage_buffered_channel_new(
        storage_channel_t *storage_channel) {
    storage_buffered_channel_t *storage_buffered_channel =
            (storage_buffered_channel_t*)ffma_mem_alloc_zero(sizeof(storage_buffered_channel_t));
    storage_buffered_channel->storage_channel = storage_channel;

    storage_buffered_channel->buffers.write.buffer.data = ffma_mem_alloc(STORAGE_BUFFERED_CHANNEL_BUFFER_SIZE);
    storage_buffered_channel->buffers.write.buffer.length = STORAGE_BUFFERED_CHANNEL_BUFFER_SIZE;
    storage_buffered_channel->buffers.read.buffer.data = ffma_mem_alloc(STORAGE_BUFFERED_CHANNEL_BUFFER_SIZE);
    storage_buffered_channel->buffers.read.buffer.length = STORAGE_BUFFERED_CHANNEL_BUFFER_SIZE;

    return storage_buffered_channel;
}

void storage_buffered_channel_free(
        storage_buffered_channel_t* storage_buffered_channel) {
    ffma_mem_free(storage_buffered_channel->buffers.write.buffer.data);
    ffma_mem_free(storage_buffered_channel->buffers.read.buffer.data);
    ffma_mem_free(storage_buffered_channel);
}
