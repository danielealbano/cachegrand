/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/channel/storage_buffered_channel.h"

TEST_CASE("storage/channel/storage_buffered_channel.c", "[storage][storage_channel][storage_buffered_channel]") {
    SECTION("storage_buffered_channel_new") {
        storage_channel_t *storage_channel = storage_channel_new();
        storage_buffered_channel_t* storage_buffered_channel = storage_buffered_channel_new(storage_channel);

        REQUIRE(storage_buffered_channel != NULL);
        REQUIRE(storage_buffered_channel->storage_channel == storage_channel);
        REQUIRE(storage_buffered_channel->offset == 0);
        REQUIRE(storage_buffered_channel->buffers.read.buffer.data != NULL);
        REQUIRE(storage_buffered_channel->buffers.read.buffer.data_size == 0);
        REQUIRE(storage_buffered_channel->buffers.read.buffer.data_offset == 0);
        REQUIRE(storage_buffered_channel->buffers.read.buffer.length == STORAGE_BUFFERED_CHANNEL_BUFFER_SIZE);
        REQUIRE(storage_buffered_channel->buffers.write.buffer.data != NULL);
        REQUIRE(storage_buffered_channel->buffers.write.buffer.data_size == 0);
        REQUIRE(storage_buffered_channel->buffers.write.buffer.data_offset == 0);
        REQUIRE(storage_buffered_channel->buffers.write.buffer.length == STORAGE_BUFFERED_CHANNEL_BUFFER_SIZE);

        storage_buffered_channel_free(storage_buffered_channel);
        storage_channel_free(storage_channel);
    }
}
