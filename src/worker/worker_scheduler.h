#ifndef CACHEGRAND_WORKER_SCHEDULER_H
#define CACHEGRAND_WORKER_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

bool worker_scheduler_is_fiber_running();

fiber_t *worker_scheduler_get_running_fiber();

void worker_scheduler_switch_to_fiber(
        fiber_t* fiber);

void worker_scheduler_switch_back();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_SCHEDULER_H
