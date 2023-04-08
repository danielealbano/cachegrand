#ifndef CACHEGRAND_WORKER_OP_H
#define CACHEGRAND_WORKER_OP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (worker_op_wait_fp_t)(
        long seconds,
        long long nanoseconds);

typedef bool (worker_op_wait_ms_fp_t)(
        uint64_t ms);

extern worker_op_wait_fp_t* worker_op_wait;
extern worker_op_wait_ms_fp_t* worker_op_wait_ms;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_OP_H
