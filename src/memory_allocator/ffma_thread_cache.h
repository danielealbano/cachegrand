#ifndef CACHEGRAND_FFMA_THREAD_CACHE_H
#define CACHEGRAND_FFMA_THREAD_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ffma ffma_t;

ffma_t **ffma_thread_cache_init();

void ffma_thread_cache_free(
        void *data);

ffma_t **ffma_thread_cache_get();

void ffma_thread_cache_set(
        ffma_t **ffmas);

void ffma_thread_cache_ensure_init();

bool ffma_thread_cache_has();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FFMA_THREAD_CACHE_H
