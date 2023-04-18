#ifndef CACHEGRAND_FFMA_THREAD_CACHE_H
#define CACHEGRAND_FFMA_THREAD_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ffma ffma_t;

extern thread_local ffma_t **thread_local_ffmas;

ffma_t **ffma_thread_cache_init();

void ffma_thread_cache_free(
        ffma_t **thread_ffmas);

ffma_t **ffma_thread_cache_get();

void ffma_thread_cache_set(
        ffma_t **ffmas);

static inline bool ffma_thread_cache_has() {
    return thread_local_ffmas != NULL;
}

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FFMA_THREAD_CACHE_H
