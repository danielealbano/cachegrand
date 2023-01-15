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
    assert(thread_local_epoch_operation_queue_hashtable_key_value == NULL);
    thread_local_epoch_operation_queue_hashtable_key_value = epoch_operation_queue_init();
}

void hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free() {
    assert(thread_local_epoch_operation_queue_hashtable_key_value != NULL);
    epoch_operation_queue_free(thread_local_epoch_operation_queue_hashtable_key_value);
    thread_local_epoch_operation_queue_hashtable_key_value = NULL;
}

uint64_t hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch() {
    assert(thread_local_epoch_operation_queue_hashtable_key_value != NULL);
    return epoch_operation_queue_get_latest_epoch(
            thread_local_epoch_operation_queue_hashtable_key_value);
}

void hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init() {
    assert(thread_local_epoch_operation_queue_hashtable_data == NULL);
    thread_local_epoch_operation_queue_hashtable_data = epoch_operation_queue_init();
}

void hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free() {
    assert(thread_local_epoch_operation_queue_hashtable_data != NULL);
    epoch_operation_queue_free(thread_local_epoch_operation_queue_hashtable_data);
    thread_local_epoch_operation_queue_hashtable_data = NULL;
}

uint64_t hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch() {
    assert(thread_local_epoch_operation_queue_hashtable_data != NULL);
    return epoch_operation_queue_get_latest_epoch(
            thread_local_epoch_operation_queue_hashtable_data);
}

