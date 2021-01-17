#include <catch2/catch.hpp>

#include "exttypes.h"

TEST_CASE("exttypes.h", "[exttypes]") {
    SECTION("sizeof(uint8_volatile_t) == 1") {
        REQUIRE(sizeof(uint8_volatile_t) == 1);
    }

    SECTION("sizeof(uint16_volatile_t) == 2") {
        REQUIRE(sizeof(uint16_volatile_t) == 2);
    }

    SECTION("sizeof(uint32_volatile_t) == 4") {
        REQUIRE(sizeof(uint32_volatile_t) == 4);
    }

    SECTION("sizeof(uint64_volatile_t) == 8") {
        REQUIRE(sizeof(uint64_volatile_t) == 8);
    }

    SECTION("sizeof(uint128_volatile_t) == 16") {
        REQUIRE(sizeof(uint128_volatile_t) == 16);
    }
}
