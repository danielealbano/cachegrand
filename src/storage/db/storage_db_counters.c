/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "config.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_rwspinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "memory_allocator/ffma.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/storage.h"
#include "storage/db/storage_db.h"

#include "storage_db_counters.h"

#define TAG "storage_db_counters"

pthread_key_t storage_db_counters_index_key;
static pthread_once_t storage_db_counters_index_key_once = PTHREAD_ONCE_INIT;

static void storage_db_counters_index_key_destroy(void *value) {
    storage_db_counters_slots_bitmap_and_index_t *slot =
            (storage_db_counters_slots_bitmap_and_index_t*)value;

    if (!slot) {
        return;
    }

    slots_bitmap_mpmc_release(slot->slots_bitmap, slot->index);
    ffma_mem_free(slot);
}

void storage_db_counters_slot_key_init_once() {
    pthread_key_create(
            &storage_db_counters_index_key,
            storage_db_counters_index_key_destroy);

    pthread_setspecific(storage_db_counters_index_key, NULL);
}

void storage_db_counters_slot_key_ensure_init(
        storage_db_t *storage_db) {
    // ensure that storage_db_counters_index_key has been initialized once, if not set it up passing the
    // storage_db down the line
    pthread_once(
            &storage_db_counters_index_key_once,
            storage_db_counters_slot_key_init_once);

    // If the value is set to null, it means that the current thread has not been assigned a slot yet
    if (pthread_getspecific(storage_db_counters_index_key) == NULL) {
        storage_db_counters_slots_bitmap_and_index_t *slot =
                ffma_mem_alloc(sizeof(storage_db_counters_slots_bitmap_and_index_t));
        if (!slot) {
            FATAL(TAG, "Unable to allocate memory for storage db counters");
        }

        slot->slots_bitmap = storage_db->counters_slots_bitmap;
        slot->index = slots_bitmap_mpmc_get_next_available(slot->slots_bitmap);

        if (slot->index == UINT64_MAX) {
            FATAL(TAG, "No more slots available for worker counters");
        }

        storage_db->counters[slot->index].per_db = hashtable_spsc_new(
                100, HASHTABLE_SPSC_DEFAULT_MAX_RANGE, false);

        pthread_setspecific(storage_db_counters_index_key, slot);
    }
}

void storage_db_counters_sum_global(
        storage_db_t *storage_db,
        storage_db_counters_t *counters) {
    uint64_t workers_to_find = storage_db->workers_count;
    uint64_t found_slot_index;
    uint64_t next_slot_index = 0;

    counters->data_size = 0;
    while(workers_to_find > 0 && (found_slot_index = slots_bitmap_mpmc_iter(
            storage_db->counters_slots_bitmap, next_slot_index)) != UINT64_MAX) {
        counters->keys_count += storage_db->counters[found_slot_index].global.keys_count;
        counters->data_size += storage_db->counters[found_slot_index].global.data_size;
        counters->keys_changed += storage_db->counters[found_slot_index].global.keys_changed;
        counters->data_changed += storage_db->counters[found_slot_index].global.data_changed;
        next_slot_index = found_slot_index + 1;
        workers_to_find--;
    }
    assert(counters->keys_count >= 0);
    assert(counters->data_size >= 0);
}

void storage_db_counters_sum_per_db(
        storage_db_t *storage_db,
        storage_db_database_number_t database_number,
        storage_db_counters_t *counters) {
    uint64_t workers_to_find = storage_db->workers_count;
    uint64_t found_slot_index;
    uint64_t next_slot_index = 0;

    counters->data_size = 0;
    while(workers_to_find-- > 0 && (found_slot_index = slots_bitmap_mpmc_iter(
            storage_db->counters_slots_bitmap, next_slot_index)) != UINT64_MAX) {
        storage_db_counters_t *counters_per_db = (storage_db_counters_t*) hashtable_spsc_op_get_by_hash_and_key_uint32(
                storage_db->counters[found_slot_index].per_db,
                database_number,
                database_number);

        next_slot_index = found_slot_index + 1;

        if (unlikely(!counters_per_db)) {
            continue;
        }

        counters->keys_count += counters_per_db->keys_count;
        counters->data_size += counters_per_db->data_size;
        counters->keys_changed += counters_per_db->keys_changed;
        counters->data_changed += counters_per_db->data_changed;
    }
}