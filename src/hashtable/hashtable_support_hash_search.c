#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash_search.h"

hashtable_bucket_slot_index_t hashtable_support_hash_search(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes)
__attribute__ ((ifunc ("hashtable_support_hash_search_resolve")));

static void *hashtable_support_hash_search_resolve(void)
{
#if defined(__x86_64__)
    if (__builtin_cpu_supports("avx2")) {
        return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(avx2, 13);
    } else if (__builtin_cpu_supports("avx")) {
        return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(avx, 13);
    }
#endif

    return HASHTABLE_SUPPORT_HASH_SEARCH_METHOD_SIZE(loop, 13);
}
