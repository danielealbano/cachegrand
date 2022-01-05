#ifndef CACHEGRAND_WORKER_IOURING_OP_H
#define CACHEGRAND_WORKER_IOURING_OP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct worker_iouring_context worker_iouring_context_t;
typedef struct worker_iouring_op_context worker_iouring_op_context_t;

typedef bool (worker_iouring_op_wrapper_completion_cb_fp_t)(
        worker_iouring_context_t* worker_iouring_context,
        worker_iouring_op_context_t* worker_iouring_op_context,
        io_uring_cqe_t *cqe,
        bool *free_op_context);

bool worker_iouring_op_fds_map_files_update_cb(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context,
        io_uring_cqe_t *cqe,
        bool *free_op_context);

void worker_iouring_op_register();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_IOURING_OP_H
