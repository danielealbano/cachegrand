#include <catch2/catch.hpp>

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
