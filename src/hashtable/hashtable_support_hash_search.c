#include <stdint.h>
#include <string.h>
#include <immintrin.h>

#include "cpu.h"
#include "hashtable_support_hash_search.h"

#if defined(__AVX2__)
int8_t hashtable_support_hash_search_avx2(uint32_t hash, volatile uint32_t* hashes) {
    uint32_t compacted_result_mask, leading_zeroes;
    __m256i cmp_vector, ring_vector, result_mask_vector;
    int8_t found_index = -1;

    if (hashes[0] == hash) {
        return 0;
    }

    cmp_vector = _mm256_set1_epi32(hash);
    for(uint8_t base_index = 0; base_index < 16; base_index += 8) {
        ring_vector = _mm256_stream_load_si256((__m256i*) (hashes + base_index));

        result_mask_vector = _mm256_cmpeq_epi32(ring_vector, cmp_vector);
        compacted_result_mask = _mm256_movemask_epi8(result_mask_vector);

        if (compacted_result_mask != 0) {
            leading_zeroes = 32 - __builtin_clz(compacted_result_mask);
            found_index = base_index + (leading_zeroes >> 2u) - 1;
            break;
        }
    }

    return found_index > 13 ? -1 : found_index;
}
#endif
}

int8_t hashtable_support_hash_search_loop(uint32_t hash, volatile uint32_t* hashes) {
    for(uint8_t index = 0; index < 14; index++) {
        if (hashes[index] == hash) {
            return index;
        }
    }

    return -1;
}

#define HASHTABLE_SUPPORT_HASH_SEARCH_INIT_INSTRUCTION_SET_CHECK(NAME, FP) \
    if (psnip_cpu_feature_check(NAME)) { \
        hashtable_support_hash_search = FP; \
    }

void hashtable_support_hash_search_select_instruction_set() {
    hashtable_support_hash_search = hashtable_support_hash_search_loop;

    HASHTABLE_SUPPORT_HASH_SEARCH_INIT_INSTRUCTION_SET_CHECK(
            PSNIP_CPU_FEATURE_X86_AVX2, hashtable_support_hash_search_avx2);
}
