/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>
#include <cstdint>
#include <unistd.h>

#if defined(__x86_64__)
#include <cpuid.h>
#endif

#include "intrinsics.h"

TEST_CASE("intrinsics.c", "[intrinsics]") {
    SECTION("intrinsics_frequency_max") {
        uint64_t cycles_per_second = intrinsics_frequency_max_calculate();
        uint64_t cpu_freq_simple = intrinsics_frequency_max_calculate_simple();

        REQUIRE(cycles_per_second == cpu_freq_simple);
    }
}
