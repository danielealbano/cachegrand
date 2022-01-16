/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
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
#include "slab_allocator.h"
#include "storage/io/storage_io_common.h"

#include "storage_channel.h"

bool storage_channel_init(
        storage_channel_t *channel) {
    return true;
}

storage_channel_t* storage_channel_new() {
    storage_channel_t *channel =
            (storage_channel_t*)slab_allocator_mem_alloc_zero(sizeof(storage_channel_t));

    storage_channel_init(channel);

    return channel;
}

storage_channel_t* storage_channel_multi_new(
        uint32_t count) {
    storage_channel_t *channels =
            (storage_channel_t*)slab_allocator_mem_alloc_zero(sizeof(storage_channel_t) * count);

    for(int index = 0; index < count; index++) {
        storage_channel_init(&channels[index]);
    }

    return channels;
}

void storage_channel_free(
        storage_channel_t* storage_channel) {
    slab_allocator_mem_free(storage_channel);
}
