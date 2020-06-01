#include <stdint.h>
#include <immintrin.h>

#include "cpu.h"
#include "hashtable_support_hash_search.h"

#define HASHTABLE_SUPPORT_HASH_SEARCH_INIT_INSTRUCTION_SET_CHECK(NAME, FP) \
    if (psnip_cpu_feature_check(NAME)) { \
        hashtable_support_hash_search = FP; \
    }

void hashtable_support_hash_search_select_instruction_set() {
    hashtable_support_hash_search = hashtable_support_hash_search_loop;

    HASHTABLE_SUPPORT_HASH_SEARCH_INIT_INSTRUCTION_SET_CHECK(
            PSNIP_CPU_FEATURE_X86_AVX2, hashtable_support_hash_search_avx2);
}
