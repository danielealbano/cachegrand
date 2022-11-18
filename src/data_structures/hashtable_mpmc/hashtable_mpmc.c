#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "log/log.h"
#include "fatal.h"
#include "pow2.h"
#include "memory_fences.h"
#include "xalloc.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint64.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "epoch_operation_queue.h"
#include "epoch_gc.h"

#include "hashtable_mpmc.h"

#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
#include "t1ha.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
#include "xxhash.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
#include "hash/hash_crc32c.h"
#else
#error "Unsupported hash algorithm"
#endif

#define TAG "hashtable_mpmc"

// This thread local variable prevents from having more instances of the hashtable but currently this is not required
static thread_local epoch_operation_queue_t *thread_local_operation_queue = NULL;

void hashtable_mpmc_thread_operation_queue_init() {
    thread_local_operation_queue = epoch_operation_queue_init();
}

void hashtable_mpmc_thread_operation_queue_free() {
    epoch_operation_queue_free(thread_local_operation_queue);
}

hashtable_mpmc_hash_t hashtable_mcmp_support_hash_calculate(
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length) {
#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
    return (hashtable_mpmc_hash_t)t1ha2_atonce(key, key_length, HASHTABLE_MPMC_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
    return (hashtable_mpmc_hash_t)XXH3_64bits_withSeed(key, key_length, HASHTABLE_MPMC_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
    uint32_t crc32 = hash_crc32c(key, key_length, HASHTABLE_MPMC_HASH_SEED);
    hashtable_mpmc_hash_t hash = ((uint64_t)hash_crc32c(key, key_length, crc32) << 32u) | crc32;

    return hash;
#endif
}

hashtable_mpmc_data_t *hashtable_mpmc_data_init(
        uint64_t buckets_count) {
    buckets_count = pow2_next(buckets_count);
    uint64_t buckets_count_real = buckets_count + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;

    size_t struct_size = sizeof(hashtable_mpmc_data_t) + (sizeof(hashtable_mpmc_data_bucket_t) * buckets_count_real);
    hashtable_mpmc_data_t *hashtable_mpmc_data = (hashtable_mpmc_data_t *)xalloc_mmap_alloc(struct_size);

    if (memset(hashtable_mpmc_data, 0, struct_size) != hashtable_mpmc_data) {
        FATAL(TAG, "Unable to zero the requested memory %lu", struct_size);
    }

    hashtable_mpmc_data->struct_size = struct_size;
    hashtable_mpmc_data->buckets_count = buckets_count;
    hashtable_mpmc_data->buckets_count_real = buckets_count_real;
    hashtable_mpmc_data->buckets_count_mask = buckets_count - 1;

    return hashtable_mpmc_data;
}

void hashtable_mpmc_data_free(
        hashtable_mpmc_data_t *hashtable_mpmc_data) {
    for(
            hashtable_mpmc_bucket_index_t bucket_index = 0;
            bucket_index < hashtable_mpmc_data->buckets_count_real;
            bucket_index++) {
        if (hashtable_mpmc_data->buckets[bucket_index].data.hash_half == 0) {
            continue;
        }

        uintptr_t key_value_ptr = (uintptr_t)hashtable_mpmc_data->buckets[bucket_index].data.key_value & ~0x01;
        hashtable_mpmc_data_key_value_t *key_value = (hashtable_mpmc_data_key_value_t*)key_value_ptr;

        if (!key_value->key_is_embedded) {
            xalloc_free(key_value->key.external.key);
        }
        xalloc_free((void*)key_value);
    }

    xalloc_mmap_free(hashtable_mpmc_data, hashtable_mpmc_data->struct_size);
}

hashtable_mpmc_t *hashtable_mpmc_init(
        uint64_t buckets_count) {
    hashtable_mpmc_t *hashtable_mpmc = (hashtable_mpmc_t *)xalloc_alloc_zero(sizeof(hashtable_mpmc_t));
    hashtable_mpmc->data = hashtable_mpmc_data_init(buckets_count);

    return hashtable_mpmc;
}

void hashtable_mpmc_free(
        hashtable_mpmc_t *hashtable_mpmc) {
    hashtable_mpmc_data_free(hashtable_mpmc->data);
    xalloc_free(hashtable_mpmc);
}

hashtable_mpmc_hash_half_t hashtable_mpmc_support_hash_half(
        hashtable_mpmc_hash_t hash) {
    return hash & 0xFFFFFFFF;
}

hashtable_mpmc_bucket_index_t hashtable_mpmc_support_bucket_index_from_hash(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash) {
    return (hash >> 32) & hashtable_mpmc_data->buckets_count_mask;
}

bool hashtable_mpmc_support_get_bucket_and_key_value(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash,
        hashtable_mpmc_hash_half_t hash_half,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        bool allow_temporary,
        hashtable_mpmc_data_bucket_t *return_bucket,
        hashtable_mpmc_bucket_index_t *return_bucket_index) {
    bool found = false;
    hashtable_mpmc_bucket_index_t bucket_index;

    // The bucket index is calculated by the upper half of the hash as the lower half is the one stored in the buckets
    // array, we would face plenty of collisions otherwise. The bucket index is calculated using an AND operation as the
    // number of buckets is always a power of 2 and a mask is pre-calculated and stored in buckets_count_mask.
    hashtable_mpmc_bucket_index_t bucket_index_start = hashtable_mpmc_support_bucket_index_from_hash(
            hashtable_mpmc_data,
            hash);

    // The extra buckets that might be searched if bucket_index_start is exactly the last element will always be present
    // because they are allocated in hashtable_mpmc_data_init
    for(
            bucket_index = bucket_index_start;
            bucket_index < bucket_index_start + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
            bucket_index++) {
        MEMORY_FENCE_LOAD();
        if (hashtable_mpmc_data->buckets[bucket_index].data.hash_half != hash_half) {
            continue;
        }

        uintptr_t key_value_uintptr_value = (uintptr_t)hashtable_mpmc_data->buckets[bucket_index].data.key_value;
        bool is_temporary = (key_value_uintptr_value & 0x01) == 0x01;

        // The least significant bit of the pointer to the key_value is used to track if the entry is still being added
        // or not. It's better to leave the hash unaltered, so it can be compared using SIMD instructions.
        if (unlikely(!allow_temporary && is_temporary)) {
            continue;
        }

        hashtable_mpmc_data_key_value_volatile_t *key_value = (void*)(key_value_uintptr_value & ~0x07);

        // Compare the key
        bool does_key_match = false;
        if (key_value->key_is_embedded) {
            does_key_match =
                    key_value->key.embedded.key_length == key_length &&
                    strncmp((char *)key_value->key.embedded.key, key, key_length) == 0;
        } else {
            does_key_match =
                    key_value->key.external.key_length == key_length &&
                    strncmp((char *)key_value->key.external.key, key, key_length) == 0;
        }

        // If the hash matches is extremely likely the key will match as well, this branch can be marked with unlikely
        // for better performances
        if (unlikely(!does_key_match)) {
            continue;
        }

        // Update the return bucket
        return_bucket->_packed = hashtable_mpmc_data->buckets[bucket_index]._packed;
        *return_bucket_index = bucket_index;
        found = true;
    }

    return found;
}

uintptr_t hashtable_mpmc_op_get(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length) {
    uintptr_t value;
    hashtable_mpmc_data_bucket_t bucket;
    hashtable_mpmc_bucket_index_t bucket_index;
    hashtable_mpmc_hash_t hash = hashtable_mcmp_support_hash_calculate(key, key_length);

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation = epoch_operation_queue_enqueue(
            thread_local_operation_queue);

    bool found = hashtable_mpmc_support_get_bucket_and_key_value(
            hashtable_mpmc->data,
            hash,
            hashtable_mpmc_support_hash_half(hash),
            key,
            key_length,
            false,
            &bucket,
            &bucket_index);

    if (found) {
        // Fetch the value
        value = bucket.data.key_value->value;
    }

    // Mark the operation as completed
    epoch_operation_queue_mark_completed(operation);

    return found ? value : 0;
}

bool hashtable_mpmc_op_delete(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length) {
    hashtable_mpmc_data_bucket_t found_bucket;
    hashtable_mpmc_bucket_index_t found_bucket_index;
    hashtable_mpmc_hash_t hash = hashtable_mcmp_support_hash_calculate(key, key_length);

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation = epoch_operation_queue_enqueue(
            thread_local_operation_queue);

    // Try to search for the key
    bool found = hashtable_mpmc_support_get_bucket_and_key_value(
            hashtable_mpmc->data,
            hash,
            hashtable_mpmc_support_hash_half(hash),
            key,
            key_length,
            false,
            &found_bucket,
            &found_bucket_index);

    // Try to empty the bucket, if the operation is successful, stage the key_value pointer to be garbage collected
    if (__atomic_compare_exchange_n(
            &hashtable_mpmc->data->buckets[found_bucket_index]._packed,
            (uint128_t*)&found_bucket._packed,
            0,
            false,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        epoch_gc_stage_object(
                EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE,
                (void*)found_bucket.data.key_value);
    }

    // Mark the operation as completed
    epoch_operation_queue_mark_completed(operation);

    return found;
}

bool hashtable_mpmc_op_set(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t value,
        bool *return_created_new,
        bool *return_value_updated) {
    bool retry_loop;
    bool result = false;
    hashtable_mpmc_data_bucket_t found_bucket, new_bucket;
    hashtable_mpmc_bucket_index_t found_bucket_index, new_bucket_index;
    hashtable_mpmc_hash_t hash = hashtable_mcmp_support_hash_calculate(key, key_length);
    hashtable_mpmc_hash_half_t hash_half = hashtable_mpmc_support_hash_half(hash);

    *return_created_new = false;
    *return_value_updated = false;

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation = epoch_operation_queue_enqueue(
            thread_local_operation_queue);

    // Uses a 3-phase approach:
    // - searches first for a bucket with a matching hash
    // - if it doesn't find it, it searches for an empty bucket and update it marking it as still being added
    // - searches again the entire range to see if some other thread did set the value, if it finds a match marked
    //   as being added will drop the bucket and restart the process from the first step otherwise will drop the bucket
    //   and drop the update operation as well as another thread finished the insert operation before the one being
    //   processed
    do {
        retry_loop = false;

        // Try to find the value in the hashtable
        bool found = hashtable_mpmc_support_get_bucket_and_key_value(
                hashtable_mpmc->data,
                hash,
                hash_half,
                key,
                key_length,
                true,
                &found_bucket,
                &found_bucket_index);

        // If the bucket was previously found, try to swap it with the new bucket
        if (found) {
            // If the value is found check if there is a transaction in progress or if the temporary bit is set and in
            // case abort the set operation and return it as successful as:
            // - if there is a transaction, it will potentially override the change anyway
            // - if the temporary bit is set the value is still being set, but it doesn't make sense to wait as the
            //   execution is in the middle of a race and therefore doesn't matter if the value is discarded

            bool is_temporary = ((uintptr_t)found_bucket.data.key_value & 0x01) == 0x01;
            if (found_bucket.data.transaction_id.id != 0 || is_temporary) {
                goto end;
            }

            uintptr_t expected_value = found_bucket.data.key_value->value;

            *return_value_updated = __atomic_compare_exchange_n(
                    &hashtable_mpmc->data->buckets[found_bucket_index].data.key_value->value,
                    &expected_value,
                    value,
                    false,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_ACQUIRE);

            // As the key is owned by the hashtable, the pointer is freed as well
            xalloc_free(key);

            result = true;
            goto end;
        }

        // If the bucket hasn't been found, a new one needs to be inserted and therefore the key_value struct has to be
        // allocated
        hashtable_mpmc_data_key_value_t *new_key_value = xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        new_key_value->value = value;

        if (key_length <= sizeof(new_key_value->key.embedded.key)) {
            // The key can be embedded
            strncpy(new_key_value->key.embedded.key, key, key_length);
            new_key_value->key.embedded.key_length = key_length;
            new_key_value->key_is_embedded = true;
        } else {
            // The key is too large and can't be embedded
            new_key_value->key.external.key = key;
            new_key_value->key.external.key_length = key_length;
            new_key_value->key_is_embedded = false;
        }

        // Prepare the new bucket, the new_key_value pointer points to the key_value struct already prepared but with
        // the least significant bit set to 1 to indicate that it's a temporary allocation.
        new_bucket.data.transaction_id.id = 0;
        new_bucket.data.hash_half = hash_half;
        new_bucket.data.key_value = (void*)((uintptr_t)new_key_value | 0x01);

        hashtable_mpmc_bucket_index_t new_bucket_index_start = (hash >> 32) & hashtable_mpmc->data->buckets_count_mask;
        bool empty_bucket_found = false;
        for(
                new_bucket_index = new_bucket_index_start;
                new_bucket_index < new_bucket_index_start + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
                new_bucket_index++) {
            // No need for an atomic operation here, the compare and exchange will fail if the value is not actually
            // zero. It's extremely likely anyway that the current CPU will have an up-to-date value because of all the
            // full memory fences triggered by the atomic operations in addition to the ad-hoc ones.
            if (hashtable_mpmc->data->buckets[new_bucket_index]._packed != 0) {
                // Skip the non-empty value
                continue;
            }

            // Initialize the value of the expected bucket to 0
            uint128_t expected_bucket_packed_value = 0;

            if (__atomic_compare_exchange_n(
                    &hashtable_mpmc->data->buckets[new_bucket_index]._packed,
                    &expected_bucket_packed_value,
                    new_bucket._packed,
                    false,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_ACQUIRE)) {
                empty_bucket_found = true;
                break;
            }
        }

        // If no empty bucket has been found the hashtable is full and needs resizing
        if (!empty_bucket_found) {
            goto end;
        }

        for(
                found_bucket_index = new_bucket_index_start;
                found_bucket_index < new_bucket_index_start + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
                found_bucket_index++) {
            MEMORY_FENCE_LOAD();
            if (hashtable_mpmc->data->buckets[found_bucket_index].data.hash_half != hash_half) {
                continue;
            }

            uintptr_t key_value_uintptr_value =
                    (uintptr_t)hashtable_mpmc->data->buckets[found_bucket_index].data.key_value;
            hashtable_mpmc_data_key_value_volatile_t *found_key_value = (void*)(key_value_uintptr_value & ~0x07);

            // Compare the key
            bool does_key_match = false;
            if (found_key_value->key_is_embedded) {
                does_key_match =
                        found_key_value->key.embedded.key_length == key_length &&
                        strncmp((char *)found_key_value->key.embedded.key, key, key_length) == 0;
            } else {
                does_key_match =
                        found_key_value->key.external.key_length == key_length &&
                        strncmp((char *)found_key_value->key.external.key, key, key_length) == 0;
            }

            // If the hash matches is extremely likely the key will match as well, this branch can be marked with unlikely
            // for better performances
            if (unlikely(!does_key_match)) {
                continue;
            }

            retry_loop = true;
            break;
        }
    } while(retry_loop);

    *return_created_new = true;
    hashtable_mpmc->data->buckets[new_bucket_index].data.key_value =
            (void*)((uintptr_t)hashtable_mpmc->data->buckets[new_bucket_index].data.key_value & ~0x07);

    // No need for an atomic operation, the value can be safely overwritten as by algorithm no other thread will touch
    // the bucket
    MEMORY_FENCE_STORE();

end:

    // Mark the operation as completed
    epoch_operation_queue_mark_completed(operation);

    return result;
}
