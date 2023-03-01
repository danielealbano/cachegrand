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
#include "thread.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_console.h"

class BenchmarkProgramSimple {
private:
    const char* tag;

    void setup_initial_log_sink_console() {
        log_level_t level = LOG_LEVEL_ALL;
        log_sink_settings_t settings = { 0 };
        settings.console.use_stdout_for_errors = false;

        log_sink_register(log_sink_console_init(level, &settings));
    }

public:
    explicit BenchmarkProgramSimple(const char *tag) {
        this->tag = tag;
    }

    int Main(int argc, char** argv) {
        signals_support_register_sigsegv_fatal_handler();

        // Setup the log sink
        BenchmarkProgramSimple::setup_initial_log_sink_console();

        // Ensure that the current thread is pinned to the core 0 otherwise some tests can fail if the kernel shift around
        // the main thread of the process
        thread_current_set_affinity(0);

        ::benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
            return 1;
        }
        ::benchmark::RunSpecifiedBenchmarks();

        return 0;
    }
};

int main(int argc, char** argv) {
    return BenchmarkProgramSimple(__FILE__).Main(argc, argv);
}
