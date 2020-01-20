#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <t1ha.h>

#include "hashtable.h"
#include "hashtable_support_index.h"
#include "hashtable_support_primenumbers.h"


uint64_t hashtable_support_index_rounddown_to_cacheline(uint64_t number) {
    return number -
           (number % HASHTABLE_HASHES_PER_CACHELINE);
}

uint64_t hashtable_support_index_roundup_to_cacheline_to_probe(uint64_t number, uint16_t cachelines_to_probe) {
    return
            hashtable_support_index_rounddown_to_cacheline(number) +
            (HASHTABLE_HASHES_PER_CACHELINE * cachelines_to_probe);
}

hashtable_bucket_index_t hashtable_support_index_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash) {
    return hashtable_support_primenumbers_mod(hash, buckets_count);
}

void hashtable_support_index_calculate_neighborhood_from_index(
        hashtable_bucket_index_t index,
        uint16_t cachelines_to_probe,
        hashtable_bucket_index_t *index_neighborhood_begin,
        hashtable_bucket_index_t *index_neighborhood_end) {
    *index_neighborhood_begin = hashtable_support_index_rounddown_to_cacheline(index);
    *index_neighborhood_end = hashtable_support_index_roundup_to_cacheline_to_probe(index, cachelines_to_probe) - 1;
}

void hashtable_support_index_calculate_neighborhood_from_hash(
        hashtable_bucket_count_t buckets_count,
        hashtable_bucket_hash_t hash,
        uint16_t cachelines_to_probe,
        hashtable_bucket_index_t *index_neighborhood_begin,
        hashtable_bucket_index_t *index_neighborhood_end) {
    uint64_t index = hashtable_support_index_from_hash(buckets_count, hash);
    hashtable_support_index_calculate_neighborhood_from_index(
            index,
            cachelines_to_probe,
            index_neighborhood_begin,
            index_neighborhood_end);
}
