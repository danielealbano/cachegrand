#ifndef CACHEGRAND_WORKER_FIBER_H
#define CACHEGRAND_WORKER_FIBER_H

#ifdef __cplusplus
extern "C" {
#endif

bool worker_fiber_init(
        worker_context_t* worker_context);

bool worker_fiber_register(
        worker_context_t *worker_context,
        char *fiber_name,
        fiber_scheduler_entrypoint_fp_t *fiber_entrypoint,
        fiber_scheduler_new_fiber_user_data_t *fiber_user_data);

void worker_fiber_free(
        worker_context_t *worker_context);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_FIBER_H
