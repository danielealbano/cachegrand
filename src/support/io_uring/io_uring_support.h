#ifndef CACHEGRAND_IO_URING_SUPPORT_H
#define CACHEGRAND_IO_URING_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct io_uring io_uring_t;
typedef struct io_uring_params io_uring_params_t;
typedef struct io_uring_sqe io_uring_sqe_t;
typedef struct io_uring_cqe io_uring_cqe_t;

typedef struct io_uring_support_feature io_uring_support_feature_t;
struct io_uring_support_feature {
    char* name;
    uint32_t id;
};

io_uring_t* io_uring_support_init(
        uint32_t entries,
        io_uring_params_t *io_uring_params,
        uint32_t *features);

void io_uring_support_free(
        io_uring_t *io_uring);

bool io_uring_support_probe_opcode(
        uint8_t opcode);

char* io_uring_support_features_str(
        char* buffer,
        size_t buffer_size);

io_uring_sqe_t* io_uring_support_get_sqe(
        io_uring_t *ring);

void io_uring_support_cq_advance(
        io_uring_t *ring,
        uint32_t count);

bool io_uring_support_sqe_enqueue_files_update(
        io_uring_t *ring,
        int *fds,
        uint32_t fds_count,
        uint32_t offset,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_timeout(
        io_uring_t *ring,
        uint64_t count,
        struct __kernel_timespec *ts,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_nop(
        io_uring_t *ring,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_accept(
        io_uring_t *ring,
        int fd,
        struct sockaddr *socket_address,
        socklen_t *socket_address_size,
        int op_flags,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_recv(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        int op_flags,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_send(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        int op_flags,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_openat(
        io_uring_t *ring,
        int dirfd,
        char *path,
        int flags,
        mode_t mode,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_readv(
        io_uring_t *ring,
        int fd,
        struct iovec *iov,
        size_t iov_nr,
        off_t offset,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_writev(
        io_uring_t *ring,
        int fd,
        struct iovec *iov,
        size_t iov_nr,
        off_t offset,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_fsync(
        io_uring_t *ring,
        int fd,
        int flags,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_fallocate(
        io_uring_t *ring,
        int fd,
        int mode,
        off_t offset,
        off_t len,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_enqueue_close(
        io_uring_t *ring,
        int fd,
        uint8_t sqe_flags,
        uint64_t user_data);

bool io_uring_support_sqe_submit(
        io_uring_t *ring);
bool io_uring_support_sqe_submit_and_wait(
        io_uring_t *ring,
        int wait_nr);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_IO_URING_SUPPORT_H
