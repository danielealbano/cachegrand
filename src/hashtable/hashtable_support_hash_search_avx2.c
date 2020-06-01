#include <stdint.h>
#include <immintrin.h>
#include <x86intrin.h>

/**
 * AVX2 branchless hash search by @chtz
 *
 * https://stackoverflow.com/a/62123631/169278
 **/
__attribute__((noinline)) int8_t hashtable_support_hash_search_avx2(uint32_t hash, uint32_t* hashes) {
#if defined(__AVX2__)
    uint32_t compacted_result_mask = 0;
    __m256i cmp_vector = _mm256_set1_epi32(hash);

    // The second load, load from the 6th uint32 to the 14th uint32, _mm256_loadu_si256 always loads 8 x uint32
    for(uint8_t base_index = 0; base_index < 12; base_index += 6) {
        __m256i ring_vector = _mm256_loadu_si256((__m256i*) (hashes + base_index));

        __m256i result_mask_vector = _mm256_cmpeq_epi32(ring_vector, cmp_vector);
        compacted_result_mask |= (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(result_mask_vector)) << (base_index);
    }
    int32_t leading_zeros = __lzcnt32(compacted_result_mask);
    return (31 - leading_zeros);
#else
    // do nothing
#endif
}
