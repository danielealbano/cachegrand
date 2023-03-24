/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"

TEST_CASE("storage/channel/storage_channel.c", "[storage][storage_channel][storage_channel]") {
    SECTION("storage_channel_init") {
        storage_channel_t storage_channel = { 0 };

        REQUIRE(storage_channel_init(&storage_channel));
    }

    SECTION("storage_channel_new") {
        storage_channel_t* storage_channel = storage_channel_new();

        REQUIRE(storage_channel != NULL);

        storage_channel_free(storage_channel);
    }

    SECTION("storage_channel_multi_new") {
        storage_channel_t* storage_channel = storage_channel_multi_new(3);

        REQUIRE(storage_channel != NULL);
        for(int i = 0; i < 3; i++) {
            REQUIRE(storage_channel[i].fd == 0);
        }

        storage_channel_free(storage_channel);
    }
}
