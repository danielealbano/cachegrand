/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>

#include "xalloc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "slab_allocator.h"
#include "xalloc.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"

#include "network_channel_iouring.h"

network_channel_iouring_t* network_channel_iouring_new() {
    network_channel_iouring_t *channel =
            (network_channel_iouring_t*)slab_allocator_mem_alloc_zero(sizeof(network_channel_iouring_t));

    network_channel_init(&channel->wrapped_channel);

    return channel;
}

network_channel_iouring_t* network_channel_iouring_multi_new(
        uint32_t count) {
    network_channel_iouring_t *channels =
            (network_channel_iouring_t*)slab_allocator_mem_alloc_zero(sizeof(network_channel_iouring_t) * count);

    for(int index = 0; index < count; index++) {
        network_channel_init(&channels[index].wrapped_channel);
    }

    return channels;
}

void network_channel_iouring_free(
        network_channel_iouring_t* network_channel) {
    slab_allocator_mem_free(network_channel);
}
