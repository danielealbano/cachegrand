#ifndef CACHEGRAND_WORKER_IOURING_H
#define CACHEGRAND_WORKER_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

uint32_t worker_thread_set_affinity(
        uint32_t worker_index);

uint32_t worker_iouring_calculate_entries(
        uint32_t max_connections,
        uint32_t network_addresses_count);
io_uring_t* worker_iouring_initialize_iouring(
        uint32_t max_connections,
        uint32_t network_addresses_count);

void worker_iouring_network_listeners_initialize(
        worker_user_data_t *worker_user_data,
        network_channel_listener_new_callback_user_data_t *listener_new_cb_user_data);
void worker_iouring_listeners_enqueue(
        io_uring_t *ring,
        network_channel_listener_new_callback_user_data_t *create_listener_user_data);

bool worker_iouring_cqe_is_error_any(
        io_uring_cqe_t *cqe);
bool worker_iouring_cqe_is_error(
        io_uring_cqe_t *cqe);
void worker_iouring_cqe_report_error(
        worker_user_data_t *worker_user_data,
        io_uring_cqe_t *cqe);

bool worker_iouring_process_op_timeout(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe);
bool worker_iouring_process_op_accept(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe);
bool worker_iouring_process_op_recv(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe);
bool worker_iouring_process_op_send(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe);

void worker_iouring_thread_process_ops_loop(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t *ring);
void worker_iouring_cleanup(
        network_channel_listener_new_callback_user_data_t *create_listener_user_data,
        io_uring_t *ring);
void* worker_iouring_thread_func(
        void* user_data);


#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_IOURING_H
