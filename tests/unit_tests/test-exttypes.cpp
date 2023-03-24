/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

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
