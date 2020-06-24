#include <benchmark/benchmark.h>

#include "exttypes.h"
#include "spinlock.h"
#include "misc.h"
#include "signal_handler_sigsegv.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash.h"

int main(int argc, char** argv) {
    signal_handler_sigsegv_init();

    fprintf(stdout, "Be aware, benchmarking is configured to:\n");
    fprintf(stdout, "- spin up to 16 threads per core (with a limit to 2048 threads), pinning the threads to the cores\n");
    fprintf(stdout, "- consume up to *120GB* of memory when testing the 2bln key hashtable size\n");
    fprintf(stdout, "- the pre-generated keys are flushed from the cpu data cache at the beginning of each test,\n");
    fprintf(stdout, "  although the test keyset by default contains 1610612736 (1.610bln) with a size of 46.5GB\n");
    fprintf(stdout, "  so there is no chance to fit it in the cpu cache.\n");
    fprintf(stdout, "\n");

    fprintf(stdout, "The benchmarks are running using the following algorithm:\n  %s\n",
#if HASHTABLE_HASH_ALGORITHM_SELECTED == HASHTABLE_HASH_ALGORITHM_T1HA2
            HASHTABLE_HASH_ALGORITHM_T1HA2_STR
#elif HASHTABLE_HASH_ALGORITHM_SELECTED == HASHTABLE_HASH_ALGORITHM_CRC32C
            HASHTABLE_HASH_ALGORITHM_CRC32C_STR
#endif
            );
    fflush(stdout);

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();

    return 0;
}