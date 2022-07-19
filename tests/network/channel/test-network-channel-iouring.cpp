/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <arpa/inet.h>

#include "config.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"

TEST_CASE("network/channel/network_channel_iouring.c", "[network][network_channel][network_channel_iouring]") {
    SECTION("network_channel_iouring_new") {
        SECTION("NETWORK_CHANNEL_TYPE_CLIENT") {
            network_channel_iouring_t* network_channel_iouring = network_channel_iouring_new(
                    NETWORK_CHANNEL_TYPE_CLIENT);

            REQUIRE(network_channel_iouring != NULL);
            REQUIRE(network_channel_iouring->wrapped_channel.address.size ==
                    sizeof(network_channel_iouring->wrapped_channel.address.socket));
            REQUIRE(network_channel_iouring->wrapped_channel.buffers.send.length != NETWORK_CHANNEL_SEND_BUFFER_SIZE);
            REQUIRE(network_channel_iouring->wrapped_channel.buffers.send.data != NULL);

            network_channel_iouring_free(network_channel_iouring);
        }

        SECTION("NETWORK_CHANNEL_TYPE_LISTENER") {
            network_channel_iouring_t* network_channel_iouring = network_channel_iouring_new(
                    NETWORK_CHANNEL_TYPE_LISTENER);

            REQUIRE(network_channel_iouring != NULL);
            REQUIRE(network_channel_iouring->wrapped_channel.address.size ==
                    sizeof(network_channel_iouring->wrapped_channel.address.socket));
            REQUIRE(network_channel_iouring->wrapped_channel.buffers.send.data == NULL);

            network_channel_iouring_free(network_channel_iouring);
        }
    }

    SECTION("network_channel_iouring_multi_new") {
        SECTION("NETWORK_CHANNEL_TYPE_CLIENT") {
            network_channel_iouring_t *network_channel_iouring = network_channel_iouring_multi_new(
                    NETWORK_CHANNEL_TYPE_CLIENT,
                    3);

            REQUIRE(network_channel_iouring != NULL);
            for (int i = 0; i < 3; i++) {
                REQUIRE(network_channel_iouring[i].wrapped_channel.address.size ==
                        sizeof(network_channel_iouring[i].wrapped_channel.address.socket));
                REQUIRE(network_channel_iouring->wrapped_channel.buffers.send.length != NETWORK_CHANNEL_SEND_BUFFER_SIZE);
                REQUIRE(network_channel_iouring->wrapped_channel.buffers.send.data != NULL);
            }

            network_channel_iouring_free(network_channel_iouring);
        }

        SECTION("NETWORK_CHANNEL_TYPE_LISTENER") {
            network_channel_iouring_t *network_channel_iouring = network_channel_iouring_multi_new(
                    NETWORK_CHANNEL_TYPE_LISTENER,
                    3);

            REQUIRE(network_channel_iouring != NULL);
            for (int i = 0; i < 3; i++) {
                REQUIRE(network_channel_iouring[i].wrapped_channel.address.size ==
                        sizeof(network_channel_iouring[i].wrapped_channel.address.socket));
                REQUIRE(network_channel_iouring->wrapped_channel.buffers.send.data == NULL);
            }

            network_channel_iouring_free(network_channel_iouring);
        }
    }
}
