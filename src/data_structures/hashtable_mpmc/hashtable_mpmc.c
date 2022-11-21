#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "fatal.h"
#include "pow2.h"
#include "memory_fences.h"
#include "intrinsics.h"
#include "xalloc.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint64.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "spinlock.h"
#include "transaction.h"
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

// TODO: need to review get during the upsize !!!
// TODO: need to review deletion during the upsize !!!
// TODO: use the epoch gc for hashtable_mpmc_data to free it

#define TAG "hashtable_mpmc"

// This thread local variable prevents from having more instances of the hashtable but currently this is not required
static thread_local epoch_operation_queue_t *thread_local_epoch_operation_queue_hashtable_key_value = NULL;
static thread_local epoch_operation_queue_t *thread_local_epoch_operation_queue_hashtable_data = NULL;

FUNCTION_CTOR(hashtable_mpmc_epoch_gc_object_type_hashtable_key_value_destructor_cb_init, {
    epoch_gc_register_object_type_destructor_cb(
            EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE,
            hashtable_mpmc_epoch_gc_object_type_hashtable_key_value_destructor_cb);
    epoch_gc_register_object_type_destructor_cb(
            EPOCH_GC_OBJECT_TYPE_HASHTABLE_DATA,
            hashtable_mpmc_epoch_gc_object_type_hashtable_data_destructor_cb);
})

void hashtable_mpmc_epoch_gc_object_type_hashtable_key_value_destructor_cb(
        uint8_t staged_objects_count,
        epoch_gc_staged_object_t staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]) {
    for(uint8_t index = 0; index < staged_objects_count; index++) {
        hashtable_mpmc_data_key_value_t *key_value = staged_objects[index].data.object;
        if (!key_value->key_is_embedded) {
            xalloc_free(key_value->key.external.key);
        }

        xalloc_free(key_value);
    }
}

void hashtable_mpmc_epoch_gc_object_type_hashtable_data_destructor_cb(
        uint8_t staged_objects_count,
        epoch_gc_staged_object_t staged_objects[EPOCH_GC_STAGED_OBJECT_DESTRUCTOR_CB_BATCH_SIZE]) {
    for(uint8_t index = 0; index < staged_objects_count; index++) {
        hashtable_mpmc_data_t *hashtable_mpmc_data = staged_objects[index].data.object;
        hashtable_mpmc_data_free(hashtable_mpmc_data);
    }
}

void hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init() {
    thread_local_epoch_operation_queue_hashtable_key_value = epoch_operation_queue_init();
}

void hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free() {
    epoch_operation_queue_free(thread_local_epoch_operation_queue_hashtable_key_value);
}

void hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init() {
    thread_local_epoch_operation_queue_hashtable_data = epoch_operation_queue_init();
}

void hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free() {
    epoch_operation_queue_free(thread_local_epoch_operation_queue_hashtable_data);
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

    size_t struct_size = sizeof(hashtable_mpmc_data_t) + (sizeof(hashtable_mpmc_bucket_t) * buckets_count_real);
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

        hashtable_mpmc_data_key_value_volatile_t *key_value =
                HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(hashtable_mpmc_data->buckets[bucket_index]);

        if (!key_value->key_is_embedded) {
            xalloc_free(key_value->key.external.key);
        }

        xalloc_free((void*)key_value);
    }

    xalloc_mmap_free(hashtable_mpmc_data, hashtable_mpmc_data->struct_size);
}

hashtable_mpmc_t *hashtable_mpmc_init(
        uint64_t buckets_count,
        uint64_t buckets_count_max,
        uint16_t upsize_preferred_block_size) {
    hashtable_mpmc_t *hashtable_mpmc = (hashtable_mpmc_t *)xalloc_alloc_zero(sizeof(hashtable_mpmc_t));

    hashtable_mpmc->data = hashtable_mpmc_data_init(buckets_count);
    hashtable_mpmc->buckets_count_max = pow2_next(buckets_count_max);
    hashtable_mpmc->upsize_preferred_block_size = upsize_preferred_block_size;

    return hashtable_mpmc;
}