hashtable_mpmc_hash_t hashtable_mpmc_support_hash_calculate(
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
        hashtable_mpmc_bucket_t bucket = { ._packed = hashtable_mpmc_data->buckets[bucket_index]._packed };

        if (bucket._packed == 0 || HASHTABLE_MPMC_BUCKET_IS_TOMBSTONE(bucket)) {
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
        uint64_t upsize_preferred_block_size) {
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
    return pow2_next(hashtable_mpmc->data->buckets_count + 1) <= hashtable_mpmc->buckets_count_max;
}

bool hashtable_mpmc_upsize_prepare(
        hashtable_mpmc_t *hashtable_mpmc) {
    hashtable_mpmc_upsize_status_t upsize_status;

    // Try to load the current status
    MEMORY_FENCE_LOAD();
    upsize_status = hashtable_mpmc->upsize.status;

    // Check if another thread has already marked the hashtable in prepare for resizing
    if (upsize_status != HASHTABLE_MPMC_STATUS_NOT_UPSIZING) {
        return false;
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
        return false;
    }

    // Recalculate the size of the blocks to ensure that the last one will be always include all the buckets, although
    // for the last block it might be greater, so it has always to ensure it's not going to try to read data outside the
    // size of buckets (defined via buckets_count_real).
    int64_t total_blocks =
            ceil((double)hashtable_mpmc->data->buckets_count_real / (double)hashtable_mpmc->upsize_preferred_block_size);
    int64_t new_block_size = ceil((double)hashtable_mpmc->data->buckets_count_real / (double)total_blocks);

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
    hashtable_mpmc->data = new_hashtable_mpmc_data;
    MEMORY_FENCE_STORE();

    hashtable_mpmc->upsize.status = HASHTABLE_MPMC_STATUS_UPSIZING;
    MEMORY_FENCE_STORE();

    return true;
}

bool hashtable_mpmc_upsize_migrate_bucket(
        hashtable_mpmc_data_t *from,
        hashtable_mpmc_data_t *to,
        hashtable_mpmc_bucket_index_t bucket_to_migrate_index) {
    hashtable_mpmc_bucket_t bucket_to_migrate, bucket_to_overwrite, new_bucket_value, deleted_bucket;
    hashtable_mpmc_bucket_index_t found_bucket_index;

    // The bucket has been processed, so it can be set to deleted (not zero as it would break all the get and delete
    // operations happening in the meantime)
    deleted_bucket._packed = 0;
    deleted_bucket.data.key_value = (void*)HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE;

    // Tries to acquire the bucket to be migrated, at the end bucket_to_copy will contain the value that was used
    // as base to mark the bucket as migrating and therefore can be used to carry out the required operations.
    // For every retry ensures that there is still something to migrate and that it's not temporary.

    do {
        MEMORY_FENCE_LOAD();
        bucket_to_migrate._packed = from->buckets[bucket_to_migrate_index]._packed;

        // If the bucket is empty, or it's a tombstone it can be skipped
        if (bucket_to_migrate._packed == 0 || HASHTABLE_MPMC_BUCKET_IS_TOMBSTONE(bucket_to_migrate)) {
            return false;
        }

        // If the bucket is temporary, wait until it's finalized or set back to be empty
        if (HASHTABLE_MPMC_BUCKET_IS_TEMPORARY(bucket_to_migrate)) {
            continue;
        }

        // Set up the new bucket value setting the migrating pointer tag on key_value
        new_bucket_value._packed = bucket_to_migrate._packed;
        new_bucket_value.data.key_value = (hashtable_mpmc_data_key_value_volatile_t*)(
                (uintptr_t)new_bucket_value.data.key_value | HASHTABLE_MPMC_POINTER_TAG_MIGRATING);

        if (__atomic_compare_exchange_n(
                &from->buckets[bucket_to_migrate_index]._packed,
                (uint128_t*)&bucket_to_migrate._packed,
                new_bucket_value._packed,
                false,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            break;
        }
    } while(true);

    hashtable_mpmc_data_key_value_volatile_t *key_value = HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(bucket_to_migrate);
    char *key = key_value->key_is_embedded
            ? (char*)key_value->key.embedded.key
            : key_value->key.external.key;
    uint16_t key_length = key_value->key_is_embedded
            ? key_value->key.embedded.key_length
            : key_value->key.external.key_length;

    // TODO: review this loop, shouldn't be necessary
    hashtable_mpmc_result_t found_empty_result;
    while ((found_empty_result = hashtable_mpmc_support_acquire_empty_bucket_for_insert(
            to,
            key_value->hash,
            bucket_to_migrate.data.hash_half,
            key,
            key_length,
            key_value->value,
            (hashtable_mpmc_data_key_value_t**)&key_value,
            &bucket_to_overwrite,
            &found_bucket_index)) == HASHTABLE_MPMC_RESULT_TRY_LATER) {
        // If the key is found in the destination retry
    };

    if (found_empty_result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING) {
        fprintf(stdout, "> can't insert key during upsize\n"); fflush(stdout);
        FATAL(TAG, "Resizing during a resize isn't supported, shutting down");
    }

    // No need to check for duplicated inserts during the upsize, the insertion or updates is blocked
    to->buckets[found_bucket_index].data.key_value = key_value;
    MEMORY_FENCE_STORE();

    // Once the temporary flag has been dropped, mark the previous value as deleted (this will allow the operation to
    // skip the search on the upsize from hashtable and search in the current one directly)
    from->buckets[bucket_to_migrate_index]._packed = deleted_bucket._packed;
    MEMORY_FENCE_STORE();

    return true;
}

uint32_t hashtable_mpmc_upsize_migrate_block(
        hashtable_mpmc_t *hashtable_mpmc) {
    int64_t new_remaining_blocks, current_remaining_blocks, block_number;
    uint16_t threads_count;
    hashtable_mpmc_bucket_index_t bucket_to_migrate_index_start, bucket_to_migrate_index_end, bucket_to_migrate_index;

    // If there are no more blocks to copy returns, doesn't use memory fences and rely solely on the value already in
    // cache if present because most likely has already been updated because of the atomic operations. Worst case
    // scenario, this check will allow the continuation of the execution and the one afterwards will stop it anyway.
    if (hashtable_mpmc->upsize.remaining_blocks < 0) {
        return 0;
    }

    new_remaining_blocks = __atomic_sub_fetch(&hashtable_mpmc->upsize.remaining_blocks, 1, __ATOMIC_ACQ_REL);

    // If there are no more blocks to copy returns
    if (new_remaining_blocks < 0) {
        return 0;
    }

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation = epoch_operation_queue_enqueue(
            thread_local_epoch_operation_queue_hashtable_key_value);
    assert(operation != NULL);

    // Increments the threads counter
    __atomic_add_fetch(&hashtable_mpmc->upsize.threads_count, 1, __ATOMIC_ACQ_REL);

    block_number = (hashtable_mpmc->upsize.total_blocks - new_remaining_blocks) - 1;

    bucket_to_migrate_index_start = block_number * hashtable_mpmc->upsize.block_size;
    bucket_to_migrate_index_end = MIN(
            hashtable_mpmc->upsize.from->buckets_count_real,
            bucket_to_migrate_index_start + hashtable_mpmc->upsize.block_size);

    uint32_t migrated_buckets_count = 0;
    for(
            bucket_to_migrate_index = bucket_to_migrate_index_start;
            bucket_to_migrate_index < bucket_to_migrate_index_end;
            bucket_to_migrate_index++) {
        migrated_buckets_count += hashtable_mpmc_upsize_migrate_bucket(
                hashtable_mpmc->upsize.from,
                hashtable_mpmc->data,
                bucket_to_migrate_index) ? 1 : 0;
    }

    // Decrements the threads counter and load the value of remaining blocks
    threads_count = __atomic_sub_fetch(&hashtable_mpmc->upsize.threads_count, 1, __ATOMIC_ACQ_REL);
    __atomic_load(&hashtable_mpmc->upsize.remaining_blocks, &current_remaining_blocks, __ATOMIC_ACQUIRE);

    epoch_operation_queue_mark_completed(operation);

    // If there are no more blocks to copy and no more threads trying to copy, the status of the upsize operation can be
    // set back to NOT_RESIZING
    if (current_remaining_blocks <= 0 && threads_count == 0) {
        hashtable_mpmc->upsize.status = HASHTABLE_MPMC_STATUS_NOT_UPSIZING;
        MEMORY_FENCE_STORE();

        epoch_gc_stage_object(EPOCH_GC_OBJECT_TYPE_HASHTABLE_DATA, hashtable_mpmc->upsize.from);

        hashtable_mpmc->upsize.from = NULL;
        MEMORY_FENCE_STORE();
    }

    return migrated_buckets_count;
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

        if (unlikely(return_bucket->_packed == 0)) {
            break;
        }

        if (likely(return_bucket->data.hash_half != hash_half || HASHTABLE_MPMC_BUCKET_IS_TOMBSTONE(*return_bucket))) {
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

        // If the hash matches, it is extremely likely the key will match as well, check also if the bucket is marked as
        // temporary, if yes the allow_temporary flag has to be set to true otherwise the key is skipped.
        if (unlikely(
                !does_key_match
                || (HASHTABLE_MPMC_BUCKET_IS_TEMPORARY(*return_bucket) && !allow_temporary))) {
            continue;
        }

        // Most of the time reads will be done on buckets not being migrated
        if (unlikely(HASHTABLE_MPMC_BUCKET_IS_MIGRATING(*return_bucket))) {
            // If a bucket is being migrated no operation can be carried out on it
            found = HASHTABLE_MPMC_RESULT_TRY_LATER;
        } else {
            // Update the return bucket and mark the operation as successful
            *return_bucket_index = bucket_index;
            found = HASHTABLE_MPMC_RESULT_TRUE;
        }

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
    hashtable_mpmc_bucket_t bucket, new_bucket = { ._packed = 0 };
    hashtable_mpmc_bucket_index_t bucket_index;

    hashtable_mpmc_bucket_index_t bucket_index_start = hashtable_mpmc_support_bucket_index_from_hash(
            hashtable_mpmc_data,
            hash);

    for(
            bucket_index = bucket_index_start;
            bucket_index < bucket_index_start + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
            bucket_index++) {
        MEMORY_FENCE_LOAD();
        bucket._packed = hashtable_mpmc_data->buckets[bucket_index]._packed;

        // In some cases, all the buckets might be occupied by a mix of temporary values and actually inserted
        // values, if it's the case and there is no space for an insertion the caller should retry later as the
        // temporarily filled buckets might get freed.
        // It might also be the case that an op set is trying to insert into a hashtable being migrated and finds
        // a bucket marked as in migrating status, in this case it can stop the execution right away to retry later.
        if (unlikely(HASHTABLE_MPMC_BUCKET_IS_TEMPORARY(bucket))) {
            found = HASHTABLE_MPMC_RESULT_TRY_LATER;
            continue;
        } else if (unlikely(HASHTABLE_MPMC_BUCKET_IS_MIGRATING(bucket))) {
            return HASHTABLE_MPMC_RESULT_TRY_LATER;
        } else if (bucket._packed != 0 && !HASHTABLE_MPMC_BUCKET_IS_TOMBSTONE(bucket)) {
            continue;
        }

        // This loop might get retried a number of times in case of clashes, therefore it allocates the
        // new_key_value only once and only if necessary. To speed up the hot path (e.g. clashes once there is a
        // minimum of content) it's marked as unlikely to be true.
        // For optimization reasons, this operation is done only if an empty bucket is found.
        if (unlikely(!*new_key_value)) {
            // If the bucket hasn't been found, a new one needs to be inserted and therefore the key_value struct
            // has to be allocated
            *new_key_value = xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
            (*new_key_value)->hash = hash;
            (*new_key_value)->value = value;
            (*new_key_value)->create_time = (*new_key_value)->update_time = intrinsics_tsc();

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
        }

        // new_key_value can be passed externally already pre initialized, therefore the new bucket needs to be
        // initialized separately.
        if (unlikely(new_bucket._packed == 0)) {
            // Prepare the new bucket marking it as temporary
            new_bucket.data.transaction_id.id = 0;
            new_bucket.data.hash_half = hash_half;
            new_bucket.data.key_value = (void *)((uintptr_t)(*new_key_value) | HASHTABLE_MPMC_POINTER_TAG_TEMPORARY);
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

    assert(found != HASHTABLE_MPMC_RESULT_FALSE);

    return found;
}

hashtable_mpmc_result_t hashtable_mpmc_support_validate_insert(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash,
        hashtable_mpmc_hash_half_t hash_half,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        hashtable_mpmc_bucket_index_t new_bucket_index) {
    hashtable_mpmc_bucket_index_t bucket_index, bucket_index_start;
    hashtable_mpmc_bucket_t bucket;

    bucket_index_start = hashtable_mpmc_support_bucket_index_from_hash(
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
        bucket._packed = hashtable_mpmc_data->buckets[bucket_index]._packed;

        if (bucket._packed == 0) {
            break;
        }

        if (likely(bucket.data.hash_half != hash_half || HASHTABLE_MPMC_BUCKET_IS_TOMBSTONE(bucket))) {
            continue;
        }

        hashtable_mpmc_data_key_value_volatile_t *key_value = HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(bucket);

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
    hashtable_mpmc_result_t return_result, upsize_from_ht_return_result;
    hashtable_mpmc_bucket_t bucket;
    hashtable_mpmc_bucket_index_t bucket_index;
    hashtable_mpmc_data_t *hashtable_mpmc_data_upsize, *hashtable_mpmc_data_current;

    MEMORY_FENCE_LOAD();
    if (unlikely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_PREPARE_FOR_UPSIZE)) {
        return HASHTABLE_MPMC_RESULT_TRY_LATER;
    }

    epoch_operation_queue_operation_t *operation_kv, *operation_ht_data = NULL;
    hashtable_mpmc_hash_t hash = hashtable_mpmc_support_hash_calculate(key, key_length);

    // Start to track the operation to avoid trying to access freed memory
    operation_kv = epoch_operation_queue_enqueue(
            thread_local_epoch_operation_queue_hashtable_key_value);
    assert(operation_kv != NULL);

    MEMORY_FENCE_LOAD();
    hashtable_mpmc_data_current = hashtable_mpmc->data;

    // If there is a resize in progress, first check if the key is being migrated, if yes the caller has to retry the
    // operation
    if (unlikely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING)) {
        operation_ht_data = epoch_operation_queue_enqueue(
                thread_local_epoch_operation_queue_hashtable_data);
        assert(operation_ht_data != NULL);

        // The value might have changed or freed in between, it's necessary to check again
        MEMORY_FENCE_LOAD();
        hashtable_mpmc_data_upsize = hashtable_mpmc->upsize.from;
        if (likely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING)) {
            upsize_from_ht_return_result = hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable_mpmc_data_upsize,
                    hash,
                    hashtable_mpmc_support_hash_half(hash),
                    key,
                    key_length,
                    false,
                    &bucket,
                    &bucket_index);

            // If it has to retry later, set the return value and jump to the end
            if (unlikely(upsize_from_ht_return_result == HASHTABLE_MPMC_RESULT_TRY_LATER)) {
                return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
                goto end;
            }

            // If the value is found and not being migrated, acquire it
            if (upsize_from_ht_return_result == HASHTABLE_MPMC_RESULT_TRUE) {
                hashtable_mpmc_data_key_value_volatile_t *key_value = HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(bucket);
                *return_value = key_value->value;
                return_result = HASHTABLE_MPMC_RESULT_TRUE;
                goto end;
            }
        }
    }

    return_result = hashtable_mpmc_support_find_bucket_and_key_value(
            hashtable_mpmc_data_current,
            hash,
            hashtable_mpmc_support_hash_half(hash),
            key,
            key_length,
            false,
            &bucket,
            &bucket_index);

    if (return_result == HASHTABLE_MPMC_RESULT_TRUE) {
        hashtable_mpmc_data_key_value_volatile_t *key_value =
                HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(bucket);
        *return_value = key_value->value;
    } else if (return_result == HASHTABLE_MPMC_RESULT_FALSE) {
        MEMORY_FENCE_LOAD();
        if (hashtable_mpmc_data_current != hashtable_mpmc->data) {
            return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        }
    }

end:

    // Mark the operation as completed
    epoch_operation_queue_mark_completed(operation_kv);

    if (unlikely(operation_ht_data)) {
        epoch_operation_queue_mark_completed(operation_ht_data);
    }

    return return_result;
}

hashtable_mpmc_result_t hashtable_mpmc_op_delete(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length) {
    hashtable_mpmc_result_t return_result, upsize_from_ht_return_result;
    hashtable_mpmc_bucket_t found_bucket, deleted_bucket;
    hashtable_mpmc_bucket_index_t found_bucket_index;
    hashtable_mpmc_data_t *hashtable_mpmc_data_upsize, *hashtable_mpmc_data_current;
    epoch_operation_queue_operation_t *operation_kv, *operation_ht_data = NULL;
    hashtable_mpmc_hash_t hash = hashtable_mpmc_support_hash_calculate(key, key_length);

    MEMORY_FENCE_LOAD();
    if (unlikely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_PREPARE_FOR_UPSIZE)) {
        return HASHTABLE_MPMC_RESULT_TRY_LATER;
    }

    // When a bucket is deleted is marked with a tombstone to let get know that there is/was something past this point,
    // and it has to continue to search
    deleted_bucket._packed = 0;
    deleted_bucket.data.key_value = (void*)HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE;

    // Start to track the operation to avoid trying to access freed memory
    operation_kv = epoch_operation_queue_enqueue(
            thread_local_epoch_operation_queue_hashtable_key_value);
    assert(operation_kv != NULL);

    // If there is a resize in progress, first check if the key is being migrated, if yes the caller has to retry the
    // operation
    MEMORY_FENCE_LOAD();
    hashtable_mpmc_data_current = hashtable_mpmc->data;
    if (unlikely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING)) {
        operation_ht_data = epoch_operation_queue_enqueue(
                thread_local_epoch_operation_queue_hashtable_data);
        assert(operation_ht_data != NULL);

        // Get the upsize hashtable and ensure that it's still in the UPSIZING status
        MEMORY_FENCE_LOAD();
        hashtable_mpmc_data_upsize = hashtable_mpmc->upsize.from;
        if (likely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING)) {
            upsize_from_ht_return_result = hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable_mpmc_data_upsize,
                    hash,
                    hashtable_mpmc_support_hash_half(hash),
                    key,
                    key_length,
                    false,
                    &found_bucket,
                    &found_bucket_index);

            // If it has to retry later, set the return value and jump to the end
            if (unlikely(upsize_from_ht_return_result == HASHTABLE_MPMC_RESULT_TRY_LATER)) {
                return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
                goto end;
            }

            if (upsize_from_ht_return_result == HASHTABLE_MPMC_RESULT_TRUE) {
                bool swap_result = __atomic_compare_exchange_n(
                        &hashtable_mpmc_data_upsize->buckets[found_bucket_index]._packed,
                        (uint128_t *) &found_bucket._packed,
                        deleted_bucket._packed,
                        false,
                        __ATOMIC_ACQ_REL,
                        __ATOMIC_ACQUIRE);

                if (likely(swap_result)) {
                    epoch_gc_stage_object(
                            EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE,
                            (void *) HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(found_bucket));

                    return_result = HASHTABLE_MPMC_RESULT_TRUE;
                } else {
                    // If replacing the value with the tombstone fails, it might have been already deleted, might have
                    // been marked for migration or might have been migrated, in all the cases but first it has to retry
                    // later and as there isn't a safe and sane way to identify if it's a delete for a migration, or
                    // it's another thread deleting better to always return a try later.
                    return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
                }

                goto end;
            }
        }
    }

    // Try to search for the key
    hashtable_mpmc_result_t found_result = hashtable_mpmc_support_find_bucket_and_key_value(
            hashtable_mpmc_data_current,
            hash,
            hashtable_mpmc_support_hash_half(hash),
            key,
            key_length,
            false,
            &found_bucket,
            &found_bucket_index);

    if (found_result == HASHTABLE_MPMC_RESULT_TRY_LATER) {
        return_result = found_result;
        goto end;
    }

    if (found_result == HASHTABLE_MPMC_RESULT_FALSE) {
        MEMORY_FENCE_LOAD();
        if (hashtable_mpmc_data_current != hashtable_mpmc->data) {
            return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        } else {
            return_result = HASHTABLE_MPMC_RESULT_FALSE;
        }

        goto end;
    }

    // Try to empty the bucket, if the operation is successful, stage the key_value pointer to be garbage collected, if
    // it's not it means that something else already deleted the value or changed it so the operation can be ignored
    if (likely(__atomic_compare_exchange_n(
            &hashtable_mpmc_data_current->buckets[found_bucket_index]._packed,
            (uint128_t*)&found_bucket._packed,
            deleted_bucket._packed,
            false,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE))) {
        epoch_gc_stage_object(
                EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE,
                (void*)HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(found_bucket));
        return_result = HASHTABLE_MPMC_RESULT_TRUE;
    } else {
        // If replacing the value with the tombstone fails, it might have been already deleted, might have been marked
        // for migration or might have been migrated, in all the cases but first it has to retry later and as there
        // isn't a safe and sane way to identify if it's a delete for a migration, or it's another thread deleting
        // better to always return a try later.
        return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
    }

