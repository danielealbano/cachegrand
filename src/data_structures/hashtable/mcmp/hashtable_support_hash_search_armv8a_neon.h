/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <numa.h>
#include <arm_neon.h>

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable.h"
#include "hashtable_support_hash_search.h"

static inline uint32_t hashtable_mcmp_support_hash_search_armv8a_neon_14(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes) {
    uint32x4_t tmp;
    uint32_t compacted_result_mask = 0;
    static const int32x4_t shift = {0, 1, 2, 3};

    uint32x4_t cmp_vector = vdupq_n_u32(hash);

    uint32x4_t ring_vector_0_3 = vld1q_u32((hashtable_hash_half_t*)hashes + 0);
    uint32x4_t cmp_vector_0_3 = vceqq_u32(ring_vector_0_3, cmp_vector);
    tmp = vshrq_n_u32(cmp_vector_0_3, 31);
    compacted_result_mask |=  vaddvq_u32(vshlq_u32(tmp, shift)) << 0;

    uint32x4_t ring_vector_4_7 = vld1q_u32((hashtable_hash_half_t*)hashes + 4);
    uint32x4_t cmp_vector_4_7 = vceqq_u32(ring_vector_4_7, cmp_vector);
    tmp = vshrq_n_u32(cmp_vector_4_7, 31);
    compacted_result_mask |=  vaddvq_u32(vshlq_u32(tmp, shift)) << 4;

    uint32x4_t ring_vector_8_11 = vld1q_u32((hashtable_hash_half_t*)hashes + 8);
    uint32x4_t cmp_vector_8_11 = vceqq_u32(ring_vector_8_11, cmp_vector);
    tmp = vshrq_n_u32(cmp_vector_8_11, 31);
    compacted_result_mask |=  vaddvq_u32(vshlq_u32(tmp, shift)) << 8;

    uint32x4_t ring_vector_10_13 = vld1q_u32((hashtable_hash_half_t*)hashes + 10);
    uint32x4_t cmp_vector_10_13 = vceqq_u32(ring_vector_10_13, cmp_vector);
    tmp = vshrq_n_u32(cmp_vector_10_13, 31);
    compacted_result_mask |=  vaddvq_u32(vshlq_u32(tmp, shift)) << 10;

    return compacted_result_mask;
}