void hashtable_mpmc_free(
        hashtable_mpmc_t *hashtable_mpmc) {
    if (hashtable_mpmc->upsize.from != NULL) {
        hashtable_mpmc_data_free(hashtable_mpmc->upsize.from);
    }

    hashtable_mpmc_data_free(hashtable_mpmc->data);
    xalloc_free(hashtable_mpmc);
}

bool hashtable_mpmc_upsize_is_allowed(
        hashtable_mpmc_t *hashtable_mpmc) {
    return pow2_next(hashtable_mpmc->data->buckets_count) <= hashtable_mpmc->buckets_count_max;
}

void hashtable_mpmc_upsize_prepare(
        hashtable_mpmc_t *hashtable_mpmc) {
    hashtable_mpmc_upsize_status_t upsize_status;

    // Try to load the current status
    __atomic_load(
            &hashtable_mpmc->upsize.status,
            &upsize_status,
            __ATOMIC_ACQUIRE);

    // Check if another thread has already marked the hashtable in prepare for resizing
    if (upsize_status == HASHTABLE_MPMC_STATUS_PREPARE_FOR_UPSIZE) {
        return;
    }

    // Try to set the value to HASHTABLE_MPMC_PREPARE_FOR_RESIZING
    if (!__atomic_compare_exchange_n(
            &hashtable_mpmc->upsize.status,
            &upsize_status,
            HASHTABLE_MPMC_STATUS_PREPARE_FOR_UPSIZE,
            false,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        // If it failed another thread has already set the flag and has the ownership of the preparation
        return;
    }

    // Recalculate the size of the blocks to ensure that the last one will be always include all the buckets, although
    // for the last block it might be greater, so it has always to ensure it's not going to try to read data outside the
    // size of buckets (defined via buckets_count_real).
    uint32_t total_blocks =
            ceil((double)hashtable_mpmc->data->buckets_count_real / (double)hashtable_mpmc->upsize_preferred_block_size);
    uint32_t new_block_size = ceil((double)hashtable_mpmc->data->buckets_count_real / (double)total_blocks);

    // As buckets count uses the current one plus one as hashtable_mpmc_data_init will calculate the next power of 2
    // and use that as size.
    hashtable_mpmc_data_t *new_hashtable_mpmc_data = hashtable_mpmc_data_init(
            hashtable_mpmc->data->buckets_count + 1);

    // In 3 steps:
    // 1) sets the information for to upsize
    // 2) sets the new memory area for the hashtable
    // 3) updates the status
    //
    // The get and delete operation have to check if the status is different from NOT_UPSIZING, in case they will have
    // to check if upsize.from is set and if it's different from data. If it's the case they will have to check there as
    // well.
    hashtable_mpmc->upsize.total_blocks = total_blocks;
    hashtable_mpmc->upsize.remaining_blocks = total_blocks;
    hashtable_mpmc->upsize.block_size = new_block_size;
    hashtable_mpmc->upsize.from = hashtable_mpmc->data;
    MEMORY_FENCE_STORE();

    hashtable_mpmc->data = new_hashtable_mpmc_data;
    MEMORY_FENCE_STORE();

    hashtable_mpmc->upsize.status = HASHTABLE_MPMC_STATUS_UPSIZING;
    MEMORY_FENCE_STORE();
}

void hashtable_mpmc_upsize_copy_block(
        hashtable_mpmc_t *hashtable_mpmc) {
    int64_t new_remaining_blocks, current_remaining_blocks, block_number;
    uint16_t threads_count;
    hashtable_mpmc_bucket_index_t bucket_to_copy_index_start, bucket_to_copy_index_end, bucket_to_copy_index;
    hashtable_mpmc_bucket_index_t found_bucket_index;
    hashtable_mpmc_bucket_t bucket_to_copy, found_bucket, bucket_to_overwrite;

    new_remaining_blocks = __atomic_sub_fetch(&hashtable_mpmc->upsize.remaining_blocks, 1, __ATOMIC_ACQ_REL);

    // If there are no more blocks to copy returns
    if (new_remaining_blocks < 0) {
        return;
    }

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation = epoch_operation_queue_enqueue(
            thread_local_epoch_operation_queue_hashtable_key_value);
    assert(operation != NULL);

    // Increments the threads counter
    __atomic_add_fetch(&hashtable_mpmc->upsize.threads_count, 1, __ATOMIC_ACQ_REL);

    block_number = (hashtable_mpmc->upsize.total_blocks - new_remaining_blocks) - 1;

    bucket_to_copy_index_start = block_number * HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE;
    bucket_to_copy_index_end =
            MIN(hashtable_mpmc->upsize.from->buckets_count_real, bucket_to_copy_index_start + HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

    for(
            bucket_to_copy_index = bucket_to_copy_index_start;
            bucket_to_copy_index < bucket_to_copy_index_end;
            bucket_to_copy_index++) {
        __atomic_load(
                &hashtable_mpmc->upsize.from->buckets[bucket_to_copy_index]._packed,
                (uint128_t*)&bucket_to_copy._packed,
                __ATOMIC_ACQUIRE);

        char *key = bucket_to_copy.data.key_value->key_is_embedded
                    ? (char*)bucket_to_copy.data.key_value->key.embedded.key
                    : bucket_to_copy.data.key_value->key.external.key;
        uint16_t key_length = bucket_to_copy.data.key_value->key_is_embedded
                    ? bucket_to_copy.data.key_value->key.embedded.key_length
                    : bucket_to_copy.data.key_value->key.external.key_length;

        // Check if the key has already been inserted in the new hashtable, temporary values are allowed as it means
        // that the value is being inserted, therefore we can skip this copy as well
        hashtable_mpmc_result_t found_existing_result = hashtable_mpmc_support_find_bucket_and_key_value(
                hashtable_mpmc->data,
                bucket_to_copy.data.key_value->hash,
                bucket_to_copy.data.hash_half,
                key,
                key_length,
                true,
                &found_bucket,
                &found_bucket_index);

        if (found_existing_result == HASHTABLE_MPMC_RESULT_TRUE) {
            // the bucket is set to zero to avoid it being accessed if the new value is deleted
            hashtable_mpmc->upsize.from->buckets[bucket_to_copy_index]._packed = 0;
            MEMORY_FENCE_STORE();
            continue;
        }

        hashtable_mpmc_result_t found_empty_result = hashtable_mpmc_support_acquire_empty_bucket_for_insert(
                hashtable_mpmc->data,
                bucket_to_copy.data.key_value->hash,
                bucket_to_copy.data.hash_half,
                key,
                key_length,
                bucket_to_copy.data.key_value->value,
                (hashtable_mpmc_data_key_value_t**)&bucket_to_copy.data.key_value,
                &bucket_to_overwrite,
                &found_bucket_index);

        if (found_empty_result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING) {
            // This is very bad, the current algorithm can't handle nested resizes, for now just fail
            assert(false);
            FATAL(TAG, "Resizing during resizes aren't supported, shutting down");
        }

        hashtable_mpmc_result_t validate_insert_result = hashtable_mpmc_support_validate_insert(
                hashtable_mpmc->data,
                bucket_to_copy.data.key_value->hash,
                bucket_to_copy.data.hash_half,
                key,
                key_length,
                found_bucket_index);

        if (validate_insert_result == HASHTABLE_MPMC_RESULT_FALSE) {
            // Resets the previously initialized bucket, no need for atomic operations as the current thread is the only
            // one that by algorithm will ever change this bucket.
            hashtable_mpmc->data->buckets[found_bucket_index]._packed = bucket_to_overwrite._packed;
            MEMORY_FENCE_STORE();

            // the bucket is set to zero to avoid it being accessed if the new value is deleted
            hashtable_mpmc->upsize.from->buckets[bucket_to_copy_index]._packed = 0;
            MEMORY_FENCE_STORE();
            continue;
        }
    }

    // Decrements the threads counter and load the value of remaining blocks
    threads_count = __atomic_sub_fetch(&hashtable_mpmc->upsize.threads_count, 1, __ATOMIC_ACQ_REL);
    __atomic_load(&hashtable_mpmc->upsize.remaining_blocks, &current_remaining_blocks, __ATOMIC_ACQUIRE);

    epoch_operation_queue_mark_completed(operation);

    // If there are no more blocks to copy and no more threads trying to copy, the status of the upsize operation can be
    // set back to NOT_RESIZING
    if (current_remaining_blocks <= 0 && threads_count) {
        hashtable_mpmc->upsize.status = HASHTABLE_MPMC_STATUS_NOT_UPSIZING;
        MEMORY_FENCE_STORE();

        epoch_gc_stage_object(EPOCH_GC_OBJECT_TYPE_HASHTABLE_DATA, hashtable_mpmc->upsize.from);

        hashtable_mpmc->upsize.from = NULL;
        MEMORY_FENCE_STORE();
    }
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

hashtable_mpmc_result_t hashtable_mpmc_support_find_bucket_and_key_value(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash,
        hashtable_mpmc_hash_half_t hash_half,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        bool allow_temporary,
        hashtable_mpmc_bucket_t *return_bucket,
        hashtable_mpmc_bucket_index_t *return_bucket_index) {
    hashtable_mpmc_result_t found = HASHTABLE_MPMC_RESULT_FALSE;
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
        return_bucket->_packed = hashtable_mpmc_data->buckets[bucket_index]._packed;

        // If no tombstone is set, the loop can be interrupted, no values have ever been set past the bucket being
        // checked
        if (unlikely(return_bucket->_packed == 0)) {
            break;
        }

        // Although it might seem odd to have a condition that will cause a slow processing of a match, the reality is
        // that in the vast majority of cases (99.6% of cases) the hash will not match and the loop will have to
        // continue.
        // Even if this likely might slow down finding buckets that are in the slot where they are supposed to be, it
        // will speed up all the other ones
        if (likely(return_bucket->data.hash_half != hash_half)) {
            continue;
        }

        if (unlikely(!allow_temporary && HASHTABLE_MPMC_BUCKET_IS_TEMPORARY(*return_bucket))) {
            continue;
        }

        hashtable_mpmc_data_key_value_volatile_t *key_value = HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(*return_bucket);

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

        // Update the return bucket and mark the operation as successful
        *return_bucket_index = bucket_index;
        found = HASHTABLE_MPMC_RESULT_TRUE;
        break;
    }

    return found;
}

hashtable_mpmc_result_t hashtable_mpmc_support_acquire_empty_bucket_for_insert(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash,
        hashtable_mpmc_hash_half_t hash_half,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t value,
        hashtable_mpmc_data_key_value_t **new_key_value,
        hashtable_mpmc_bucket_t *overridden_bucket,
        hashtable_mpmc_bucket_index_t *new_bucket_index) {
    hashtable_mpmc_result_t found = HASHTABLE_MPMC_RESULT_NEEDS_RESIZING;
    hashtable_mpmc_bucket_t bucket, new_bucket;
    hashtable_mpmc_bucket_index_t bucket_index;

    hashtable_mpmc_bucket_index_t bucket_index_start = hashtable_mpmc_support_bucket_index_from_hash(
            hashtable_mpmc_data,
            hash);

    for(
            bucket_index = bucket_index_start;
            bucket_index < bucket_index_start + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
            bucket_index++) {
        __atomic_load(
                &hashtable_mpmc_data->buckets[bucket_index]._packed,
                (uint128_t*)&bucket._packed,
                __ATOMIC_ACQUIRE);

        if (bucket.data.hash_half != 0) {
            // Skip the non-empty value
            continue;
        }

        // This loop might get retried a number of times in case of clashes, therefore it allocates the
        // new_key_value only once and only if necessary. To speed up the hot path (e.g. no clashes) it's marked as
        // likely to be true.
        // For optimization reasons, this operation is done only if an empty bucket is found.
        if (likely(!*new_key_value)) {
            // If the bucket hasn't been found, a new one needs to be inserted and therefore the key_value struct
            // has to be allocated
            *new_key_value = xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
            (*new_key_value)->hash = hash;
            (*new_key_value)->value = value;

            // Check if the key can be embedded, uses the lesser than or equal as we don't care about the nul
            // because the key_length is what drives the string operations on the keys.
            if (key_length <= sizeof((*new_key_value)->key.embedded.key)) {
                // The key can be embedded
                strncpy((*new_key_value)->key.embedded.key, key, key_length);
                (*new_key_value)->key.embedded.key_length = key_length;
                (*new_key_value)->key_is_embedded = true;
            } else {
                // The key is too large and can't be embedded
                (*new_key_value)->key.external.key = key;
                (*new_key_value)->key.external.key_length = key_length;
                (*new_key_value)->key_is_embedded = false;
            }

            // Prepare the new bucket, the new_key_value pointer points to the key_value struct already prepared but
            // with the least significant bit set to 1 to indicate that it's a temporary allocation.
            new_bucket.data.transaction_id.id = 0;
            new_bucket.data.hash_half = hash_half;
            new_bucket.data.key_value = (void*)((uintptr_t)(*new_key_value) | HASHTABLE_MPMC_POINTER_TAG_TEMPORARY);
        }

        if (__atomic_compare_exchange_n(
                &hashtable_mpmc_data->buckets[bucket_index]._packed,
                (uint128_t*)&bucket._packed,
                new_bucket._packed,
                false,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            found = HASHTABLE_MPMC_RESULT_TRUE;
            overridden_bucket->_packed = bucket._packed;
            *new_bucket_index = bucket_index;
            break;
        }
    }

    return found;
}

hashtable_mpmc_result_t hashtable_mpmc_support_validate_insert(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash,
        hashtable_mpmc_hash_half_t hash_half,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        hashtable_mpmc_bucket_index_t new_bucket_index) {
    hashtable_mpmc_bucket_index_t bucket_index;

    hashtable_mpmc_bucket_index_t bucket_index_start = hashtable_mpmc_support_bucket_index_from_hash(
            hashtable_mpmc_data,
            hash);

    // Third pass iteration to ensure that no other thread is trying to insert the same exact key
    for(
            bucket_index = bucket_index_start;
            bucket_index < bucket_index_start + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
            bucket_index++) {
        if (bucket_index == new_bucket_index) {
            continue;
        }

        MEMORY_FENCE_LOAD();
        if (hashtable_mpmc_data->buckets[bucket_index].data.hash_half != hash_half) {
            continue;
        }

        hashtable_mpmc_data_key_value_volatile_t *key_value =
                HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(hashtable_mpmc_data->buckets[bucket_index]);

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

        return HASHTABLE_MPMC_RESULT_FALSE;
    }

    return HASHTABLE_MPMC_RESULT_TRUE;
}

hashtable_mpmc_result_t hashtable_mpmc_op_get(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t *return_value) {
    hashtable_mpmc_bucket_t bucket;
    hashtable_mpmc_bucket_index_t bucket_index;
    hashtable_mpmc_hash_t hash = hashtable_mcmp_support_hash_calculate(key, key_length);

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation = epoch_operation_queue_enqueue(
            thread_local_epoch_operation_queue_hashtable_key_value);
    assert(operation != NULL);

    hashtable_mpmc_result_t found_result = hashtable_mpmc_support_find_bucket_and_key_value(
            hashtable_mpmc->data,
            hash,
            hashtable_mpmc_support_hash_half(hash),
            key,
            key_length,
            false,
            &bucket,
            &bucket_index);

    if (unlikely(found_result == HASHTABLE_MPMC_RESULT_TRY_LATER)) {
        return HASHTABLE_MPMC_RESULT_TRY_LATER;
    } else if (found_result == HASHTABLE_MPMC_RESULT_TRUE) {
        // Fetch the value
        *return_value = bucket.data.key_value->value;
    }

    // Mark the operation as completed
    epoch_operation_queue_mark_completed(operation);

    return found_result;
}

hashtable_mpmc_result_t hashtable_mpmc_op_delete(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length) {
    hashtable_mpmc_bucket_t found_bucket;
    hashtable_mpmc_bucket_index_t found_bucket_index;
    hashtable_mpmc_hash_t hash = hashtable_mcmp_support_hash_calculate(key, key_length);

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation = epoch_operation_queue_enqueue(
            thread_local_epoch_operation_queue_hashtable_key_value);
    assert(operation != NULL);

    // Try to search for the key
    hashtable_mpmc_result_t found_result = hashtable_mpmc_support_find_bucket_and_key_value(
            hashtable_mpmc->data,
            hash,
            hashtable_mpmc_support_hash_half(hash),
            key,
            key_length,
            false,
            &found_bucket,
            &found_bucket_index);

    if (found_result != HASHTABLE_MPMC_RESULT_TRUE) {
        return found_result;
    }

    // When a bucket is deleted is marked with a tombstone to let get know that there is/was something past this point,
    // and it has to continue to search
    hashtable_mpmc_bucket_t deleted_bucket = { ._packed = 0 };
    deleted_bucket.data.key_value = (void*)HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE;

    // Try to empty the bucket, if the operation is successful, stage the key_value pointer to be garbage collected, if
    // it's not it means that something else already deleted the value or changed it so the operation can be ignored
    if (__atomic_compare_exchange_n(
            &hashtable_mpmc->data->buckets[found_bucket_index]._packed,
            (uint128_t*)&found_bucket._packed,
            deleted_bucket._packed,
            false,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        epoch_gc_stage_object(
                EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE,
                (void*)found_bucket.data.key_value);
    }

    // Mark the operation as completed
    epoch_operation_queue_mark_completed(operation);

    return found_result;
}

hashtable_mpmc_result_t hashtable_mpmc_op_set(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t value,
        bool *return_created_new,
        bool *return_value_updated,
        uintptr_t *return_previous_value) {
    bool retry_loop;
    hashtable_mpmc_result_t result;
    hashtable_mpmc_bucket_t found_bucket, bucket_to_overwrite;
    hashtable_mpmc_bucket_index_t found_bucket_index, new_bucket_index;
    hashtable_mpmc_data_key_value_t *new_key_value = NULL;
    hashtable_mpmc_hash_t hash = hashtable_mcmp_support_hash_calculate(key, key_length);
    hashtable_mpmc_hash_half_t hash_half = hashtable_mpmc_support_hash_half(hash);

    *return_created_new = false;
    *return_value_updated = false;
    *return_previous_value = 0;

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation = epoch_operation_queue_enqueue(
            thread_local_epoch_operation_queue_hashtable_key_value);
    assert(operation != NULL);

    // Uses a 3-phase approach:
    // - searches first for a bucket with a matching hash
    // - if it doesn't find it, it searches for an empty bucket and update it marking it as still being added
    // - searches again the entire range to see if some other thread did set the value, if it finds a match marked
    //   as being added will drop the bucket and ask the caller to try again the process from the first step otherwise
    //   will drop the bucket and drop the update operation as well as another thread finished the insert operation
    //   before the one being processed

    // Maximum number of allowed retries
    uint8_t retries = 3;
    do {
        retry_loop = false;

        // Try to find the value in the hashtable
        hashtable_mpmc_result_t found_existing_result = hashtable_mpmc_support_find_bucket_and_key_value(
                hashtable_mpmc->data,
                hash,
                hash_half,
                key,
                key_length,
                true,
                &found_bucket,
                &found_bucket_index);

        // If the bucket was previously found, try to swap it with the new bucket
        if (found_existing_result == HASHTABLE_MPMC_RESULT_TRUE) {
            // If the value found is temporary or if there is a transaction in progress it means that another thread is
            // writing the data so the flow can just wait for it to complete before carrying out the operations.
            bool is_temporary = ((uintptr_t)found_bucket.data.key_value & 0x01) == 0x01;
            if (unlikely(is_temporary || found_bucket.data.transaction_id.id != 0)) {
                retry_loop = true;
                continue;
            }

            uintptr_t expected_value = found_bucket.data.key_value->value;

            *return_value_updated = __atomic_compare_exchange_n(
                    &hashtable_mpmc->data->buckets[found_bucket_index].data.key_value->value,
                    &expected_value,
                    value,
                    false,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_ACQUIRE);

            if (*return_value_updated) {
                *return_previous_value = expected_value;
            }

            // As the key is owned by the hashtable, the pointer is freed as well
            xalloc_free(key);

            result = HASHTABLE_MPMC_RESULT_TRUE;
            goto end;
        }

        // If the status of the hashtable is HASHTABLE_MPMC_PREPARE_FOR_RESIZING retry the loop directly as there is
        // work in progress to upsize the hashtable
        MEMORY_FENCE_LOAD();
        if (hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_PREPARE_FOR_UPSIZE) {
            retry_loop = true;
            continue;
        }

        hashtable_mpmc_result_t found_empty_result = hashtable_mpmc_support_acquire_empty_bucket_for_insert(
                hashtable_mpmc->data,
                hash,
                hash_half,
                key,
                key_length,
                value,
                &new_key_value,
                &bucket_to_overwrite,
                &new_bucket_index);

        // If no empty bucket has been found the hashtable is full and needs resizing
        if (unlikely(found_empty_result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING)) {
            if (unlikely(!hashtable_mpmc_upsize_is_allowed(hashtable_mpmc))) {
                result = HASHTABLE_MPMC_RESULT_FALSE;
                break;
            }
            
            hashtable_mpmc_upsize_prepare(hashtable_mpmc);
            retry_loop = true;
            continue;
        }

        hashtable_mpmc_result_t validate_insert_result = hashtable_mpmc_support_validate_insert(
                hashtable_mpmc->data,
                hash,
                hash_half,
                key,
                key_length,
                new_bucket_index);

        if (validate_insert_result == HASHTABLE_MPMC_RESULT_FALSE) {
            // Resets the previously initialized bucket, no need for atomic operations as the current thread is the only
            // one that by algorithm will ever change this bucket.
            hashtable_mpmc->data->buckets[new_bucket_index]._packed = bucket_to_overwrite._packed;
            MEMORY_FENCE_STORE();

            bucket_to_overwrite._packed = 0;
            retry_loop = true;
            break;
        }
    } while(retry_loop && --retries > 0);

    // If after the allowed retries the retry loop flag is still true, asks the caller to try again later
    if (retry_loop) {
        result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        goto end;
    }

    // Drop the temporary flag from the key_value pointer
    hashtable_mpmc->data->buckets[new_bucket_index].data.key_value = new_key_value;

    // No need for an atomic operation, the value can be safely overwritten as by algorithm no other thread will touch
    // the bucket
    MEMORY_FENCE_STORE();

    // Operation successful, set the result to true and update the status variables
    *return_created_new = true;
    *return_value_updated = true;
    result = HASHTABLE_MPMC_RESULT_TRUE;

end:

    // Mark the operation as completed
    epoch_operation_queue_mark_completed(operation);

    // If a new_key_value has been allocated but at the end a new bucket wasn't created, it has to be staged in the GC
    // as another thread in the meantime (e.g. another thread trying to insert the same key) might be reading it.
    if (unlikely(*return_created_new == false && new_key_value != NULL)) {
        epoch_gc_stage_object(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE, new_key_value);
    }

    return result;
}
