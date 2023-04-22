/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>

#include "misc.h"
#include "log/log.h"
#include "xalloc.h"
#include "memory_fences.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "epoch_gc.h"

#include "epoch_gc_worker.h"

#define TAG "epoch_gc_worker"

bool epoch_gc_worker_should_terminate(
        epoch_gc_worker_context_t *context) {
    MEMORY_FENCE_LOAD();
    return *context->terminate_event_loop;
}

char* epoch_gc_worker_log_producer_set_early_prefix_thread(
        epoch_gc_worker_context_t *context) {
    size_t prefix_size = snprintf(
            NULL,
            0,
            EPOCH_GC_THREAD_LOG_PRODUCER_PREFIX_TEMPLATE,
            context->epoch_gc->object_type) + 1;
    char *prefix = xalloc_alloc_zero(prefix_size);

    snprintf(
            prefix,
            prefix_size,
            EPOCH_GC_THREAD_LOG_PRODUCER_PREFIX_TEMPLATE,
            context->epoch_gc->object_type);
    log_set_early_prefix_thread(prefix);

    return prefix;
}

char *epoch_gc_worker_set_thread_name(
        epoch_gc_worker_context_t *context) {
    size_t thread_name_size = snprintf(
            NULL,
            0,
            EPOCH_GC_THREAD_NAME_TEMPLATE,
            context->epoch_gc->object_type) + 1;

    // The thread name can be long max 16 chars including the nul terminator
    if (thread_name_size > 16) {
        thread_name_size = 16;
    }

    char *thread_name = xalloc_alloc_zero(thread_name_size);
    snprintf(
            thread_name,
            thread_name_size,
            EPOCH_GC_THREAD_NAME_TEMPLATE,
            context->epoch_gc->object_type);
    thread_name[thread_name_size - 1] = 0;

    if (pthread_setname_np(
            pthread_self(),
            thread_name) != 0) {
        LOG_E(TAG, "Unable to set name of the signal handler thread");
        LOG_E_OS_ERROR(TAG);
    }

    return thread_name;
}

void epoch_gc_worker_free_thread_list_cache(
        epoch_gc_thread_t **epoch_gc_thread_list_cache) {
    if (epoch_gc_thread_list_cache == NULL) {
        return;
    }

    xalloc_free(epoch_gc_thread_list_cache);
}

void epoch_gc_worker_build_thread_list_cache(
        epoch_gc_t *epoch_gc,
        epoch_gc_thread_t ***epoch_gc_thread_list_cache,
        uint64_t *epoch_gc_thread_list_change_epoch,
        uint32_t *epoch_gc_thread_list_length) {
    uint32_t epoch_gc_thread_list_index;

    // Lock the thread list
    spinlock_lock(&epoch_gc->thread_list_spinlock);

    // Fetch the length and build the array of cached threads
    *epoch_gc_thread_list_length = epoch_gc->thread_list->count;
    *epoch_gc_thread_list_cache = (epoch_gc_thread_t**)xalloc_alloc(
            *epoch_gc_thread_list_length * sizeof(epoch_gc_thread_t*));

    // Iterate over the double linked list
    epoch_gc_thread_list_index = 0;
    double_linked_list_item_t *item = NULL;
    while((item = double_linked_list_iter_next(epoch_gc->thread_list, item)) != NULL) {
        (*epoch_gc_thread_list_cache)[epoch_gc_thread_list_index] = (epoch_gc_thread_t*)item->data;
        epoch_gc_thread_list_index++;
    }

    // Fetch the epoch of the last change
    *epoch_gc_thread_list_change_epoch = epoch_gc->thread_list_change_epoch;

    // Unlock the thread list
    spinlock_unlock(&epoch_gc->thread_list_spinlock);
}

uint32_t epoch_gc_worker_collect_staged_objects(
        epoch_gc_thread_t **epoch_gc_thread_list_cache,
        uint32_t epoch_gc_thread_list_length,
        bool force) {
    uint32_t collected_objects = 0;

    // Iterate over the cached epoch gc threads
    for(
            uint32_t epoch_gc_thread_list_index = 0;
            epoch_gc_thread_list_index < epoch_gc_thread_list_length;
            epoch_gc_thread_list_index++) {
        if (unlikely(force)) {
            // Force an epoch advance to be able to collect everything
            epoch_gc_thread_advance_epoch_tsc(epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);
        }

        collected_objects += epoch_gc_thread_collect_all(
                epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);
    }

    return collected_objects;
}

