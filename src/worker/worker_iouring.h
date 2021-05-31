#ifndef CACHEGRAND_WORKER_IOURING_H
#define CACHEGRAND_WORKER_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct worker_iouring_context worker_iouring_context_t;
struct worker_iouring_context {
    worker_context_t *worker_context;
    io_uring_t *ring;
};

worker_iouring_context_t* worker_iouring_context_get();

void worker_iouring_context_set(
        worker_iouring_context_t *worker_iouring_context);

void worker_iouring_context_reset();

int32_t worker_iouring_fds_map_add_and_enqueue_files_update(
        io_uring_t *ring,
        network_channel_iouring_t *channel);

int worker_iouring_fds_map_remove(
        int index);

char* worker_iouring_get_callback_function_name(
        void* callback,
        char* callback_function_name,
        size_t callback_function_name_size);

bool worker_iouring_cqe_is_error_any(
        io_uring_cqe_t *cqe);

bool worker_iouring_cqe_is_error(
        io_uring_cqe_t *cqe);

void worker_iouring_cqe_log(
        io_uring_cqe_t *cqe);

uint32_t worker_iouring_calculate_fds_count(
        uint32_t workers_count,
        uint32_t max_connections,
        uint32_t network_addresses_count);

bool worker_iouring_initialize(
        worker_context_t *worker_user_data);

void worker_iouring_cleanup(
        worker_context_t *worker_user_data);

bool worker_iouring_process_events_loop(
        worker_context_t *worker_user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_IOURING_H
