/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>

#include "misc.h"
#include "xalloc.h"
#include "utils_cpu.h"
#include "utils_numa.h"
#include "log/log.h"

#include "thread.h"

#define TAG "thread"

thread_local uint32_t thread_current_core_index = UINT32_MAX;
thread_local uint32_t thread_current_numa_node_index = UINT32_MAX;

uint16_t* internal_selected_cpus = NULL;
uint16_t internal_selected_cpus_count = 0;

void thread_flush_cached_core_index_and_numa_node_index() {
    thread_current_core_index = UINT32_MAX;
    thread_current_numa_node_index = UINT32_MAX;
}

long thread_current_get_id() {
    return syscall(SYS_gettid);
}

uint32_t thread_current_set_affinity(
        uint32_t thread_index) {
    int res;
    cpu_set_t cpuset;
    pthread_t thread;
    uint32_t logical_core_index;

    if (internal_selected_cpus == NULL) {
        logical_core_index = thread_index % utils_cpu_count();
    } else {
        logical_core_index = internal_selected_cpus[thread_index % internal_selected_cpus_count];
    }

    CPU_ZERO(&cpuset);
    CPU_SET(logical_core_index, &cpuset);

    thread = pthread_self();
    res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        LOG_E(
                TAG,
                "Unable to set current thread <%ld> affinity to core <%u>",
                thread_current_get_id(),
                logical_core_index);
        LOG_E_OS_ERROR(TAG);
        logical_core_index = 0;
    }

    thread_flush_cached_core_index_and_numa_node_index();

    return logical_core_index;
}

int thread_affinity_set_selected_cpus_sort(
        const void * a,
        const void * b) {
    return (*(uint16_t*)a - *(uint16_t*)b);
}

void thread_affinity_set_selected_cpus(
        uint16_t* selected_cpus,
        uint16_t selected_cpus_count) {
    if (selected_cpus != NULL) {
        qsort(selected_cpus, selected_cpus_count, sizeof(uint16_t), thread_affinity_set_selected_cpus_sort);
    }

    internal_selected_cpus = selected_cpus;
    internal_selected_cpus_count = selected_cpus_count;
}