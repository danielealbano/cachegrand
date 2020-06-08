#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "hashtable.h"
#include "hashtable_support_hash_search.h"

hashtable_bucket_chain_ring_index_t hashtable_support_hash_search_loop_14(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes_mask) {
    uint32_t skip_indexes_mask_inv = ~skip_indexes_mask;
    for(uint8_t index = 0; index < 14; index++) {
        uint32_t index_bitshift = (1u << index);
        if (hashes[index] == hash && (index_bitshift & skip_indexes_mask_inv) == index_bitshift) {
            return index;
        }
    }

    return HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND;
}

hashtable_bucket_chain_ring_index_t hashtable_support_hash_search_loop_8(
        hashtable_bucket_hash_half_t hash,
        hashtable_bucket_hash_half_atomic_t* hashes,
        uint32_t skip_indexes_mask) {
    uint32_t skip_indexes_mask_inv = ~skip_indexes_mask;
    for(uint8_t index = 0; index < 8; index++) {
        uint32_t index_bitshift = (1u << index);
        if (hashes[index] == hash && (index_bitshift & skip_indexes_mask_inv) == index_bitshift) {
            return index;
        }
    }

    return HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND;
}
