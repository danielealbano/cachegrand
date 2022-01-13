#ifndef CACHEGRAND_WORKER_OP_H
#define CACHEGRAND_WORKER_OP_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_TIMER_LOOP_MS 500l

typedef bool (worker_op_timer_fp_t)(
        long seconds,
        long long nanoseconds);

void worker_timer_fiber_entrypoint(
        void *user_data);

void worker_timer_setup(
        worker_context_t* worker_context);

extern worker_op_timer_fp_t* worker_op_timer;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_OP_H
