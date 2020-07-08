#ifndef CACHEGRAND_NETWORK_IO_IOURING_H
#define CACHEGRAND_NETWORK_IO_IOURING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct io_uring io_uring_t;
typedef struct io_uring_params io_uring_params_t;
typedef struct io_uring_sqe io_uring_sqe_t;
typedef struct io_uring_cqe io_uring_cqe_t;

io_uring_t* network_io_iouring_init(
        uint32_t entries,
        io_uring_params_t *io_uring_params,
        uint32_t *features);
void network_io_iouring_free(
        io_uring_t *io_uring);
bool network_io_iouring_probe_feature(
        uint32_t features,
        uint32_t feature);
bool network_io_iouring_probe_opcode(
        io_uring_t *io_uring,
        uint8_t opcode);
void network_io_iouring_sqe_enqueue_accept(
        io_uring_t *ring,
        int fd,
        struct sockaddr *socket_address,
        socklen_t *socket_address_size,
        unsigned flags,
        uint64_t user_data);
void network_io_iouring_sqe_enqueue_recv(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        uint64_t user_data);
void network_io_iouring_sqe_enqueue_send(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        uint64_t user_data);
bool network_io_iouring_sqe_submit(
        io_uring_t *ring);
bool network_io_iouring_sqe_submit_and_wait(
        io_uring_t *ring,
        int wait_nr);

#define network_io_iouring_cqe_foreach io_uring_for_each_cqe

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_IO_IOURING_H
