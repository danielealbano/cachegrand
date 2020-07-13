#include <netinet/in.h>

#include "xalloc.h"

#include "network_channel_iouring.h"

network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new(
        uint32_t op) {
    // TODO: implement slab allocator

    network_channel_iouring_entry_user_data_t *userdata =
            xalloc_alloc_zero(sizeof(network_channel_iouring_entry_user_data_t));
    userdata->op = op;
    userdata->fd = -1;

    return userdata;
}

network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new_with_fd(
        uint32_t op,
        uint32_t fd) {
    network_channel_iouring_entry_user_data_t *userdata =
            network_channel_iouring_entry_user_data_new(op);
    userdata->fd = fd;

    return userdata;
}
