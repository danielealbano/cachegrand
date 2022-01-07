#include <catch2/catch.hpp>

#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"

TEST_CASE("storage/channel/storage_channel.c", "[storage][channel][storage_channel]") {
    SECTION("storage_channel_init") {
        storage_channel_t storage_channel = { 0 };

        REQUIRE(storage_channel_init(&storage_channel));
    }
}
