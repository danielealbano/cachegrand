#include <catch2/catch.hpp>

#include <unistd.h>
#include <sys/syscall.h>

#include "thread.h"

TEST_CASE("thread.c", "[thread]") {
    SECTION("thread_current_get_id") {
        REQUIRE(thread_current_get_id() == syscall(SYS_gettid));
    }

    SECTION("thread_current_set_affinity") {
        uint32_t cpus_count = sysconf(_SC_NPROCESSORS_ONLN);

        SECTION("with selected cpus") {
            uint16_t *selected_cpus = (uint16_t*)malloc(sizeof(uint16_t) * cpus_count);
            for (int index = 0; index < cpus_count; index++) {
                selected_cpus[index] = index;
            }
            thread_affinity_set_selected_cpus(selected_cpus, cpus_count);

            SECTION("set affinity to cpu 0") {
                REQUIRE(thread_current_set_affinity(0) == 0);
            }

            SECTION("set affinity to cpu 1") {
                if (cpus_count > 0) {
                        REQUIRE(thread_current_set_affinity(1) == 1);
                } else {
                    WARN("Can't test, not enough cores");
                }
            }

            SECTION("affinity max cpu count == 0") {
                REQUIRE(thread_current_set_affinity(cpus_count) == 0);
            }
        }

        SECTION("without selected cpus") {
            thread_affinity_set_selected_cpus(NULL, 0);

            SECTION("set affinity to cpu 0") {
                REQUIRE(thread_current_set_affinity(0) == 0);
            }

            SECTION("set affinity to cpu 1") {
                if (cpus_count > 0) {
                    REQUIRE(thread_current_set_affinity(1) == 1);
                } else {
                    WARN("Can't test, not enough cores");
                }
            }

            SECTION("affinity max cpu count == 0") {
                REQUIRE(thread_current_set_affinity(cpus_count) == 0);
            }
        }
    }

    SECTION("thread_get_current_core_index") {
        uint32_t core_index;

        getcpu(&core_index, NULL);

        REQUIRE(thread_get_current_core_index() == core_index);
    }

    SECTION("thread_get_current_numa_node_index") {
        uint32_t numa_node_index;

        getcpu(NULL, &numa_node_index);

        REQUIRE(thread_get_current_numa_node_index() == numa_node_index);
    }
}
