/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <numa.h>
#include <sched.h>

#include "misc.h"
#include "utils_numa.h"

static int numa_node_configured_count = INT32_MAX;
static int numa_cpu_configured_count = INT32_MAX;

bool utils_numa_is_available() {
#if defined(__linux__)
    return numa_available() == -1 ? false : true;
#else
#error Platform not supported
#endif
}

int utils_numa_node_configured_count() {
    if (likely(numa_node_configured_count == INT32_MAX)) {
#if defined(__linux__)
        numa_node_configured_count = numa_num_configured_nodes();
#else
#error Platform not supported
#endif

        // Ensure that a NUMA node gets always reported
        if (numa_node_configured_count == 0) {
            numa_node_configured_count = 1;
        }
    }

    return numa_node_configured_count;
}

uint32_t utils_numa_node_current_index() {
    uint32_t numa_node_index;

#if defined(__linux__)
    getcpu(NULL, &numa_node_index);
#else
#error Platform not supported
#endif

    return numa_node_index;
}

int utils_numa_cpu_configured_count() {
    if (likely(numa_cpu_configured_count == INT32_MAX)) {
#if defined(__linux__)
        numa_cpu_configured_count = numa_num_configured_cpus();
#else
#error Platform not supported
#endif
    }

    return numa_cpu_configured_count;
}

bool utils_numa_cpu_allowed(
        uint32_t cpu_index) {
    return numa_bitmask_isbitset(numa_all_cpus_ptr, cpu_index) == 1;
}

uint32_t utils_numa_cpu_current_index() {
    uint32_t cpu_index;

#if defined(__linux__)
    getcpu(&cpu_index, NULL);
#else
#error Platform not supported
#endif

    return cpu_index;
}
