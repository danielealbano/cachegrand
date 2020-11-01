#include <stdbool.h>
#include <netinet/in.h>

#include "xalloc.h"
#include <network/protocol/network_protocol.h>
#include <protocols/resp3/protocol_resp3.h>
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"

#include "network_channel_iouring.h"

network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new(
        uint32_t op) {
    // TODO: implement slab allocator

    network_channel_iouring_entry_user_data_t *userdata =
            xalloc_alloc_zero(sizeof(network_channel_iouring_entry_user_data_t));
    userdata->op = op;

    return userdata;
}

network_channel_iouring_entry_user_data_t* network_channel_iouring_entry_user_data_new_with_mapped_fd(
        uint32_t op,
        network_chanell_iouring_mapped_fd_t mapped_fd) {
    network_channel_iouring_entry_user_data_t *userdata = network_channel_iouring_entry_user_data_new(op);
    userdata->mapped_fd = mapped_fd;

    return userdata;
}

void network_channel_iouring_entry_user_data_free(
        network_channel_iouring_entry_user_data_t* iouring_user_data) {
    if (iouring_user_data->channel) {
        xalloc_free(iouring_user_data->channel);
    }

    if (iouring_user_data->recv_buffer.data) {
        xalloc_free(iouring_user_data->recv_buffer.data);
    }

    if (iouring_user_data->send_buffer.data) {
        xalloc_free(iouring_user_data->send_buffer.data);
    }

    xalloc_free(iouring_user_data);
}
