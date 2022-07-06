#include <catch2/catch.hpp>

#include <sys/types.h>
#include <sys/resource.h>

#include "memory_fences.h"

#include "program_ulimit.h"

TEST_CASE("program_ulimit.c", "[program][ulimit]") {
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
        REQUIRE(program_ulimit_set_nofile(0x70000));
        REQUIRE_FALSE(program_ulimit_set_nofile(-1));
    }

    SECTION("program_ulimit_memlock") {
        CHECK_NOFAIL(program_ulimit_set_memlock(0xFFFFFFFFUL));
    }

    SECTION("program_ulimit_setup") {
        // The test changes on purpose the current limit to ensure that it's not going to match the one being set
        // by the code, and it sets it to current - 1 to avoid hitting system limits.
        struct rlimit limit;

        REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
        REQUIRE(program_ulimit_wrapper(RLIMIT_NOFILE, limit.rlim_max - 1));

        REQUIRE(getrlimit(RLIMIT_MEMLOCK, &limit) == 0);
        REQUIRE(program_ulimit_wrapper(RLIMIT_MEMLOCK, limit.rlim_max - 1));

        CHECK_NOFAIL(program_ulimit_wrapper(RLIMIT_MEMLOCK, PROGRAM_ULIMIT_MEMLOCK) == true);
        if (program_ulimit_wrapper(RLIMIT_NOFILE, PROGRAM_ULIMIT_NOFILE)) {
            REQUIRE(getrlimit(RLIMIT_NOFILE, &limit) == 0);
            REQUIRE(limit.rlim_cur == PROGRAM_ULIMIT_NOFILE);
            REQUIRE(limit.rlim_max == PROGRAM_ULIMIT_NOFILE);
        }

        CHECK_NOFAIL(program_ulimit_wrapper(RLIMIT_MEMLOCK, PROGRAM_ULIMIT_MEMLOCK) == true);
        if (program_ulimit_wrapper(RLIMIT_MEMLOCK, PROGRAM_ULIMIT_MEMLOCK)) {
            REQUIRE(getrlimit(RLIMIT_MEMLOCK, &limit) == 0);
            REQUIRE(limit.rlim_cur == PROGRAM_ULIMIT_MEMLOCK);
            REQUIRE(limit.rlim_max == PROGRAM_ULIMIT_MEMLOCK);
        }
    }
}