#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"
#include "xalloc.h"
#include "pow2.h"

#include "hashtable.h"
#include "hashtable_data.h"

hashtable_data_t* hashtable_mcmp_data_init(hashtable_bucket_count_t buckets_count) {
    if (pow2_is(buckets_count) == false) {
        return NULL;
    }

    hashtable_data_t* hashtable_data = (hashtable_data_t*)xalloc_alloc(sizeof(hashtable_data_t));

    hashtable_data->buckets_count =
            buckets_count;
    hashtable_data->buckets_count_real =
            hashtable_data->buckets_count +
            (buckets_count % HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT) +
            (HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX * HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT);
    hashtable_data->chunks_count =
            hashtable_data->buckets_count_real / HASHTABLE_MCMP_HALF_HASHES_CHUNK_SLOTS_COUNT;

    hashtable_data->half_hashes_chunk_size =
            sizeof(hashtable_half_hashes_chunk_volatile_t) * hashtable_data->chunks_count;
    hashtable_data->keys_values_size =
            sizeof(hashtable_key_value_volatile_t) * hashtable_data->buckets_count_real;

    hashtable_data->half_hashes_chunk =
            (hashtable_half_hashes_chunk_volatile_t *)xalloc_mmap_alloc(hashtable_data->half_hashes_chunk_size);
    hashtable_data->keys_values =
            (hashtable_key_value_volatile_t *)xalloc_mmap_alloc(hashtable_data->keys_values_size);

    return hashtable_data;
}

bool hashtable_mcmp_data_numa_interleave_memory(
        hashtable_data_t* hashtable_data,
        struct bitmask* numa_nodes_bitmask) {
    numa_interleave_memory(
            (void*)hashtable_data->half_hashes_chunk,
            hashtable_data->half_hashes_chunk_size,
            numa_nodes_bitmask);
    numa_interleave_memory(
            (void*)hashtable_data->keys_values,
            hashtable_data->keys_values_size,
            numa_nodes_bitmask);
    return true;
}

void hashtable_mcmp_data_free(hashtable_data_t* hashtable_data) {
    xalloc_mmap_free((void*)hashtable_data->half_hashes_chunk, hashtable_data->half_hashes_chunk_size);
    xalloc_mmap_free((void*)hashtable_data->keys_values, hashtable_data->keys_values_size);
    xalloc_free((void*)hashtable_data);
}