end:

    // Mark the operation as completed
    epoch_operation_queue_mark_completed(operation_kv);

    if (unlikely(operation_ht_data)) {
        epoch_operation_queue_mark_completed(operation_ht_data);
    }

    return return_result;
}

hashtable_mpmc_result_t hashtable_mpmc_op_set_update_value_if_key_exists(
        hashtable_mpmc_data_t *hashtable_mpmc_data,
        hashtable_mpmc_hash_t hash,
        hashtable_mpmc_hash_t hash_half,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t value,
        uintptr_t *return_previous_value) {
    hashtable_mpmc_bucket_t found_bucket = { ._packed = 0 };
    hashtable_mpmc_bucket_index_t found_bucket_index = 0;
    hashtable_mpmc_result_t return_result;

    return_result = hashtable_mpmc_support_find_bucket_and_key_value(
            hashtable_mpmc_data,
            hash,
            hash_half,
            key,
            key_length,
            true,
            &found_bucket,
            &found_bucket_index);

    if (return_result != HASHTABLE_MPMC_RESULT_TRUE) {
        // If no bucket is found or if it's necessary to retry later jump to tne end
        goto end;
    } else if (unlikely(HASHTABLE_MPMC_BUCKET_IS_TEMPORARY(found_bucket) || found_bucket.data.transaction_id.id != 0)) {
        // If the value found is temporary or if there is a transaction in progress it means that another thread is
        // writing the data so the flow can just wait for it to complete before carrying out the operations.
        return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        goto end;
    }

    // Acquire the current value
    hashtable_mpmc_data_key_value_volatile_t *key_value = HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(found_bucket);
    uintptr_t expected_value = key_value->value;

    // Try to set the new value
    bool return_value_updated = __atomic_compare_exchange_n(
            &key_value->value,
            &expected_value,
            value,
            true,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE);

    if (!return_value_updated) {
        return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
    } else {
        return_result = HASHTABLE_MPMC_RESULT_TRUE;
        *return_previous_value = expected_value;
        key_value->update_time = intrinsics_tsc();

        // As the key is owned by the hashtable and this copy is not in use, the key is freed as well
        xalloc_free(key);
    }

end:

    return return_result;
}

