#include <benchmark/benchmark.h>

#include "exttypes.h"
#include "spinlock.h"
#include "misc.h"
#include "signals_support.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash.h"

int main(int argc, char** argv) {
    signals_support_register_sigsegv_fatal_handler();
    
    fprintf(stdout, "Be aware, benchmarking is configured to:\n");
    fprintf(stdout, "- spin up to 16 threads per core (with a limit to 2048 threads), pinning the threads to the cores\n");
    fprintf(stdout, "- consume up to *120GB* of memory when testing the 2bln key hashtable size\n");
    fprintf(stdout, "- the pre-generated keys are flushed from the cpu data cache at the beginning of each test,\n");
    fprintf(stdout, "  although the test keyset by default contains 1610612736 (1.610bln) with a size of 46.5GB\n");
    fprintf(stdout, "  so there is no chance to fit it in the cpu cache.\n");
    fprintf(stdout, "\n");

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