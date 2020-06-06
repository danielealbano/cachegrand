#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "cpu.h"
#include "hashtable.h"
#include "hashtable_support_hash_search.h"

static bool hashtable_support_hash_search_select_instruction_set_invoked = false;

#define HASHTABLE_SUPPORT_HASH_SEARCH_INIT_INSTRUCTION_SET_CHECK(NAME, FP) \
    if (psnip_cpu_feature_check(NAME)) { \
        hashtable_support_hash_search = FP; \
    }

void hashtable_support_hash_search_select_instruction_set() {
    if (hashtable_support_hash_search_select_instruction_set_invoked) {
        return;
    }

    hashtable_support_hash_search = hashtable_support_hash_search_loop;

#if defined(PSNIP_CPU_ARCH_X86_64)
    HASHTABLE_SUPPORT_HASH_SEARCH_INIT_INSTRUCTION_SET_CHECK(
            PSNIP_CPU_FEATURE_X86_AVX, hashtable_support_hash_search_avx);
    HASHTABLE_SUPPORT_HASH_SEARCH_INIT_INSTRUCTION_SET_CHECK(
            PSNIP_CPU_FEATURE_X86_AVX2, hashtable_support_hash_search_avx2);
#endif
    
    hashtable_support_hash_search_select_instruction_set_invoked = true;
}
