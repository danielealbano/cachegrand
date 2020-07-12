#include "../../catch.hpp"

#include <stdint.h>
#include <arpa/inet.h>

#include "xalloc.h"

#include "network/channel/network_channel_iouring.h"

TEST_CASE("network/channel/network_channel_iouring.c", "[network][channel][network_channel_iouring]") {
    SECTION("network_channel_iouring_entry_user_data_new") {
        network_channel_iouring_entry_user_data_t* user_data =
                network_channel_iouring_entry_user_data_new(NETWORK_IO_IOURING_OP_ACCEPT);

        REQUIRE(user_data != NULL);
        REQUIRE(user_data->op == NETWORK_IO_IOURING_OP_ACCEPT);
    }

    SECTION("network_channel_iouring_entry_user_data_new_with_fd") {
        network_channel_iouring_entry_user_data_t* user_data =
                network_channel_iouring_entry_user_data_new_with_fd(NETWORK_IO_IOURING_OP_ACCEPT, 1);

        REQUIRE(user_data != NULL);
        REQUIRE(user_data->op == NETWORK_IO_IOURING_OP_ACCEPT);
        REQUIRE(user_data->fd == 1);
    }
}
