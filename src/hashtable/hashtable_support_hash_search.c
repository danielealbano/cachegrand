#include <stdint.h>
#include <string.h>
#include <immintrin.h>

#include "hashtable_support_hash_search.h"

#if defined(__AVX512F__)
int8_t hashtable_support_hash_search_avx512(uint32_t hash, volatile uint32_t* hashes) {
    __mmask16  compacted_result_mask;
    __m512i cmp_vector, ring_vector;

    if (hashes[0] == hash) {
        return 0;
    }

    cmp_vector = _mm512_set1_epi32(hash);
    ring_vector = _mm512_stream_load_si512((__m512i*)hashes);

    compacted_result_mask = _mm512_cmpeq_epi32_mask(ring_vector, cmp_vector);

    return 31 - __builtin_clz(compacted_result_mask);
}
#endif

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

#if defined(__SSE4_2__)
int8_t hashtable_support_hash_search_sse42(uint32_t hash, volatile uint32_t* hashes) {
    uint32_t compacted_result_mask, leading_zeroes;
    __m128i cmp_vector, ring_vector, result_mask_vector;
    int8_t found_index = -1;

    if (hashes[0] == hash) {
        return 0;
    }

    cmp_vector = _mm_set1_epi32 (hash);
    for(uint8_t base_index = 0; base_index < 14; base_index += 4) {
        ring_vector = _mm_stream_load_si128((__m128i*) (hashes + base_index));

        result_mask_vector = _mm_cmpeq_epi32(ring_vector, cmp_vector);
        compacted_result_mask = _mm_movemask_epi8(result_mask_vector);

        if (compacted_result_mask != 0) {
            leading_zeroes = 32 - __builtin_clz(compacted_result_mask);
            found_index = base_index + (leading_zeroes >> 2u) - 1;
            break;
        }
    }

    return found_index > 13 ? -1 : found_index;
}
#endif

int8_t hashtable_support_hash_search_loop(uint32_t hash, volatile uint32_t* hashes) {
    for(uint8_t index = 0; index < 14; index++) {
        if (hashes[index] == hash) {
            return index;
        }
    }

    return -1;
}

#define hashtable_support_hash_search_init_instruction_set_check(NAME, FP) \
    if (__builtin_cpu_supports(NAME)) { \
        hashtable_support_hash_search = FP; \
         \
        if (prefer_instruction_set != NULL && strcmp(prefer_instruction_set, NAME) == 0) { \
            return hashtable_support_hash_search; \
        } \
    }

hashtable_support_hash_search_fp_t hashtable_support_hash_search_select_instruction_set(char* prefer_instruction_set) {
    hashtable_support_hash_search = hashtable_support_hash_search_loop;

#if defined(__SSE4_2__) || defined(__AVX2__) || defined(__AVX512__)
    __builtin_cpu_init();

#if defined(__SSE4_2__)
    hashtable_support_hash_search_init_instruction_set_check("sse4.2", hashtable_support_hash_search_sse42);
#endif // defined(__SSE4_2__)

#if defined(__AVX2__)
    hashtable_support_hash_search_init_instruction_set_check("avx2", hashtable_support_hash_search_avx2);
#endif // defined(__AVX2__)

#if defined(__AVX512__)
    hashtable_support_hash_search_init_instruction_set_check("avx512f", hashtable_support_hash_search_avx512);
#endif // defined(__AVX512__)

#endif // defined(__SSE4_2__) || defined(__AVX2__) || defined(__AVX512__)

    return hashtable_support_hash_search;
}
