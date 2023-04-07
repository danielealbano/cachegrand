#ifndef CACHEGRAND_WORKER_IOURING_OP_H
#define CACHEGRAND_WORKER_IOURING_OP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct worker_iouring_context worker_iouring_context_t;

void worker_iouring_op_register();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_IOURING_OP_H
