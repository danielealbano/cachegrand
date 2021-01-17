#include <catch2/catch.hpp>

#include <unistd.h>
#include <sys/syscall.h>

#include "thread.h"

TEST_CASE("thread.c", "[thread]") {
    SECTION("thread_current_get_id") {
        REQUIRE(thread_current_get_id() == syscall(SYS_gettid));
    }

    SECTION("thread_current_set_affinity") {
        uint32_t available_cores = sysconf(_SC_NPROCESSORS_ONLN);

        SECTION("set affinity to cpu 0") {
            REQUIRE(thread_current_set_affinity(0) == 0);
        }

        SECTION("set affinity to cpu 2") {
            REQUIRE(thread_current_set_affinity(1) == 2);
        }

        SECTION("set affinity to cpu 1") {
            REQUIRE(thread_current_set_affinity(available_cores >> 1u) == 1);
        }

        SECTION("set affinity to cpu 2 with high thread index") {
            REQUIRE(thread_current_set_affinity(available_cores * 4 + 1) == 2);
        }
    }
}