void epoch_gc_worker_wait_for_epoch_gc_threads_termination(
        epoch_gc_thread_t **epoch_gc_thread_list_cache,
        uint32_t epoch_gc_thread_list_length) {
    // Wait for all the threads using this gc to be marked as terminated
    bool epoch_gc_all_terminated;
    do {
        epoch_gc_all_terminated = true;
        for(
                uint32_t epoch_gc_thread_list_index = 0;
                epoch_gc_thread_list_index < epoch_gc_thread_list_length;
                epoch_gc_thread_list_index++) {
            if (!epoch_gc_thread_is_terminated(epoch_gc_thread_list_cache[epoch_gc_thread_list_index])) {
                epoch_gc_all_terminated = false;
                break;
            }
        }
    } while(!epoch_gc_all_terminated);
}

void epoch_gc_worker_free_epoch_gc_thread(
        epoch_gc_thread_t **epoch_gc_thread_list_cache,
        uint32_t epoch_gc_thread_list_length) {
    for(
            uint32_t epoch_gc_thread_list_index = 0;
            epoch_gc_thread_list_index < epoch_gc_thread_list_length;
            epoch_gc_thread_list_index++) {
        epoch_gc_thread_unregister_global(epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);
        epoch_gc_thread_free(epoch_gc_thread_list_cache[epoch_gc_thread_list_index]);
    }
}

void epoch_gc_worker_main_loop(
        epoch_gc_worker_context_t *epoch_gc_worker_context) {
    epoch_gc_t *epoch_gc = epoch_gc_worker_context->epoch_gc;
    epoch_gc_thread_t **epoch_gc_thread_list_cache = NULL;
    uint32_t epoch_gc_thread_list_index = 0;
    uint64_t epoch_gc_thread_list_change_epoch = 0;

    do {
        usleep(EPOCH_GC_THREAD_LOOP_WAIT_TIME_MS * 1000);

        // Check if the cache of threads for the epoch_gc needs to be rebuilt
        MEMORY_FENCE_LOAD();
        if (epoch_gc_thread_list_change_epoch == 0 ||
            epoch_gc_thread_list_change_epoch != epoch_gc->thread_list_change_epoch) {
            epoch_gc_worker_free_thread_list_cache(epoch_gc_thread_list_cache);
            epoch_gc_worker_build_thread_list_cache(
                    epoch_gc,
                    &epoch_gc_thread_list_cache,
                    &epoch_gc_thread_list_change_epoch,
                    &epoch_gc_thread_list_index);
        }

        epoch_gc_worker_context->stats.collected_objects += epoch_gc_worker_collect_staged_objects(
                epoch_gc_thread_list_cache,
                epoch_gc_thread_list_index,
                false);
        MEMORY_FENCE_STORE();
    } while(!epoch_gc_worker_should_terminate(epoch_gc_worker_context));

    // Wait for all the thread attached to the current epoch gc to terminate before moving on
    epoch_gc_worker_wait_for_epoch_gc_threads_termination(
            epoch_gc_thread_list_cache,
            epoch_gc_thread_list_index);

    // All the threads are terminated, advance the epoch on the epoch_gc_thread, trigger a collect_all and at the end
    // free the structure
    epoch_gc_worker_context->stats.collected_objects += epoch_gc_worker_collect_staged_objects(
            epoch_gc_thread_list_cache,
            epoch_gc_thread_list_index,
            true);
    MEMORY_FENCE_STORE();

    // Unregister and free all the epoch_gc_thread
    epoch_gc_worker_free_epoch_gc_thread(
            epoch_gc_thread_list_cache,
            epoch_gc_thread_list_index);

    // Free up the cache
    epoch_gc_worker_free_thread_list_cache(epoch_gc_thread_list_cache);
}

bool epoch_gc_worker_teardown(
        char *log_producer_early_prefix_thread,
        char *thread_name) {
    bool res = true;

    LOG_V(TAG, "Tearing down epoch gc worker");

    xalloc_free(log_producer_early_prefix_thread);
    xalloc_free(thread_name);
    log_unset_early_prefix_thread();

    return res;
}

void* epoch_gc_worker_func(
        void* user_data) {
    epoch_gc_worker_context_t *epoch_gc_worker_context = (epoch_gc_worker_context_t*)user_data;

    // Initial setup
    char *log_producer_early_prefix_thread =
            epoch_gc_worker_log_producer_set_early_prefix_thread(epoch_gc_worker_context);
    char *thread_name = epoch_gc_worker_set_thread_name(epoch_gc_worker_context);

    // Thread main loop
    epoch_gc_worker_main_loop(epoch_gc_worker_context);

    // Teardown
    epoch_gc_worker_teardown(
            log_producer_early_prefix_thread,
            thread_name);

    return NULL;
}
