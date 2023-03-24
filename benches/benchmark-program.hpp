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
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "thread.h"
#include "hugepage_cache.h"
#include "xalloc.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_console.h"

class BenchmarkProgram {
private:
    const char* tag;

    void setup_initial_log_sink_console() {
        log_level_t level = LOG_LEVEL_ALL;
        log_sink_settings_t settings = { 0 };
        settings.console.use_stdout_for_errors = false;

        log_sink_register(log_sink_console_init(level, &settings));
    }

public:
    explicit BenchmarkProgram(const char *tag) {
        this->tag = tag;
    }

    int Main(int argc, char** argv) {
        signals_support_register_sigsegv_fatal_handler();

        // Enable the hugepage cache and the slab allocator
        hugepage_cache_init();
        ffma_enable(true);

        // Setup the log sink
        BenchmarkProgram::setup_initial_log_sink_console();

        // Ensure that the current thread is pinned to the core 0 otherwise some tests can fail if the kernel shift around
        // the main thread of the process
        thread_current_set_affinity(0);

        LOG_I(this->tag, "The benchmarks are running using the following hash algorithm: %s", HASHTABLE_SUPPORT_HASH_NAME);

        ::benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
            return 1;
        }
        ::benchmark::RunSpecifiedBenchmarks();

        return 0;
    }
};

int main(int argc, char** argv) {
    return BenchmarkProgram(__FILE__).Main(argc, argv);
}
