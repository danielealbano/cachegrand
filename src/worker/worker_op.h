#ifndef CACHEGRAND_WORKER_OP_H
#define CACHEGRAND_WORKER_OP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (worker_op_timer_fp_t)(
        long seconds,
        long long nanoseconds);

extern worker_op_timer_fp_t* worker_op_timer;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_OP_H
