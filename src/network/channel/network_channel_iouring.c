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

#include "xalloc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
#include "config.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"

#include "network_channel_iouring.h"

network_channel_iouring_t* network_channel_iouring_new(
        network_channel_type_t type) {
    network_channel_iouring_t *channel =
            (network_channel_iouring_t*)xalloc_alloc_zero(sizeof(network_channel_iouring_t));

    network_channel_init(type, &channel->wrapped_channel);

    return channel;
}

network_channel_iouring_t* network_channel_iouring_multi_new(
        network_channel_type_t type,
        uint32_t count) {
    network_channel_iouring_t *channels =
            (network_channel_iouring_t*)xalloc_alloc_zero(sizeof(network_channel_iouring_t) * count);

    for(int index = 0; index < count; index++) {
        network_channel_init(type, &channels[index].wrapped_channel);
    }

    return channels;
}

void network_channel_iouring_multi_free(
        network_channel_iouring_t *channels,
        uint32_t count) {
    for(int index = 0; index < count; index++) {
        network_channel_cleanup(&channels[index].wrapped_channel);
    }

    xalloc_free(channels);
}

void network_channel_iouring_free(
        network_channel_iouring_t* network_channel) {
    network_channel_cleanup(&network_channel->wrapped_channel);
    xalloc_free(network_channel);
}
