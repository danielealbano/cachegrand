#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <liburing.h>

#include "misc.h"
#include "log.h"
#include "fatal.h"
#include "xalloc.h"

#include "io_uring_support.h"

LOG_PRODUCER_CREATE_DEFAULT("io_uring_support", io_uring_support)

io_uring_support_feature_t io_uring_support_features[] = {
        { "IORING_FEAT_SINGLE_MMAP",    IORING_FEAT_SINGLE_MMAP },
        { "IORING_FEAT_NODROP",         IORING_FEAT_NODROP },
        { "IORING_FEAT_SUBMIT_STABLE",  IORING_FEAT_SUBMIT_STABLE },
        { "IORING_FEAT_RW_CUR_POS",     IORING_FEAT_RW_CUR_POS },
        { "IORING_FEAT_CUR_PERSONALITY",IORING_FEAT_CUR_PERSONALITY },
        { "IORING_FEAT_FAST_POLL",      IORING_FEAT_FAST_POLL }
};
#define IO_URING_SUPPORT_FEATURES_COUNT \
    (sizeof(io_uring_support_features) / sizeof(io_uring_support_feature_t))

io_uring_t* io_uring_support_init(
        uint32_t entries,
        io_uring_params_t *io_uring_params,
        uint32_t *features) {
    int res;
    io_uring_t *io_uring;
    io_uring_params_t temp_io_uring_params = {0};

    io_uring = xalloc_alloc(sizeof(io_uring_t));

    if (io_uring_params == NULL && features != NULL) {
        io_uring_params = &temp_io_uring_params;
    }

    if (io_uring_params) {
        res = io_uring_queue_init_params(entries, io_uring, io_uring_params);
    } else {
        res = io_uring_queue_init(entries, io_uring, 0);
    }

    if (res < 0) {
        // If there isn't enough memory hard fail
        if (errno == -ENOMEM) {
            FATAL(LOG_PRODUCER_DEFAULT, "Unable to allocate or lock enough memory to initialize io_uring, please check the available memory and/or increase the memlock ulimit.");
        } else {
            LOG_E(LOG_PRODUCER_DEFAULT, "Unable to allocate io_uring using the given params");
            LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
        }

        xalloc_free(io_uring);

        return NULL;
    }

    if (features != NULL) {
        *features = io_uring_params->features;
    }

    return io_uring;
}

void io_uring_support_free(
        io_uring_t *io_uring) {
    io_uring_queue_exit(io_uring);
    xalloc_free(io_uring);
}

bool io_uring_support_probe_opcode(
        uint8_t opcode) {
    struct io_uring_probe *probe;
    bool res = true;

    probe = io_uring_get_probe();
    if (!probe) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to allocate or fetch the supported io_uring opcodes");
        res = false;
    } else {
        res = io_uring_opcode_supported(probe, opcode);
        free(probe);
    }

    return res;
}

char* io_uring_support_features_str(
        char* buffer,
        size_t buffer_size) {
    io_uring_params_t params = {0};
    io_uring_t* ring = io_uring_support_init(10, &params, NULL);

    if (ring == NULL) {
        return NULL;
    }

    for(
            uint32_t feature_index = 0;
            feature_index < IO_URING_SUPPORT_FEATURES_COUNT;
            feature_index++) {
        if ((params.features & io_uring_support_features[feature_index].id)) {
            if (strlen(buffer) > 0) {
                strncat(
                        buffer,
                        ", ",
                        buffer_size - strlen(buffer) - 1);
            }

            strncat(
                    buffer,
                    io_uring_support_features[feature_index].name,
                    buffer_size - strlen(buffer) - 1);

        }
    }

    io_uring_support_free(ring);

    return buffer;
}

io_uring_sqe_t* io_uring_support_get_sqe(
        io_uring_t *ring) {
    io_uring_sqe_t *sqe = io_uring_get_sqe(ring);
    if (sqe == NULL) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Failed to fetch an sqe, queue full");
    }

    return sqe;
}

void io_uring_support_cq_advance(
        io_uring_t *ring,
        uint32_t count) {
    io_uring_cq_advance(ring, count);
}

// TODO: implement test
bool io_uring_support_sqe_enqueue_timeout(
        io_uring_t *ring,
        uint64_t count,
        struct __kernel_timespec *ts,
        uint8_t sqe_flags,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
    if (sqe == NULL) {
        return false;
    }

    io_uring_prep_timeout(sqe, ts, count, 0);
    io_uring_sqe_set_flags(sqe, sqe_flags);
    sqe->user_data = user_data;

    return true;
}

// TODO: implement test
bool io_uring_support_sqe_enqueue_nop(
        io_uring_t *ring,
        uint8_t sqe_flags,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
    if (sqe == NULL) {
        return false;
    }

    io_uring_prep_nop(sqe);
    io_uring_sqe_set_flags(sqe, sqe_flags);
    sqe->user_data = user_data;

    return true;
}

bool io_uring_support_sqe_enqueue_files_update(
        io_uring_t *ring,
        int *fds,
        uint32_t fds_count,
        uint32_t offset,
        uint8_t sqe_flags,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
    if (sqe == NULL) {
        return false;
    }

    io_uring_prep_files_update(sqe, fds, fds_count, offset);
    io_uring_sqe_set_flags(sqe, sqe_flags);
    sqe->user_data = user_data;

    return true;
}

bool io_uring_support_sqe_enqueue_accept(
        io_uring_t *ring,
        int fd,
        struct sockaddr *socket_address,
        socklen_t *socket_address_size,
        int op_flags,
        uint8_t sqe_flags,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
    if (sqe == NULL) {
        return false;
    }

    io_uring_prep_accept(sqe, fd, socket_address, socket_address_size, op_flags);
    io_uring_sqe_set_flags(sqe, sqe_flags);
    sqe->user_data = user_data;

    return true;
}

bool io_uring_support_sqe_enqueue_recv(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        int op_flags,
        uint8_t sqe_flags,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
    if (sqe == NULL) {
        return false;
    }

    io_uring_prep_recv(sqe, fd, buffer, buffer_size, op_flags);
    io_uring_sqe_set_flags(sqe, sqe_flags);
    sqe->user_data = user_data;

    return true;
}

bool io_uring_support_sqe_enqueue_send(
        io_uring_t *ring,
        int fd,
        void *buffer,
        size_t buffer_size,
        int op_flags,
        uint8_t sqe_flags,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
    if (sqe == NULL) {
        return false;
    }

    io_uring_prep_send(sqe, fd, buffer, buffer_size, op_flags);
    io_uring_sqe_set_flags(sqe, sqe_flags);
    sqe->user_data = user_data;

    return true;
}

bool io_uring_support_sqe_enqueue_close(
        io_uring_t *ring,
        int fd,
        uint8_t sqe_flags,
        uint64_t user_data) {
    io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
    if (sqe == NULL) {
        return false;
    }

    io_uring_prep_close(sqe, fd);
    io_uring_sqe_set_flags(sqe, sqe_flags);
    sqe->user_data = user_data;

    return true;
}

bool io_uring_support_sqe_submit(
        io_uring_t *ring) {
    if (io_uring_submit(ring) < 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Failed to submit the io_uring sqes");

        return false;
    }

    return true;
}

bool io_uring_support_sqe_submit_and_wait(
        io_uring_t *ring,
        int wait_nr) {
    int res;
    if ((res = io_uring_submit_and_wait(ring, wait_nr)) < 0) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Failed to submit the io_uring sqes, error code <%s (%d)>",
                strerror(res),
                res);

        return false;
    }

    return true;
}
