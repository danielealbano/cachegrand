#include <benchmark/benchmark.h>

#include "signal_handler_sigsegv.h"

int main(int argc, char** argv) {
    signal_handler_sigsegv_init();

    fprintf(stdout, "Be aware, benchmarking is configured to:\n");
    fprintf(stdout, "- spin up 4 threads per core and pin the threads\n");
    fprintf(stdout, "- consume up to 120GB of memory when testing the 2bln key hashtable size\n");
    fprintf(stdout, "- the keys used for the benches are regenerated at every run, it's very time consuming with the big hashtables\n");
    fprintf(stdout, "\n");

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();

    return 0;
}