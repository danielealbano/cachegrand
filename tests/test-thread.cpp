#include <catch2/catch.hpp>

#include <unistd.h>
#include <sys/syscall.h>

#include "thread.h"

TEST_CASE("thread.c", "[thread]") {
    SECTION("thread_current_get_id") {
        REQUIRE(thread_current_get_id() == syscall(SYS_gettid));
    }

    SECTION("thread_current_set_affinity") {
        uint32_t selected_cpus_count = sysconf(_SC_NPROCESSORS_ONLN);
        uint16_t *selected_cpus = (uint16_t*)malloc(sizeof(uint16_t) * selected_cpus_count);
        for (int index = 0; index < selected_cpus_count; index++) {
            selected_cpus[index] = index;
        }
        thread_affinity_set_selected_cpus(selected_cpus, selected_cpus_count);

        SECTION("set affinity to cpu 0") {
            REQUIRE(thread_current_set_affinity(0) == 0);
        }

        SECTION("set affinity to cpu 1") {
            if (selected_cpus_count > 0) {
                    REQUIRE(thread_current_set_affinity(1) == 1);
            } else {
                WARN("Can't test, not enough cores");
            }
        }

        SECTION("affinity max cpu count == 0") {
            REQUIRE(thread_current_set_affinity(selected_cpus_count) == 0);
        }
    }
}
