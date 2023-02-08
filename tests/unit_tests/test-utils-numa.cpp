/**
 * Copyright (C) 2018-2022 Vito Castellano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>
#include <numa.h>

#include "utils_numa.h"

TEST_CASE("utils_numa.c", "[utils_numa]") {
    SECTION("utils_numa_is_available") {
        REQUIRE(utils_numa_is_available() == (numa_available() == -1 ? false : true));
    }

    SECTION("utils_numa_node_configured_count") {
        REQUIRE(utils_numa_node_configured_count() == numa_num_configured_nodes());
    }

    SECTION("utils_numa_node_current_index") {
        uint32_t numa_node_index;
        getcpu(NULL, &numa_node_index);

        REQUIRE(utils_numa_node_current_index() == numa_node_index);
    }

    SECTION("utils_numa_cpu_configured_count") {
        REQUIRE(utils_numa_cpu_configured_count() == numa_num_configured_cpus());
    }

    SECTION("utils_numa_cpu_allowed") {
        REQUIRE(utils_numa_cpu_allowed(1) == true);
    }

    SECTION("utils_numa_cpu_current_index") {
        uint32_t cpu_index;
        getcpu(&cpu_index, NULL);

        REQUIRE(utils_numa_cpu_current_index() == cpu_index);
    }
}
