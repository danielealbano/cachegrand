/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/fast_fixed_memory_allocator.h"
#include "storage/io/storage_io_common.h"

#include "storage_channel.h"

bool storage_channel_init(
        storage_channel_t *channel) {
    // The function here is for future usage but as it does nothing currently LGTM reports is as not having side effects
    // so it's setting the fd field of the channel to 0, which is safe to do as this function initialize the storage
    // channel right after creation.
    channel->fd = 0;
    return true;
}

storage_channel_t* storage_channel_new() {
    storage_channel_t *channel =
            (storage_channel_t*)fast_fixed_memory_allocator_mem_alloc_zero(sizeof(storage_channel_t));

    storage_channel_init(channel);

    return channel;
}

storage_channel_t* storage_channel_multi_new(
        uint32_t count) {
    storage_channel_t *channels =
            (storage_channel_t*)fast_fixed_memory_allocator_mem_alloc_zero(sizeof(storage_channel_t) * count);

    for(int index = 0; index < count; index++) {
        storage_channel_init(&channels[index]);
    }

    return channels;
}

void storage_channel_free(
        storage_channel_t* storage_channel) {
    fast_fixed_memory_allocator_mem_free(storage_channel);
}
