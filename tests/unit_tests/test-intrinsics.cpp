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

#include "intrinsics.h"

double test_intrinsics_read_cpu_freq_from_cpuinfo() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;

    while (std::getline(cpuinfo, line)) {
        if (line.find("cpu MHz") != std::string::npos) {
            std::size_t pos = line.find(':');
            double cpu_freq = std::stod(line.substr(pos + 1));
            return cpu_freq * 1000000; // Convert MHz to Hz
        }
    }
    throw std::runtime_error("Cannot find 'cpu MHz' in /proc/cpuinfo");
}

TEST_CASE("intrinsics.c", "[intrinsics]") {
    SECTION("intrinsics_cycles_per_second_calculate") {
        uint64_t cycles_per_second = intrinsics_cycles_per_second_calculate();
        double cpu_freq_from_cpuinfo = test_intrinsics_read_cpu_freq_from_cpuinfo();

        // Since there can be some variation in the frequency, we will use a tolerance
        double tolerance = 0.02 * cpu_freq_from_cpuinfo; // 2% tolerance

        REQUIRE(cycles_per_second >= (cpu_freq_from_cpuinfo - tolerance));
        REQUIRE(cycles_per_second <= (cpu_freq_from_cpuinfo + tolerance));
    }
}
