/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include "pow2.h"

TEST_CASE("pow2.c", "[pow2]") {
    SECTION("pow2_is") {
        SECTION("valid power of 2 numbers") {
            REQUIRE(pow2_is(0x000Fu + 1u));
            REQUIRE(pow2_is(0x00FFu + 1u));
            REQUIRE(pow2_is(0x0FFFu + 1u));
            REQUIRE(pow2_is(0xFFFFu + 1u));
        }
        SECTION("invalid power of 2 numbers") {
            REQUIRE(!pow2_is(0x000Fu));
            REQUIRE(!pow2_is(0x00FFu));
            REQUIRE(!pow2_is(0x0FFFu));
            REQUIRE(!pow2_is(0xFFFFu));
        }
    }

    SECTION("pow2_next_pow2m1") {
        REQUIRE(pow2_next_pow2m1(0x000Fu - 1u) == 0x000Fu);
        REQUIRE(pow2_next_pow2m1(0x00FFu - 1u) == 0x00FFu);
        REQUIRE(pow2_next_pow2m1(0x0FFFu - 1u) == 0x0FFFu);
        REQUIRE(pow2_next_pow2m1(0xFFFFu - 1u) == 0xFFFFu);
    }

    SECTION("pow2_next") {
        REQUIRE(pow2_next(0x000Fu) == 0x000Fu + 1u);
        REQUIRE(pow2_next(0x00FFu) == 0x00FFu + 1u);
        REQUIRE(pow2_next(0x0FFFu) == 0x0FFFu + 1u);
        REQUIRE(pow2_next(0xFFFFu) == 0xFFFFu + 1u);
    }
}
