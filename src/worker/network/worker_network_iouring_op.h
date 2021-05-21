#ifndef CACHEGRAND_WORKER_NETWORK_IOURING_OP_H
#define CACHEGRAND_WORKER_NETWORK_IOURING_OP_H

#ifdef __cplusplus
extern "C" {
#endif

bool worker_network_iouring_op_network_accept(
        worker_iouring_context_t *context,
        worker_op_network_accept_completion_cb_fp_t* accept_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *listener_channel,
        void* user_data);

bool worker_network_iouring_op_network_accept_wrapper(
        worker_op_network_accept_completion_cb_fp_t* accept_completion_cb,
        worker_op_network_error_completion_cb_fp_t* error_completion_cb,
        network_channel_t *listener_channel,
        void* user_data);

bool worker_network_iouring_op_network_receive_submit_sqe(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context);

bool worker_network_iouring_op_network_send_submit_sqe(
        worker_iouring_context_t *context,
        worker_iouring_op_context_t *op_context);

void worker_network_iouring_op_register();

bool worker_network_iouring_initialize(
        worker_context_t *worker_context);

void worker_network_iouring_listeners_listen_pre(
        worker_context_t *worker_context);

void worker_network_iouring_cleanup(
        worker_context_t *worker_context);

void worker_network_iouring_op_register();

//bool worker_iouring_process_op_timeout_ensure_loop(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t* ring,
//        io_uring_cqe_t *cqe);
//
//bool worker_iouring_process_op_accept(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t* ring,
//        io_uring_cqe_t *cqe);
//
//bool worker_iouring_process_op_recv_close_or_error(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t* ring,
//        io_uring_cqe_t *cqe);
//
//bool worker_iouring_process_op_recv(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t* ring,
//        io_uring_cqe_t *cqe);
//
//bool worker_iouring_process_op_send(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t* ring,
//        io_uring_cqe_t *cqe);
//
//void worker_iouring_thread_process_ops_loop(
//        worker_context_t *worker_context,
//        worker_stats_t* stats,
//        io_uring_t *ring);
//
//void* worker_iouring_thread_func(
//        void* user_data);


#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_IOURING_OP_H
