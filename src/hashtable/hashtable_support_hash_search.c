#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "log.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash_search.h"

hashtable_chunk_slot_index_t hashtable_support_hash_search(
        hashtable_hash_half_t hash,
        hashtable_hash_half_atomic_t* hashes,
        uint32_t skip_indexes)
__attribute__ ((ifunc ("hashtable_support_hash_search_resolve")));

static void *hashtable_support_hash_search_resolve(void)
{
    __builtin_cpu_init();

    LOG_DI("Selecting optimal hashtable_support_hash_search_resolve");

#if defined(__x86_64__)
    LOG_DI("CPU FOUND: %s", "X64");
    LOG_DI(">  HAS AVX: %s", __builtin_cpu_supports("avx") ? "yes" : "no");
    LOG_DI("> HAS AVX2: %s", __builtin_cpu_supports("avx2") ? "yes" : "no");

    if (__builtin_cpu_supports("avx2")) {
        LOG_DI("Selecting AVX2");

        return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(avx2, 14);
    } else if (__builtin_cpu_supports("avx")) {
        LOG_DI("Selecting AVX");

        return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(avx, 14);
    }
#endif

    LOG_DI("No optimization available, selecting loop-based search algorithm");

    return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(loop, 14);
}
