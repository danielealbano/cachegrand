/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/channel/storage_channel_iouring.h"

storage_channel_iouring_t* storage_channel_iouring_new() {
    storage_channel_iouring_t *channel =
            (storage_channel_iouring_t*)ffma_mem_alloc_zero(sizeof(storage_channel_iouring_t));

    storage_channel_init(&channel->wrapped_channel);

    return channel;
}

storage_channel_iouring_t* storage_channel_iouring_multi_new(
        uint32_t count) {
    storage_channel_iouring_t *channels =
            (storage_channel_iouring_t*)ffma_mem_alloc_zero(sizeof(storage_channel_iouring_t) * count);

    for(int index = 0; index < count; index++) {
        storage_channel_init(&channels[index].wrapped_channel);
    }

    return channels;
}

void storage_channel_iouring_free(
        storage_channel_iouring_t* storage_channel) {
    ffma_mem_free(storage_channel->wrapped_channel.path);
    ffma_mem_free(storage_channel);
}
