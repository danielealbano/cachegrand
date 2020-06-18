#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support_hash_search.h"

hashtable_chunk_slot_index_t hashtable_support_hash_search_loop_n(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes,
        uint32_t skip_indexes_mask,
        uint8_t n) {
    uint32_t skip_indexes_mask_inv = ~skip_indexes_mask;
    for(uint8_t index = 0; index < n; index++) {
        uint32_t index_bitshift = (1u << index);
        if (hashes[index] == hash && (index_bitshift & skip_indexes_mask_inv) == index_bitshift) {
            return index;
        }
    }

    return HASHTABLE_SUPPORT_HASH_SEARCH_NOT_FOUND;
}

hashtable_chunk_slot_index_t hashtable_support_hash_search_loop_14(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes,
        uint32_t skip_indexes_mask) {
    return hashtable_support_hash_search_loop_n(hash, hashes, skip_indexes_mask, 14);
}

hashtable_chunk_slot_index_t hashtable_support_hash_search_loop_13(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes,
        uint32_t skip_indexes_mask) {
    return hashtable_support_hash_search_loop_n(hash, hashes, skip_indexes_mask, 13);
}

hashtable_chunk_slot_index_t hashtable_support_hash_search_loop_8(
        hashtable_hash_half_t hash,
        hashtable_hash_half_volatile_t* hashes,
        uint32_t skip_indexes_mask) {
    return hashtable_support_hash_search_loop_n(hash, hashes, skip_indexes_mask, 8);
}
