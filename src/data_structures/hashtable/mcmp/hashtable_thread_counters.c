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
#include <stdatomic.h>
#include <numa.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "xalloc.h"
#include "hashtable.h"

#include "hashtable_thread_counters.h"

thread_local bool hashtable_mcmp_thread_counter_index_fetched = false;
thread_local uint32_t hashtable_mcmp_thread_counter_index;

hashtable_counters_t *hashtable_mcmp_thread_counters_sum_fetch(
        hashtable_t *hashtable) {
    hashtable_counters_t *counters_sum = xalloc_alloc_zero(sizeof(hashtable_counters_t));

    for(uint32_t index = 0; index < hashtable->ht_current->thread_counters.size; index++) {
        hashtable_counters_volatile_t *thread_counter = hashtable_mcmp_thread_counters_get_by_index(
                hashtable->ht_current, index);
        counters_sum->size += thread_counter->size;
    }

    assert(counters_sum->size >= 0);

    return counters_sum;
}

void hashtable_mcmp_thread_counters_sum_free(
        hashtable_counters_t *counters_sum) {
    xalloc_free(counters_sum);
}

void hashtable_mcmp_thread_counters_reset(
        hashtable_data_volatile_t *hashtable_data) {
    spinlock_lock(&hashtable_data->thread_counters.lock, true);

    if (hashtable_data->thread_counters.size == 0) {
        // If the current list has size 0 it doesn't contain any counter so there is nothing to reset
        spinlock_unlock(&hashtable_data->thread_counters.lock);
        return;
    }

    hashtable_counters_volatile_t **old_counters = hashtable_data->thread_counters.list;
    hashtable_data->thread_counters.size = 0;
    hashtable_data->thread_counters.list = NULL;

    spinlock_unlock(&hashtable_data->thread_counters.lock);

    if (old_counters) {
        // At this point a memory fence has been issued so the old counters can be flushed away
        xalloc_free(old_counters);
    }
}

void hashtable_mcmp_thread_counters_expand_to(
        hashtable_data_volatile_t *hashtable_data,
        uint32_t new_size) {
    assert(hashtable_data->thread_counters.size < UINT32_MAX);

    spinlock_lock(&hashtable_data->thread_counters.lock, true);

    // Ensure that the resize is actually needed under lock
    if (new_size <= hashtable_data->thread_counters.size) {
        spinlock_unlock(&hashtable_data->thread_counters.lock);
        return;
    }

    // Below is basically a realloc, but the new value has to be assigned before the old one gets freed otherwise
    // threads will try to access to freed memory triggering a sigsegv, it has to be done without realloc in a two-step
    // process
    hashtable_counters_volatile_t **counters = xalloc_alloc(sizeof(hashtable_counters_t*) * new_size);

    // Copy the previous set of pointers
    if (hashtable_data->thread_counters.list) {
        memcpy(
                (hashtable_counters_t*)*counters,
                (hashtable_counters_t*)*hashtable_data->thread_counters.list,
                sizeof(hashtable_counters_t*) * hashtable_data->thread_counters.size);
    }

    // Allocate the new structs for the new size
    for(uint32_t index = hashtable_data->thread_counters.size; index < new_size; index++) {
        counters[index] = xalloc_alloc_zero(sizeof(hashtable_counters_t));
    }

    // No need to use memory fences here, spinlock_unlock will do it for us
    hashtable_counters_volatile_t **old_counters = hashtable_data->thread_counters.list;
    hashtable_data->thread_counters.list = counters;
    hashtable_data->thread_counters.size = new_size;

    spinlock_unlock(&hashtable_data->thread_counters.lock);

    if (old_counters) {
        // At this point a memory fence has been issued so the old counters can be flushed away
        xalloc_free(old_counters);
    }
}

uint32_t hashtable_mcmp_thread_counters_fetch_new_index(
        hashtable_data_volatile_t *hashtable_data) {
    return hashtable_data->thread_counters.size;
}

hashtable_counters_volatile_t* hashtable_mcmp_thread_counters_get_by_index(
        hashtable_data_volatile_t *hashtable_data,
        uint32_t index) {
    MEMORY_FENCE_LOAD();

    // Ensure that the current thread_counters size contains the index being requested, if it's not the case it expands
    // the list as needed. This is necessary because the hashtable might be flushed, we don't want to deal with the
    // thread_counters at the high level.
    if (index >= hashtable_data->thread_counters.size) {
        hashtable_mcmp_thread_counters_expand_to(hashtable_data, index + 1);
    }

    return hashtable_data->thread_counters.list[index];
}

hashtable_counters_volatile_t* hashtable_mcmp_thread_counters_get_current_thread(
        hashtable_t* hashtable) {
    if (unlikely(!hashtable_mcmp_thread_counter_index_fetched)) {
        hashtable_mcmp_thread_counter_index = hashtable_mcmp_thread_counters_fetch_new_index(
                hashtable->ht_current);
        hashtable_mcmp_thread_counter_index_fetched = true;
    }

    return hashtable_mcmp_thread_counters_get_by_index(
            hashtable->ht_current,
            hashtable_mcmp_thread_counter_index);
}

void hashtable_mcmp_thread_counters_init(
        hashtable_data_volatile_t *hashtable_data,
        uint32_t initial_size) {
    hashtable_counters_volatile_t **counters = NULL;

    spinlock_init(&hashtable_data->thread_counters.lock);
    hashtable_data->thread_counters.list = NULL;
    hashtable_data->thread_counters.size = 0;

    hashtable_mcmp_thread_counters_expand_to(hashtable_data, initial_size);
}

void hashtable_mcmp_thread_counters_free(
        hashtable_data_volatile_t *hashtable_data) {
    for(uint32_t index = 0; index < hashtable_data->thread_counters.size; index++) {
        xalloc_free((hashtable_counters_t*)hashtable_data->thread_counters.list[index]);
    }
    xalloc_free(hashtable_data->thread_counters.list);
    hashtable_data->thread_counters.list = NULL;
    hashtable_data->thread_counters.size = 0;
}
