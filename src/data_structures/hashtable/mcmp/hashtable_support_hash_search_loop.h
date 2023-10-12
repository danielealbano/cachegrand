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

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable.h"
#include "hashtable_support_hash_search.h"

static inline uint32_t hashtable_mcmp_support_hash_search_loop_n(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes,
        uint8_t n) {
    uint32_t return_val = 0;
    for(uint8_t index = 0; index < n; index++) {
        if (hashes[index] == hash) {
            return_val |= 1u << index;
        }
    }

    return return_val;
}

static inline uint32_t hashtable_mcmp_support_hash_search_loop_14(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes) {
    return hashtable_mcmp_support_hash_search_loop_n(hash, hashes, 14);
}
