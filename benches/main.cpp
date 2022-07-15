/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <numa.h>

#include <benchmark/benchmark.h>

#include "exttypes.h"
#include "spinlock.h"
#include "misc.h"
#include "signals_support.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_support_hash.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "thread.h"
#include "hugepage_cache.h"
#include "slab_allocator.h"


int main(int argc, char** argv) {
    signals_support_register_sigsegv_fatal_handler();

    // Enable the hugepage cache and the slab allocator
    hugepage_cache_init();
    slab_allocator_enable(true);
    slab_allocator_predefined_allocators_init();

    // Ensure that the current thread is pinned to the core 0 otherwise some tests can fail if the kernel shift around
    // the main thread of the process
    thread_current_set_affinity(0);

    fprintf(stdout, "The benchmarks are running using the following hash algorithm:\n  %s\n",
            HASHTABLE_SUPPORT_HASH_NAME);
    fflush(stdout);

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();

    return 0;
}