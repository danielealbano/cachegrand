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
#include "log/log.h"
#include "fatal.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "xalloc.h"
#include "ffma.h"

#include "ffma_thread_cache.h"

#define TAG "ffma_thread_cache"

static pthread_key_t ffma_thread_cache_key;

FUNCTION_CTOR(ffma_thread_cache_init_ctor, {
    pthread_key_create(&ffma_thread_cache_key, ffma_thread_cache_free);
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

void ffma_thread_cache_free(void *data) {
    ffma_t **thread_ffmas = data;

    for(int i = 0; i < FFMA_PREDEFINED_OBJECT_SIZES_COUNT; i++) {
        uint32_t object_size = ffma_predefined_object_sizes[i];
        uint32_t thread_ffmas_index = ffma_index_by_object_size(object_size);
        ffma_free(thread_ffmas[thread_ffmas_index]);
        thread_ffmas[thread_ffmas_index] = NULL;
    }

    xalloc_free(data);
}

ffma_t** ffma_thread_cache_get() {
    ffma_t **thread_ffmas = pthread_getspecific(ffma_thread_cache_key);
    return thread_ffmas;
}

void ffma_thread_cache_set(
        ffma_t** ffmas) {
    if (pthread_setspecific(ffma_thread_cache_key, ffmas) != 0) {
        FATAL(TAG, "Unable to set the fast fixed memory allocator thread cache");
    }
}

void ffma_thread_cache_ensure_init() {
    ffma_t **thread_ffmas = pthread_getspecific(ffma_thread_cache_key);
    if (unlikely(thread_ffmas == NULL)) {
        thread_ffmas = ffma_thread_cache_init();
        ffma_thread_cache_set(thread_ffmas);
    }
}

bool ffma_thread_cache_has() {
    return ffma_thread_cache_get() != NULL;
}
