#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <immintrin.h>

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash_search.h"

/**
 * AVX2 branchless hash search by @chtz
 *
 * https://stackoverflow.com/a/62123631/169278
 **/
hashtable_bucket_slot_index_t hashtable_support_hash_search_avx2_14(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes_mask) {
    uint32_t compacted_result_mask = 0;
    uint32_t skip_indexes_mask_inv = ~skip_indexes_mask;
    __m256i cmp_vector = _mm256_set1_epi32(hash);

    // The second load, load from the 6th uint32 to the 14th uint32, _mm256_loadu_si256 always loads 8 x uint32
    for(uint8_t base_index = 0; base_index < 12; base_index += 6) {
        __m256i ring_vector = _mm256_loadu_si256((__m256i*) (hashes + base_index));
        __m256i result_mask_vector = _mm256_cmpeq_epi32(ring_vector, cmp_vector);

        // Uses _mm256_movemask_ps to reduce the bandwidth
        compacted_result_mask |= (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(result_mask_vector)) << (base_index);
    }

    return _tzcnt_u32(compacted_result_mask & skip_indexes_mask_inv);
}

hashtable_bucket_slot_index_t hashtable_support_hash_search_avx2_13(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes_mask) {
    uint32_t compacted_result_mask = 0;
    uint32_t skip_indexes_mask_inv = ~skip_indexes_mask;
    __m256i cmp_vector = _mm256_set1_epi32(hash);

    // The second load, load from the 5th uint32 to the 13th uint32, _mm256_loadu_si256 always loads 8 x uint32
    for(uint8_t base_index = 0; base_index < 10; base_index += 5) {
        __m256i ring_vector = _mm256_loadu_si256((__m256i*) (hashes + base_index));
        __m256i result_mask_vector = _mm256_cmpeq_epi32(ring_vector, cmp_vector);

        // Uses _mm256_movemask_ps to reduce the bandwidth
        compacted_result_mask |= (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(result_mask_vector)) << (base_index);
    }

    return _tzcnt_u32(compacted_result_mask & skip_indexes_mask_inv);
}

hashtable_bucket_slot_index_t hashtable_support_hash_search_avx2_8(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes_mask) {
    uint32_t compacted_result_mask = 0;
    uint32_t skip_indexes_mask_inv = ~skip_indexes_mask;
    __m256i cmp_vector = _mm256_set1_epi32(hash);

    __m256i ring_vector = _mm256_loadu_si256((__m256i*) hashes);
    __m256i result_mask_vector = _mm256_cmpeq_epi32(ring_vector, cmp_vector);

    // Uses _mm256_movemask_ps to reduce the bandwidth
    compacted_result_mask = (uint32_t)_mm256_movemask_ps(_mm256_castsi256_ps(result_mask_vector));

    return _tzcnt_u32(compacted_result_mask & skip_indexes_mask_inv);
}
