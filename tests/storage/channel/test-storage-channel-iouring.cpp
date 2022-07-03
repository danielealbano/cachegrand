/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <arpa/inet.h>

#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/channel/storage_channel_iouring.h"

TEST_CASE("storage/channel/storage_channel_iouring.c", "[storage][storage_channel][storage_channel_iouring]") {
    SECTION("storage_channel_iouring_new") {
        storage_channel_iouring_t* storage_channel_iouring = storage_channel_iouring_new();

        REQUIRE(storage_channel_iouring != NULL);

        storage_channel_iouring_free(storage_channel_iouring);
    }

    SECTION("storage_channel_iouring_multi_new") {
        storage_channel_iouring_t* storage_channel_iouring = storage_channel_iouring_multi_new(3);

        REQUIRE(storage_channel_iouring != NULL);
        for(int i = 0; i < 3; i++) {
            REQUIRE(storage_channel_iouring[i].wrapped_channel.fd == 0);
        }

        storage_channel_iouring_free(storage_channel_iouring);
    }
}
