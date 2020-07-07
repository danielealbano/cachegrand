#include <stdlib.h>
#include <string.h>
#include <liburing.h>

#include "misc.h"
#include "log.h"
#include "xalloc.h"

#include "network_io_iouring.h"

LOG_PRODUCER_CREATE_LOCAL_DEFAULT("network_io_iouring", network_io_iouring)

io_uring_t* network_io_iouring_init(
        uint32_t entries,
        io_uring_params_t *io_uring_params,
        uint32_t *features) {
    io_uring_t *io_uring;
    io_uring_params_t temp_io_uring_params = {0};

    io_uring = xalloc_alloc(sizeof(io_uring_t));

    if (io_uring_params == NULL) {
        io_uring_params = &temp_io_uring_params;
    }

    if (io_uring_queue_init_params(entries, io_uring, io_uring_params) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to allocate io_uring using the given params");
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);

        xalloc_free(io_uring);

        return NULL;
    }

    if (features != NULL) {
        *features = io_uring_params->features;
    }

    return io_uring;
}

bool network_io_iouring_probe_feature(
        uint32_t features,
        uint32_t feature) {
    return (features & feature) == feature;
}

bool network_io_iouring_probe_opcode(
        io_uring_t *io_uring,
        uint8_t opcode) {
    struct io_uring_probe *probe;
    bool res = true;

    probe = io_uring_get_probe_ring(io_uring);
    if (!probe) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to allocate or fetch the supported io_uring opcodes");
        res = false;
    } else {
        res = io_uring_opcode_supported(probe, opcode);
        free(probe);
    }

    return res;
}

void network_io_iouring_sqe_enqueue_accept(
        io_uring_t *ring,
        int fd,
        struct sockaddr *socket_address,
        socklen_t *socket_address_size,
        unsigned flags,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_get_sqe(ring);
    io_uring_prep_accept(sqe, fd, socket_address, socket_address_size, 0);
    io_uring_sqe_set_flags(sqe, flags);
    sqe->user_data = user_data;
}

void network_io_iouring_sqe_enqueue_recv(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_get_sqe(ring);
    io_uring_prep_recv(sqe, fd, buffer, buffer_size, 0);
    io_uring_sqe_set_flags(sqe, 0);
    sqe->user_data = user_data;
}

void network_io_iouring_sqe_enqueue_send(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_get_sqe(ring);
    io_uring_prep_send(sqe, fd, buffer, buffer_size, 0);
    io_uring_sqe_set_flags(sqe, 0);
    sqe->user_data = user_data;
}

bool network_io_iouring_sqe_submit(
        io_uring_t *ring) {
    if (io_uring_submit(ring) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Failed to submit the io_uring sqes");

        return false;
    }

    return true;
}

bool network_io_iouring_sqe_submit_and_wait(
        io_uring_t *ring,
        int wait_nr) {
    if (io_uring_submit_and_wait(ring, wait_nr) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Failed to submit the io_uring sqes");

        return false;
    }

    return true;
}