hashtable_mpmc_result_t hashtable_mpmc_op_set(
        hashtable_mpmc_t *hashtable_mpmc,
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length,
        uintptr_t value,
        bool *return_created_new,
        bool *return_value_updated,
        uintptr_t *return_previous_value) {
    hashtable_mpmc_result_t return_result;
    hashtable_mpmc_bucket_t found_bucket, bucket_to_overwrite;
    hashtable_mpmc_bucket_index_t found_bucket_index, new_bucket_index;
    hashtable_mpmc_data_t *hashtable_mpmc_data_upsize, *hashtable_mpmc_data_current;
    epoch_operation_queue_operation_t *operation_ht_data = NULL;
    hashtable_mpmc_data_key_value_t *new_key_value = NULL;
    hashtable_mpmc_hash_t hash = hashtable_mpmc_support_hash_calculate(key, key_length);
    hashtable_mpmc_hash_half_t hash_half = hashtable_mpmc_support_hash_half(hash);

    *return_created_new = false;
    *return_value_updated = false;
    *return_previous_value = 0;

    MEMORY_FENCE_LOAD();
    if (unlikely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_PREPARE_FOR_UPSIZE)) {
        return HASHTABLE_MPMC_RESULT_TRY_LATER;
    }

    // Start to track the operation to avoid trying to access freed memory
    epoch_operation_queue_operation_t *operation_kv = epoch_operation_queue_enqueue(
            thread_local_epoch_operation_queue_hashtable_key_value);
    assert(operation_kv != NULL);

    MEMORY_FENCE_LOAD();
    hashtable_mpmc_data_current = hashtable_mpmc->data;

    // Uses a 3-phase approach:
    // - searches first for a bucket with a matching hash
    // - if it doesn't find it, it searches for an empty bucket and update it marking it as still being added
    // - searches again the entire range to see if some other thread did set the value, if it finds a match marked
    //   as being added will drop the bucket and ask the caller to try again the process from the first step otherwise
    //   will drop the bucket and drop the update operation as well as another thread finished the insert operation
    //   before the one being processed

    // If there is a resize in progress, first check if the key is being migrated, if yes the caller has to retry
    // the operation
    if (unlikely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING)) {
        operation_ht_data = epoch_operation_queue_enqueue(
                thread_local_epoch_operation_queue_hashtable_data);
        assert(operation_ht_data != NULL);

        // Load the previous hashtable
        MEMORY_FENCE_LOAD();
        hashtable_mpmc_data_upsize = hashtable_mpmc->upsize.from;
        if (likely(hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING)) {
            // TODO: should allow to return of the buckets being migrated as we care only about updating the value in
            //       the key value structure and will not touch the bucket itself
            hashtable_mpmc_result_t found_existing_result_in_upsize_ht =
                    hashtable_mpmc_support_find_bucket_and_key_value(
                        hashtable_mpmc_data_upsize,
                        hash,
                        hashtable_mpmc_support_hash_half(hash),
                        key,
                        key_length,
                        false,
                        &found_bucket,
                        &found_bucket_index);

            if (unlikely(found_existing_result_in_upsize_ht == HASHTABLE_MPMC_RESULT_TRY_LATER)) {
                return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
                goto end;
            }

            // If the bucket was previously found, try to swap it with the new bucket
            if (found_existing_result_in_upsize_ht == HASHTABLE_MPMC_RESULT_TRUE) {
                // Acquire the current value
                hashtable_mpmc_data_key_value_volatile_t *key_value =
                        HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(found_bucket);
                uintptr_t expected_value = key_value->value;

                // Try to set the new value
                *return_value_updated = __atomic_compare_exchange_n(
                        &key_value->value,
                        &expected_value,
                        value,
                        false,
                        __ATOMIC_ACQ_REL,
                        __ATOMIC_ACQUIRE);

                if (!*return_value_updated) {
                    return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
                } else {
                    return_result = HASHTABLE_MPMC_RESULT_TRUE;
                    *return_previous_value = expected_value;
                    key_value->update_time = intrinsics_tsc();

                    // As the key is owned by the hashtable and this copy is not in use, the key is freed as well
                    xalloc_free(key);
                }

                goto end;
            }
        }
    }

    // Try to find the value in the hashtable
    hashtable_mpmc_result_t found_existing_result = hashtable_mpmc_support_find_bucket_and_key_value(
            hashtable_mpmc_data_current,
            hash,
            hash_half,
            key,
            key_length,
            true,
            &found_bucket,
            &found_bucket_index);

    if (unlikely(found_existing_result == HASHTABLE_MPMC_RESULT_TRY_LATER)) {
        return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        goto end;
    } else if (unlikely(HASHTABLE_MPMC_BUCKET_IS_TEMPORARY(found_bucket) || found_bucket.data.transaction_id.id != 0)) {
        // If the value found is temporary or if there is a transaction in progress it means that another thread is
        // writing the data so the flow can just wait for it to complete before carrying out the operations.
        return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        goto end;
    }

    // If the bucket was previously found, try to swap it with the new bucket
    if (found_existing_result == HASHTABLE_MPMC_RESULT_TRUE) {
        // Acquire the current value
        hashtable_mpmc_data_key_value_volatile_t *key_value = HASHTABLE_MPMC_BUCKET_GET_KEY_VALUE_PTR(found_bucket);
        uintptr_t expected_value = key_value->value;

        // Try to set the new value
        *return_value_updated = __atomic_compare_exchange_n(
                &key_value->value,
                &expected_value,
                value,
                false,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE);

        // If the swap fails it can be because of another set so the previous value is popped up only if the
        // operation is successful
        if (likely(!*return_value_updated)) {
            return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        } else {
            return_result = HASHTABLE_MPMC_RESULT_TRUE;
            *return_previous_value = expected_value;
            key_value->update_time = intrinsics_tsc();

            // As the key is owned by the hashtable, the pointer is freed as well
            xalloc_free(key);
        }

        goto end;
    }

    // If the status of the hashtable is HASHTABLE_MPMC_PREPARE_FOR_RESIZING retry the loop directly as there is
    // work in progress to upsize the hashtable
    MEMORY_FENCE_LOAD();
    if (hashtable_mpmc->upsize.status == HASHTABLE_MPMC_STATUS_PREPARE_FOR_UPSIZE) {
        return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        goto end;
    }

    hashtable_mpmc_result_t found_empty_result = hashtable_mpmc_support_acquire_empty_bucket_for_insert(
            hashtable_mpmc_data_current,
            hash,
            hash_half,
            key,
            key_length,
            value,
            &new_key_value,
            &bucket_to_overwrite,
            &new_bucket_index);

    // If no empty bucket has been found the hashtable is full and needs resizing
    if (unlikely(
            found_empty_result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING ||
            found_empty_result == HASHTABLE_MPMC_RESULT_TRY_LATER)) {
        return_result = found_empty_result;
        goto end;
    }

    hashtable_mpmc_result_t validate_insert_result = hashtable_mpmc_support_validate_insert(
            hashtable_mpmc_data_current,
            hash,
            hash_half,
            key,
            key_length,
            new_bucket_index);

    // If the validation failed or the hashtable started a resize in the meantime, the process will try to carry
    // out an insert again
    MEMORY_FENCE_LOAD();
    if (validate_insert_result == HASHTABLE_MPMC_RESULT_FALSE || hashtable_mpmc_data_current != hashtable_mpmc->data) {
        // The bucket has to be set to a tombstone, can't be set to the previous value, if the previous value would be
        // _packed = 0 and because it was marked as temporary another thread inserted a key afterwards, that key would
        // become unreachable.
        // When reverting a temporary insert it has always to be set to a tombstone.
        bucket_to_overwrite._packed = 0;
        bucket_to_overwrite.data.key_value = (void*)HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE;
        hashtable_mpmc_data_current->buckets[new_bucket_index]._packed = bucket_to_overwrite._packed;
        MEMORY_FENCE_STORE();

        return_result = HASHTABLE_MPMC_RESULT_TRY_LATER;
        goto end;
    }

    // Drop the temporary flag from the key_value pointer
    hashtable_mpmc_data_current->buckets[new_bucket_index].data.key_value = new_key_value;

    // No need for an atomic operation, the value can be safely overwritten as by algorithm no other thread will touch
    // the bucket
    MEMORY_FENCE_STORE();

    // If the key is embedded free the one passed
    if (new_key_value->key_is_embedded) {
        xalloc_free(key);
    }

    // Operation successful, set the result to true and update the status variables
    *return_created_new = true;
    *return_value_updated = true;
    return_result = HASHTABLE_MPMC_RESULT_TRUE;

end:

    // Mark the operations as completed
    epoch_operation_queue_mark_completed(operation_kv);

    if (operation_ht_data != NULL) {
        epoch_operation_queue_mark_completed(operation_ht_data);
    }

    // If a new_key_value has been allocated but at the end a new bucket wasn't created, it has to be staged in the GC
    // as another thread in the meantime (e.g. another thread trying to insert the same key) might be reading it.
    if (unlikely(*return_created_new == false && new_key_value != NULL)) {
        epoch_gc_stage_object(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE, new_key_value);
    }

    return return_result;
}
