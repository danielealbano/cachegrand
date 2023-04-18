/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
#include "ffma.h"

#include "ffma_thread_cache.h"

#define TAG "ffma_thread_cache"

static pthread_key_t ffma_thread_cache_destructor_key;
thread_local ffma_t** thread_local_ffmas = NULL;

FUNCTION_CTOR(ffma_thread_cache_init_ctor, {
    pthread_key_create(&ffma_thread_cache_destructor_key, NULL);
})

ffma_t **ffma_thread_cache_init() {
    ffma_t **thread_ffmas = (ffma_t**)xalloc_alloc_zero(
            FFMA_PREDEFINED_OBJECT_SIZES_COUNT * (sizeof(ffma_t*) + 1));

    for(int i = 0; i < FFMA_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = ffma_predefined_object_sizes[i];
        thread_ffmas[ffma_index_by_object_size(object_size)] = ffma_init(object_size);
    }

    return thread_ffmas;
}

void ffma_thread_cache_free(
        ffma_t **thread_ffmas) {
    for(int i = 0; i < FFMA_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = ffma_predefined_object_sizes[i];
        uint32_t thread_ffmas_index = ffma_index_by_object_size(object_size);
        ffma_free(thread_ffmas[thread_ffmas_index]);
        thread_ffmas[thread_ffmas_index] = NULL;
    }

    xalloc_free(thread_ffmas);
}

ffma_t** ffma_thread_cache_get() {
    return thread_local_ffmas;
}

void ffma_thread_cache_set(
        ffma_t** ffmas) {
    thread_local_ffmas = ffmas;

    // Required to trigger the destructor
    pthread_setspecific(ffma_thread_cache_destructor_key, (void*)1);
}

