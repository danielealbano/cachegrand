#include <catch2/catch.hpp>

#include <unistd.h>

#include "utils_cpu.h"

TEST_CASE("utils_cpu.c", "[utils_cpu]") {
    SECTION("utils_cpu_count") {
        REQUIRE(utils_cpu_count() == sysconf(_SC_NPROCESSORS_ONLN));
    }

    SECTION("utils_cpu_count_all") {
        REQUIRE(utils_cpu_count_all() == sysconf(_SC_NPROCESSORS_CONF));
    }
}
