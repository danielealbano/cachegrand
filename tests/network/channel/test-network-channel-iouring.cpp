#include <catch2/catch.hpp>

#include <stdint.h>
#include <arpa/inet.h>

#include "xalloc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"

#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"

TEST_CASE("network/channel/network_channel_iouring.c", "[network][channel][network_channel_iouring]") {
    SECTION("network_channel_iouring_new") {
        network_channel_iouring_t* network_channel_iouring = network_channel_iouring_new();

        REQUIRE(network_channel_iouring != NULL);
        REQUIRE(network_channel_iouring->wrapped_channel.address.size ==
            sizeof(network_channel_iouring->wrapped_channel.address.socket));

        network_channel_iouring_free(network_channel_iouring);
    }

    SECTION("network_channel_iouring_new_multi") {
        network_channel_iouring_t* network_channel_iouring = network_channel_iouring_new_multi(3);

        REQUIRE(network_channel_iouring != NULL);
        for(int i = 0; i < 3; i++) {
            REQUIRE(network_channel_iouring[i].wrapped_channel.address.size ==
                    sizeof(network_channel_iouring[i].wrapped_channel.address.socket));
        }

        network_channel_iouring_free(network_channel_iouring);
    }
}
