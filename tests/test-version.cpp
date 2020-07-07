#include "catch.hpp"

#include <assert.h>

#include "version.h"

TEST_CASE("version.c", "[version]") {
    SECTION("version_parse") {
        SECTION("parse 1.2.3") {
            long version[4] = {0};
            REQUIRE(version_parse("1.2.3", (long*)&version, sizeof(version)) == 3);

            fprintf(
                    stdout,
                    "version: %ld %ld %ld %ld\n",
                    version[0], version[1], version[2], version[3]);
            fflush(stdout);

            REQUIRE(version[0] == 1);
            REQUIRE(version[1] == 2);
            REQUIRE(version[2] == 3);
        }
        SECTION("parse 1.2.3.4") {
            long version[4] = {0};
            REQUIRE(version_parse("1.2.3.4", (long*)&version, sizeof(version)) == 4);
            REQUIRE(version[0] == 1);
            REQUIRE(version[1] == 2);
            REQUIRE(version[2] == 3);
            REQUIRE(version[3] == 4);
        }
        SECTION("parse 0.0.0.0") {
            long version[4] = {0};
            REQUIRE(version_parse("0.0.0.0", (long*)&version, sizeof(version)) == 4);
            REQUIRE(version[0] == 0);
            REQUIRE(version[1] == 0);
            REQUIRE(version[2] == 0);
            REQUIRE(version[3] == 0);
        }
        SECTION("parse 1.2.3-01234") {
            long version[4] = {0};
            REQUIRE(version_parse("1.2.3-01234", (long*)&version, sizeof(version)) == 4);
            REQUIRE(version[0] == 1);
            REQUIRE(version[1] == 2);
            REQUIRE(version[2] == 3);
            REQUIRE(version[3] == 1234);
        }
        SECTION("invalid version") {
            long version[4] = {0};
            REQUIRE(version_parse("abcdef", (long*)&version, sizeof(version)) == 0);
        }
    }

    SECTION("version_compare") {
        SECTION("test lower") {
        }
        SECTION("test higher") {
        }
        SECTION("test same") {
        }
    }

    SECTION("version_kernel") {
        SECTION("valid kernel version") {
            long kernel_version[4] = {0};
            REQUIRE(version_kernel(kernel_version, 4));
            REQUIRE(kernel_version[0] > 0);

            // The major and minor revision and the patch level may be zero
            REQUIRE(kernel_version[1] >= 0);
            REQUIRE(kernel_version[2] >= 0);
            REQUIRE(kernel_version[3] >= 0);
        }
    }

    SECTION("version_kernel_min") {
        long kernel_version[4] = {0};
        long kernel_version_minor[4] = {0};
        long kernel_version_major[4] = {0};
        assert(version_kernel(kernel_version, 4));
        assert(version_kernel(kernel_version_minor, 4));
        assert(version_kernel(kernel_version_major, 4));

        if (kernel_version_minor[3] > 0) {
            kernel_version_minor[3]--;
        } else if (kernel_version_minor[2] > 0) {
            kernel_version_minor[2]--;
        } else if (kernel_version_minor[1] > 0) {
            kernel_version_minor[1]--;
        } else {
            kernel_version_minor[0]--;
        }

        kernel_version_major[3]++;

        SECTION("test minor kernel version") {
            REQUIRE(!version_kernel_min(kernel_version_minor, 4));
        }

        SECTION("test major kernel version") {
            REQUIRE(version_kernel_min(kernel_version_major, 4));
        }

        SECTION("test same kernel version") {
            REQUIRE(version_kernel_min(kernel_version, 4));
        }
    }
}
