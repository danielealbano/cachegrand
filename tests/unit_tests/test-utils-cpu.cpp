/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include "utils_cpu.h"

TEST_CASE("utils_cpu.c", "[utils_cpu]") {
    SECTION("utils_cpu_count") {
        REQUIRE(utils_cpu_count() == sysconf(_SC_NPROCESSORS_ONLN));
    }

    SECTION("utils_cpu_count_all") {
        SECTION("fresh data") {
            REQUIRE(utils_cpu_count_all() == sysconf(_SC_NPROCESSORS_CONF));
        }

        SECTION("cached data") {
            REQUIRE(utils_cpu_count_all() == sysconf(_SC_NPROCESSORS_CONF));
        }
    }
}
