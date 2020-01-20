#include <benchmark/benchmark.h>

#include "signal_handler_sigsegv.h"

int main(int argc, char** argv) {
    signal_handler_sigsegv_init();

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();

    return 0;
}