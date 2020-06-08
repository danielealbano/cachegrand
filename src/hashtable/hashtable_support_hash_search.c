#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "hashtable.h"
#include "hashtable_support_hash_search.h"

hashtable_bucket_chain_ring_index_t hashtable_support_hash_search(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes)
__attribute__ ((ifunc ("hashtable_support_hash_search_resolve")));

static void *hashtable_support_hash_search_resolve(void)
{
#if defined(PSNIP_CPU_ARCH_X86_64)
    if (__builtin_cpu_supports("avx2")) {
        return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(avx2, 8);
    } else if (__builtin_cpu_supports("avx")) {
        return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(avx, 8);
    }
#endif

    return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(loop, 8);
}
