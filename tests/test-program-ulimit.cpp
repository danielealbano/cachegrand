#include <catch2/catch.hpp>

#include <limits.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "memory_fences.h"

#include "program_ulimit.h"

TEST_CASE("program_ulimit.c", "[program][program_ulimit]") {
    SECTION("program_ulimit_wrapper") {
        ulong current_value;
        struct rlimit limit;

        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        current_value = limit.rlim_cur;

        REQUIRE(program_ulimit_wrapper(RLIMIT_NOFILE, current_value - 1));
        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        REQUIRE(limit.rlim_cur == current_value - 1);
        REQUIRE(limit.rlim_max == current_value - 1);
    }

    SECTION("program_ulimit_nofile") {
        SECTION("allowed value") {
            REQUIRE(program_ulimit_set_nofile(0x40000));
        }

        SECTION("not allowed value") {
            REQUIRE_FALSE(program_ulimit_set_nofile(-1));
        }
    }

    SECTION("program_ulimit_memlock") {
        SECTION("allowed value") {
            CHECK_NOFAIL(program_ulimit_set_memlock(0xFFFFFFFFUL));
        }

        SECTION("not allowed value") {
            REQUIRE_FALSE(program_ulimit_set_memlock(ULONG_MAX));
        }
    }

    SECTION("program_ulimit_setup") {
        struct rlimit limit;

        program_ulimit_setup();

        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        CHECK_NOFAIL(limit.rlim_cur == PROGRAM_ULIMIT_NOFILE);
        CHECK_NOFAIL(limit.rlim_max == PROGRAM_ULIMIT_NOFILE);

        REQUIRE(getrlimit(RLIMIT_MEMLOCK, &limit) == 0);
        CHECK_NOFAIL(limit.rlim_cur == PROGRAM_ULIMIT_MEMLOCK);
        CHECK_NOFAIL(limit.rlim_max == PROGRAM_ULIMIT_MEMLOCK);
    }

    SECTION("program_ulimit_wrapper - limits") {
        // The test changes on purpose the current limit to ensure that it's not going to match the one being set
        // by the code, and it sets it to current - 1 to avoid hitting system limits.
        struct rlimit limit;

        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        REQUIRE(program_ulimit_wrapper(RLIMIT_NOFILE, limit.rlim_max - 1));

        REQUIRE(getrlimit(RLIMIT_MEMLOCK, &limit) == 0);
        REQUIRE(program_ulimit_wrapper(RLIMIT_MEMLOCK, limit.rlim_max - 1));

        SECTION("RLIMIT_NOFILE") {
            CHECK_NOFAIL(program_ulimit_wrapper(RLIMIT_MEMLOCK, PROGRAM_ULIMIT_MEMLOCK) == true);

            // Don't test the limits if the call failed
            if (program_ulimit_wrapper(RLIMIT_NOFILE, PROGRAM_ULIMIT_NOFILE)) {
                REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
                REQUIRE(limit.rlim_cur == PROGRAM_ULIMIT_NOFILE);
                REQUIRE(limit.rlim_max == PROGRAM_ULIMIT_NOFILE);
            }
        }

        SECTION("RLIMIT_MEMLOCK") {
            CHECK_NOFAIL(program_ulimit_wrapper(RLIMIT_MEMLOCK, PROGRAM_ULIMIT_MEMLOCK) == true);

            // Don't test the limits if the call failed
            if (program_ulimit_wrapper(RLIMIT_MEMLOCK, PROGRAM_ULIMIT_MEMLOCK)) {
                REQUIRE(getrlimit(RLIMIT_MEMLOCK, &limit) == 0);
                REQUIRE(limit.rlim_cur == PROGRAM_ULIMIT_MEMLOCK);
                REQUIRE(limit.rlim_max == PROGRAM_ULIMIT_MEMLOCK);
            }
        }
    }
}
