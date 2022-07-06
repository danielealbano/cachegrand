#ifndef CACHEGRAND_WORKER_IOURING_H
#define CACHEGRAND_WORKER_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct worker_iouring_context worker_iouring_context_t;
struct worker_iouring_context {
    uint32_t core_index;
    io_uring_t *ring;
};

worker_iouring_context_t* worker_iouring_context_get();

void worker_iouring_context_set(
        worker_iouring_context_t *worker_iouring_context);

void worker_iouring_context_reset();

bool worker_iouring_fds_map_add_and_enqueue_files_update(
        io_uring_t *ring,
        int fd,
        bool *has_mapped_fd,
        int *base_sqe_flags,
        int *wrapped_channel_fd);

bool worker_iouring_fds_map_files_update(
        io_uring_t *ring,
        int index,
        int fd,
        bool *has_mapped_fd,
        int *base_sqe_flags,
        int *wrapped_channel_fd);

int worker_iouring_fds_map_remove(
        int index);

bool worker_iouring_cqe_is_error_any(
        io_uring_cqe_t *cqe);

bool worker_iouring_cqe_is_error(
        io_uring_cqe_t *cqe);

void worker_iouring_cqe_log(
        io_uring_cqe_t *cqe);

bool worker_iouring_initialize(
        worker_context_t *worker_context,
        uint32_t max_fd,
        uint32_t entries);

void worker_iouring_cleanup(
        __attribute__((unused)) worker_context_t *worker_context);

bool worker_iouring_process_events_loop(
        worker_context_t *worker_context);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_IOURING_H
