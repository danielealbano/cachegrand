/**
 * Copyright (C) 2020-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <immintrin.h>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable.h"
#include "hashtable_support_hash_search.h"

static inline uint32_t hashtable_mcmp_support_hash_search_avx512f_14(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes) {
    __m512i cmp_vector = _mm512_set1_epi32(hash);

    __m512i ring_vector = _mm512_loadu_si512((__m512i*)hashes);
    uint32_t compacted_result_mask = _mm512_cmpeq_epi32_mask(ring_vector, cmp_vector);

    return compacted_result_mask;
}
