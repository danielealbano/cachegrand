#ifndef CACHEGRAND_WORKER_FIBER_SCHEDULER_H
#define CACHEGRAND_WORKER_FIBER_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

void worker_fiber_scheduler_switch_to(fiber_t *fiber);

void worker_fiber_scheduler_switch_back();

fiber_t *worker_fiber_scheduler_get_current();

void worker_fiber_scheduler_ensure_in_fiber();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_FIBER_SCHEDULER_H
