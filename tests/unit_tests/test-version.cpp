/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include <assert.h>

#include "version.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE("version.c", "[version]") {
    SECTION("version_parse") {
        SECTION("parse 1.2.3") {
            long version[4] = {0};
            REQUIRE(version_parse("1.2.3", (long*)&version, 3) == 3);

            REQUIRE(version[0] == 1);
            REQUIRE(version[1] == 2);
            REQUIRE(version[2] == 3);
        }

        SECTION("parse 1.2.3.4") {
            long version[4] = {0};
            REQUIRE(version_parse("1.2.3.4", (long*)&version, 4) == 4);
            REQUIRE(version[0] == 1);
            REQUIRE(version[1] == 2);
            REQUIRE(version[2] == 3);
            REQUIRE(version[3] == 4);
        }

        SECTION("parse 0.0.0.0") {
            long version[4] = {0};
            REQUIRE(version_parse("0.0.0.0", (long*)&version, 4) == 4);
            REQUIRE(version[0] == 0);
            REQUIRE(version[1] == 0);
            REQUIRE(version[2] == 0);
            REQUIRE(version[3] == 0);
        }

        SECTION("parse 1.2.3-01234") {
            long version[4] = {0};
            REQUIRE(version_parse("1.2.3-01234", (long*)&version, 4) == 4);
            REQUIRE(version[0] == 1);
            REQUIRE(version[1] == 2);
            REQUIRE(version[2] == 3);
            REQUIRE(version[3] == 1234);
        }

        SECTION("invalid version") {
            long version[4] = {0};
            REQUIRE(version_parse("abcdef", (long*)&version, 4) == 0);
        }
    }

    SECTION("version_compare") {
        long version_base[3] = {0};
        REQUIRE(version_parse("1.5.0", (long*)&version_base, 3) == 3);

        SECTION("test minor") {
            long version_minor[3] = {0};
            REQUIRE(version_parse("1.4.99", (long*)&version_minor, 3) == 3);
            REQUIRE(version_compare(version_minor, version_base, 3) == -1);
        }

        SECTION("test major") {
            long version_major[3] = {0};
            REQUIRE(version_parse("1.5.1", (long*)&version_major, 3) == 3);
            REQUIRE(version_compare(version_major, version_base, 3) == 1);
        }

        SECTION("test same") {
            long version_same[3] = {0};
            REQUIRE(version_parse("1.5.0", (long*)&version_same, 3) == 3);
            REQUIRE(version_compare(version_same, version_base, 3) == 0);
        }
    }

    SECTION("version_kernel") {
        SECTION("valid kernel version") {
            long kernel_version[4] = {0};
            REQUIRE(version_kernel(kernel_version, 3));
            REQUIRE(kernel_version[0] > 0);

            // The major and minor revision and the patch level may be zero
            REQUIRE(kernel_version[1] >= 0);
            REQUIRE(kernel_version[2] >= 0);
        }
    }

    SECTION("version_kernel_min") {
        long kernel_version[3] = {0};
        REQUIRE(version_kernel(kernel_version, 3));

        SECTION("test minor kernel version") {
            long kernel_version_minor[3] = {0};
            REQUIRE(version_kernel(kernel_version_minor, 3));

            if (kernel_version_minor[2] > 0) {
                kernel_version_minor[2]--;
            } else if (kernel_version_minor[1] > 0) {
                kernel_version_minor[1]--;
            } else {
                kernel_version_minor[0]--;
            }

            REQUIRE(version_kernel_min(kernel_version_minor, 3));
        }

        SECTION("test major kernel version") {
            long kernel_version_major[3] = {0};
            REQUIRE(version_kernel(kernel_version_major, 3));

            kernel_version_major[2]++;

            REQUIRE(!version_kernel_min(kernel_version_major, 3));
        }

        SECTION("test same kernel version") {
            REQUIRE(version_kernel_min(kernel_version, 3));
        }
    }
}
