#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <liburing.h>

#include "exttypes.h"
#include "misc.h"
#include "log.h"
#include "fatal.h"
#include "xalloc.h"
#include "thread.h"
#include "memory_fences.h"
#include "io_uring_support.h"
#include "io_uring_capabilities.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "pow2.h"
#include "worker.h"

#include "worker_iouring.h"

LOG_PRODUCER_CREATE_THREAD_LOCAL_DEFAULT("worker_iouring", worker_iouring)

static thread_local int *fds_map_registered = NULL;
static thread_local int *fds_map = NULL;
static thread_local uint32_t fds_map_count = 0;
static thread_local uint32_t fds_map_mask = 0;
static thread_local uint32_t fds_map_last_free = 0;

static thread_local bool op_files_update_link_support = false;

bool worker_fds_map_files_update(
        io_uring_t *ring,
        uint32_t index,
        int fd) {

    bool ret;

    fds_map[index] = fd;

    if (likely(op_files_update_link_support)) {
        // TODO: use slab allocator
        network_channel_iouring_entry_user_data_t *iouring_userdata_new = network_channel_iouring_entry_user_data_new_with_fd(
                NETWORK_IO_IOURING_OP_FILES_UPDATE,
                index);

        io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
        if (sqe != NULL) {
            io_uring_prep_files_update(sqe, &fds_map[index], 1, index);
            io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
            sqe->user_data = (uintptr_t)iouring_userdata_new;

            ret = true;
        } else {
            ret = false;
        }
    } else {
        ret = io_uring_register_files_update(ring, index, &fds_map[index], 1) == 1;
    }

    if (!ret) {
        fds_map[index] = 0;

        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Failed to update the registered fd <%d> with index <%u> in the registered files", fd, index);
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
    }

    return ret;
}

int32_t worker_fds_map_find_free_index() {
    for(uint32_t i = 0; i < fds_map_count; i++) {
        uint32_t fds_map_index = (i + fds_map_last_free) & fds_map_mask;
        if (fds_map[fds_map_index] == -1) {
            return fds_map_index;
        }
    }

    LOG_E(
            LOG_PRODUCER_DEFAULT,
            "Unable to find a free slot for an fd in the fds map");

    return -1;
}

int32_t worker_fds_map_add(
        io_uring_t *ring,
        int fd) {
    int32_t index;

    if ((index = worker_fds_map_find_free_index()) < 0) {
        return -1;
    }

    LOG_D(
            LOG_PRODUCER_DEFAULT,
            "Registering fd <%d> with index <%d>", fd, index);

    if (!worker_fds_map_files_update(ring, index, fd)) {
        return -1;
    }

    fds_map_last_free = (index + 1) & fds_map_mask;

    return index;
}

int32_t worker_fds_map_remove(
        io_uring_t *ring,
        int index) {
    int fd;

    fd = fds_map[index];

    LOG_D(
            LOG_PRODUCER_DEFAULT,
            "Unregistering fd <%d> from index <%d>", fd, index);

    if (!worker_fds_map_files_update(ring, index, -1)) {
        return -1;
    }

    return index;
}

bool worker_fds_register(
        uint32_t max_connections,
        uint32_t network_addresses_count,
        io_uring_t *ring) {

    // pow2_next may return a value greater than UINT32_MAX but it would mean that we are requesting to handle more than
    // 4 billions of FDS with this thread, that can't simply the case -- EVER.
    // The caller should ensure this will never happen.
    fds_map_count = max_connections + network_addresses_count;
    fds_map_count = pow2_next(fds_map_count);
    fds_map_mask = fds_map_count - 1;
    fds_map_registered = xalloc_alloc(sizeof(int) * fds_map_count);
    fds_map = xalloc_alloc(sizeof(int) * fds_map_count);

    for(uint32_t fds_map_index = 0; fds_map_index < fds_map_count; fds_map_index++) {
        fds_map[fds_map_index] = -1;
        fds_map_registered[fds_map_index] = -1;
    }

    if (io_uring_register_files(ring, fds_map_registered, fds_map_count) < 0) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Failed to register the fds_map with io_uring");
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);

        io_uring_support_free(ring);

        return false;
    }

    return true;
}

uint32_t worker_thread_set_affinity(
        uint32_t worker_index) {
    return thread_current_set_affinity(worker_index);
}

uint32_t worker_iouring_calculate_entries(
        uint32_t max_connections,
        uint32_t network_addresses_count) {
    // TODO: this should take into account the storage as well although the math should already be fine
    return (max_connections * 2) + (network_addresses_count * 2);
}

io_uring_t* worker_iouring_initialize_iouring(
        uint32_t core_index,
        uint32_t max_connections,
        uint32_t network_addresses_count) {
    io_uring_t *ring;
    io_uring_params_t *params = xalloc_alloc_zero(sizeof(io_uring_params_t));

    // TODO: fix this
//    params->flags = IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF;
//    params->sq_thread_cpu = core_index;
//    params->sq_thread_idle = 1000;

    ring = io_uring_support_init(
            worker_iouring_calculate_entries(max_connections, network_addresses_count),
            params,
            NULL);

    if (worker_fds_register(max_connections, network_addresses_count, ring) == false) {
        io_uring_support_free(ring);
        ring = NULL;
    }

    return ring;
}

void worker_iouring_network_listeners_initialize(
        worker_user_data_t *worker_user_data,
        network_channel_listener_new_callback_user_data_t *listener_new_cb_user_data) {
    for(
            uint32_t address_index = 0;
            address_index < worker_user_data->addresses_count;
            address_index++) {
        if (network_channel_listener_new(
                worker_user_data->addresses[address_index].address,
                worker_user_data->addresses[address_index].port,
                listener_new_cb_user_data) == false) {
            LOG_E(LOG_PRODUCER_DEFAULT, "Unable to setup listener for <%s:%u>",
                  worker_user_data->addresses[address_index].address,
                  worker_user_data->addresses[address_index].port);
        }
    }
}

void worker_iouring_listeners_enqueue(
        io_uring_t *ring,
        network_channel_listener_new_callback_user_data_t *listener_new_cb_user_data) {
    network_channel_iouring_entry_user_data_t *iouring_user_data;

    // TODO: really ... handle errors
    for(
            uint32_t listener_index = 0;
            listener_index < listener_new_cb_user_data->listeners_count;
            listener_index++) {
        network_channel_listener_t *listener = &listener_new_cb_user_data->listeners[listener_index];

        if ((iouring_user_data = network_channel_iouring_entry_user_data_new(
                NETWORK_IO_IOURING_OP_ACCEPT)) == NULL) {
            // TODO: handle error properly
            continue;
        }
        iouring_user_data->address_size = listener->address_size;

        if ((iouring_user_data->fd = worker_fds_map_add(
                ring,
                listener->fd)) < 0) {
            // TODO: handle error
            continue;
        }

        // TODO: This is VERY wrong, need to move the fds into a different struct and keep track of the sockaddr on
        //       which it's binded!
        network_channel_listener_t temp = {0};
        getsockname(
                listener->fd,
                (struct sockaddr*)&temp.address.base,
                &listener->address_size);

        network_io_common_socket_address_str(
                &temp.address.base,
                (char *)&iouring_user_data->address_str,
                sizeof(iouring_user_data->address_str));

        if (io_uring_support_sqe_enqueue_accept(
                ring,
                listener->fd,
                &iouring_user_data->new_socket_address.base,
                &iouring_user_data->address_size,
                0,
                0,
                (uintptr_t)iouring_user_data) == false) {

            // TODO: handle error
            // TODO: free up memory and registered fds
            continue;
        }
    }
}

bool worker_iouring_cqe_is_error_any(
        io_uring_cqe_t *cqe) {
    return cqe->res < 0;
}

bool worker_iouring_cqe_is_error(
        io_uring_cqe_t *cqe) {
    return cqe->res != -ETIME && cqe->res != -ECONNRESET && cqe->res < 0;
}

void worker_iouring_cqe_log(
        worker_user_data_t *worker_user_data,
        io_uring_cqe_t *cqe) {
    network_channel_iouring_entry_user_data_t *iouring_userdata;
    iouring_userdata = (network_channel_iouring_entry_user_data_t*)cqe->user_data;

    LOG_E(
            LOG_PRODUCER_DEFAULT,
            "[OP:%u][FD IDX:%d][FD:%d] cqe->user_data = <0x%08lx>, cqe->res = <%s (%d)>, cqe->flags >> 16 = <%d>, cqe->flags >> 16 = <%d>",
            iouring_userdata->op,
            iouring_userdata->fd,
            iouring_userdata->fd >= 0 ? fds_map[iouring_userdata->fd] : -1,
            cqe->user_data,
            cqe->res >= 0 ? "Success" : strerror(cqe->res * -1),
            cqe->res,
            cqe->flags >> 16u,
            cqe->flags & 0xFFFFu);
}

bool worker_iouring_process_op_files_update(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe) {
    network_channel_iouring_entry_user_data_t *iouring_userdata_current;
    iouring_userdata_current = (network_channel_iouring_entry_user_data_t*)cqe->user_data;

    if (cqe->res < 0) {
        fds_map[iouring_userdata_current->fd] = 0;
    }

    // TODO: use slab allocator
    xalloc_free(iouring_userdata_current);

    return true;
}

bool worker_iouring_process_op_timeout_ensure_loop(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe) {
    network_channel_iouring_entry_user_data_t *iouring_userdata_current;

    // Has to be static, it will be use AFTER the function will end and we need to preserve the memory
    static struct __kernel_timespec ts = {
            0,
            WORKER_LOOP_MAX_WAIT_TIME_MS * 1000000
    };

    if (cqe == NULL) {
        iouring_userdata_current = network_channel_iouring_entry_user_data_new(WORKER_IOURING_OP_TIMEOUT_ENSURE_LOOP);
    } else {
        iouring_userdata_current = (network_channel_iouring_entry_user_data_t *)cqe->user_data;
    }

    return io_uring_support_sqe_enqueue_timeout(
            ring,
            1,
            &ts,
            0,
            (uintptr_t)iouring_userdata_current);
}

bool worker_iouring_process_op_accept(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe) {
    int new_fd;
    network_channel_iouring_entry_user_data_t *iouring_userdata_current, *iouring_userdata_new;
    iouring_userdata_current = (network_channel_iouring_entry_user_data_t*)cqe->user_data;

    if (worker_iouring_cqe_is_error_any(cqe)) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Error while waiting for connections on listener <%s>",
                iouring_userdata_current->address_str);
        worker_request_terminate(worker_user_data);

        return false;
    }

    new_fd = cqe->res;

    iouring_userdata_new = network_channel_iouring_entry_user_data_new(
            NETWORK_IO_IOURING_OP_RECV);
    network_io_common_socket_address_str(
            &iouring_userdata_current->new_socket_address.base,
            (char *)&iouring_userdata_new->address_str,
            sizeof(iouring_userdata_new->address_str));

    LOG_V(
            LOG_PRODUCER_DEFAULT,
            "Listener <%s> accepting new connection from <%s>",
            iouring_userdata_current->address_str,
            iouring_userdata_new->address_str);

    if (io_uring_support_sqe_enqueue_accept(
            ring,
            iouring_userdata_current->fd,
            &iouring_userdata_current->new_socket_address.base,
            &iouring_userdata_current->address_size,
            0,
            IOSQE_FIXED_FILE,
            (uintptr_t)iouring_userdata_current) == false) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Can't start to listen again on listener <%s>",
                iouring_userdata_current->address_str);

        xalloc_free(iouring_userdata_new);

        // TODO: close client socket properly
        // TODO: close listener socket properly
        // TODO: remove the fd from the registered files
        // TODO: handle failure in a more appropriate way (ie. the world has ended, let other worker know)
        return false;
    }

    // Because potentially network_channel_client_setup may fail, the address of new connection must be extracted and the
    // accept operation on the lister must be enqueue-ed. If the operation fails the newly created iouring_userdata_new
    // will be freed and the operation marked as failed.
    // Currently the main loop of the worker is ignoring the return value because there isn't anything really that
    // should be done apart continuing the execution and freeing up any allocated memory.
    if (network_channel_client_setup(
            new_fd,
            worker_user_data->core_index) == false) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Can't accept the connection <%s> coming from listener <%s>",
                iouring_userdata_new->address_str,
                iouring_userdata_current->address_str);

        // TODO: close client socket properly
        // TODO: implement ad hoc function to free the iouring_entry_user_data (when will implement the SLAB allocator
        //       that function will have to rely on it)
        xalloc_free(iouring_userdata_new);

        return false;
    }

    // TODO: Use SLAB allocator to fetch a new buffer
    iouring_userdata_new->buffer = (char *)xalloc_alloc(NETWORK_CHANNEL_PACKET_BUFFER_SIZE);

    if ((iouring_userdata_new->fd = worker_fds_map_add(
            ring,
            new_fd)) < 0) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Can't accept the connection <%s> coming from listener <%s>, no more fds free (should never happen, bug!)",
                iouring_userdata_new->address_str,
                iouring_userdata_current->address_str);

        // TODO: close client socket properly
        // TODO: implement ad hoc function to free the iouring_entry_user_data (when will implement the SLAB allocator
        //       that function will have to rely on it)
        xalloc_free(iouring_userdata_new->buffer);
        xalloc_free(iouring_userdata_new);

        return false;
    }

    if (io_uring_support_sqe_enqueue_recv(
            ring,
            new_fd,
            iouring_userdata_new->buffer,
            NETWORK_CHANNEL_PACKET_BUFFER_SIZE,
            0,
            0,
            (uintptr_t)iouring_userdata_new) == false) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Can't start to read data from the connection <%s> coming from listener <%s>",
                iouring_userdata_new->address_str,
                iouring_userdata_current->address_str);
        // TODO: implement ad hoc function to free the iouring_entry_user_data (when will implement the SLAB allocator
        //       that function will have to rely on it)

        network_io_common_socket_close(fds_map[iouring_userdata_new->fd], true);
        worker_fds_map_remove(
                ring,
                iouring_userdata_new->fd);

        xalloc_free(iouring_userdata_new->buffer);
        xalloc_free(iouring_userdata_new);

        return false;
    }

    stats->network.active_connections++;
    stats->network.accepted_connections_total++;
    stats->network.accepted_connections_per_second++;

    return true;
}

bool worker_iouring_process_op_recv(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe) {
    network_channel_iouring_entry_user_data_t *iouring_userdata_current;
    iouring_userdata_current = (network_channel_iouring_entry_user_data_t *)cqe->user_data;

    if (cqe->res <= 0) {
        if (cqe->res == 0) {
            LOG_V(LOG_PRODUCER_DEFAULT, "Closing client <%s>", iouring_userdata_current->address_str);
        } else{
            LOG_E(
                    LOG_PRODUCER_DEFAULT,
                    "Error <%s (%d)>, closing client <%s>",
                    strerror(cqe->res),
                    cqe->res,
                    iouring_userdata_current->address_str);
        }

        stats->network.active_connections--;
        network_io_common_socket_close(fds_map[iouring_userdata_current->fd], true);
        worker_fds_map_remove(
                ring,
                iouring_userdata_current->fd);

        xalloc_free(iouring_userdata_current->buffer);
        xalloc_free(iouring_userdata_current);

        return true;
    }

    int len = snprintf(iouring_userdata_current->buffer, NETWORK_CHANNEL_PACKET_BUFFER_SIZE, "+PONG\r\n");

    iouring_userdata_current->op = NETWORK_IO_IOURING_OP_SEND;

    // TODO: handle errors (ie. close the connection)
    io_uring_support_sqe_enqueue_send(
            ring,
            iouring_userdata_current->fd,
            iouring_userdata_current->buffer,
//            cqe->res,
            len,
            0,
            IOSQE_FIXED_FILE,
            (uintptr_t)iouring_userdata_current);

    stats->network.received_packets_total++;
    stats->network.received_packets_per_second++;
    stats->network.max_packet_size =
            max(stats->network.max_packet_size, cqe->res);

    return true;
}

bool worker_iouring_process_op_send(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe) {
    network_channel_iouring_entry_user_data_t *iouring_userdata_current;
    iouring_userdata_current = (network_channel_iouring_entry_user_data_t *)cqe->user_data;

    if (worker_iouring_cqe_is_error_any(cqe)) {
        worker_iouring_cqe_log(worker_user_data, cqe);

        network_io_common_socket_close(fds_map[iouring_userdata_current->fd], true);
        worker_fds_map_remove(
                ring,
                iouring_userdata_current->fd);

        xalloc_free(iouring_userdata_current->buffer);
        xalloc_free(iouring_userdata_current);

        return true;
    }

    iouring_userdata_current->op = NETWORK_IO_IOURING_OP_RECV;

    // TODO: handle errors (ie. close the connection)
    io_uring_support_sqe_enqueue_recv(
            ring,
            iouring_userdata_current->fd,
            iouring_userdata_current->buffer,
            NETWORK_CHANNEL_PACKET_BUFFER_SIZE,
            0,
            IOSQE_FIXED_FILE,
            (uintptr_t)iouring_userdata_current);

    stats->network.sent_packets_total++;
    stats->network.sent_packets_per_second++;

    return true;
}

void worker_iouring_thread_process_ops_loop(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t *ring
        ) {
    network_channel_iouring_entry_user_data_t *iouring_userdata_current, *iouring_userdata_timeout;

    worker_iouring_process_op_timeout_ensure_loop(
            worker_user_data,
            stats,
            ring,
            NULL);

    do {
        io_uring_cqe_t *cqe;
        uint32_t head, count = 0;

        io_uring_support_sqe_submit_and_wait(ring, 1);

        io_uring_for_each_cqe(ring, head, cqe) {
            count++;

            if (worker_iouring_cqe_is_error(cqe)) {
                worker_iouring_cqe_log(worker_user_data, cqe);
            }

            iouring_userdata_current = (network_channel_iouring_entry_user_data_t*)cqe->user_data;

            if (iouring_userdata_current->op == NETWORK_IO_IOURING_OP_NOP) {
                // do nothing
            } else if (iouring_userdata_current->op == NETWORK_IO_IOURING_OP_FILES_UPDATE) {
                // TODO: currently not in use because the async op is not behaving as expecting, keep getting operation
                //       cancelled -125
                worker_iouring_process_op_files_update(
                        worker_user_data,
                        stats,
                        ring,
                        cqe);
            } else if (iouring_userdata_current->op == WORKER_IOURING_OP_TIMEOUT_ENSURE_LOOP) {
                worker_iouring_process_op_timeout_ensure_loop(
                        worker_user_data,
                        stats,
                        ring,
                        cqe);
            } else if (iouring_userdata_current->op == NETWORK_IO_IOURING_OP_ACCEPT) {
                worker_iouring_process_op_accept(
                        worker_user_data,
                        stats,
                        ring,
                        cqe);
            } else if (iouring_userdata_current->op == NETWORK_IO_IOURING_OP_RECV) {
                worker_iouring_process_op_recv(
                        worker_user_data,
                        stats,
                        ring,
                        cqe);
            } else if (iouring_userdata_current->op == NETWORK_IO_IOURING_OP_SEND) {
                worker_iouring_process_op_send(
                        worker_user_data,
                        stats,
                        ring,
                        cqe);
            } else {
                LOG_W(
                        LOG_PRODUCER_DEFAULT,
                        "Unknown operation <%u> on <%s>, ignoring...",
                        iouring_userdata_current->op,
                        iouring_userdata_current->address_str);
            }
        }

        io_uring_support_cq_advance(ring, count);

        if (worker_should_publish_stats(&worker_user_data->stats)) {
            worker_publish_stats(
                    stats,
                    &worker_user_data->stats);
        }

    } while(!worker_should_terminate(worker_user_data));
}

void worker_iouring_cleanup(
        network_channel_listener_new_callback_user_data_t *listener_new_cb_user_data,
        io_uring_t *ring) {
    for(
            uint32_t listener_index = 0;
            listener_index < listener_new_cb_user_data->listeners_count;
            listener_index++) {
        network_io_common_socket_close(
                listener_new_cb_user_data->listeners[listener_index].fd,
                false);
    }

    // Unregister the files for SQPOLL
    io_uring_unregister_files(ring);
    xalloc_free(fds_map);
    xalloc_free((void*)fds_map_registered);
    io_uring_support_free(ring);
}

void* worker_iouring_thread_func(
        void* user_data) {
    io_uring_t *ring;
    worker_stats_t stats = {0};
    network_channel_listener_new_callback_user_data_t listener_new_cb_user_data = {0};
    worker_user_data_t *worker_user_data = user_data;

    // Initialize the log producer for the worker and set the thread prefix
    log_producer_local_init_worker_iouring();
    char* log_producer_early_prefix_thread = worker_log_producer_set_early_prefix_thread(worker_user_data);

    LOG_V(LOG_PRODUCER_DEFAULT, "Worker starting");

    // TODO: The affinity has to be set by the caller
    worker_user_data->core_index = worker_thread_set_affinity(worker_user_data->worker_index);

    // TODO: The listeners have to be initialized by the caller, each listener has to be paired up with a protocol parser
    //       and a protocol state machine and this code has to be shared across the different kind of workers (io_uring,
    //       epoll, iocp, kqueue/kevent, dpdk, etc.)
    LOG_V(LOG_PRODUCER_DEFAULT, "Initializing listeners");
    listener_new_cb_user_data.backlog = worker_user_data->backlog;
    listener_new_cb_user_data.core_index = worker_user_data->core_index;
    worker_iouring_network_listeners_initialize(
            worker_user_data,
            &listener_new_cb_user_data);

    // Reports the available features in io_uring
    if (worker_user_data->worker_index == 0) {
        char available_features_str[512] = {0};
        LOG_V(
                LOG_PRODUCER_DEFAULT,
                "io_uring available features: <%s>",
                io_uring_support_features_str(available_features_str, sizeof(available_features_str)));
    }

    LOG_V(LOG_PRODUCER_DEFAULT, "Initializing local ring for io_uring");

    ring = worker_iouring_initialize_iouring(
            worker_user_data->core_index,
            worker_user_data->max_connections,
            worker_user_data->addresses_count);

    if (ring != NULL) {
        LOG_I(LOG_PRODUCER_DEFAULT, "Checking if io_uring can link file updates ops");
        if ((op_files_update_link_support = io_uring_capabilities_is_linked_op_files_update_supported())) {
            LOG_I(LOG_PRODUCER_DEFAULT, "> linking supported");
        } else {
            LOG_W(LOG_PRODUCER_DEFAULT, "> linking not supported, accepting new connections will incur in a performance penalty");
        }

        LOG_I(LOG_PRODUCER_DEFAULT, "Enqueing listeners");

        worker_iouring_listeners_enqueue(
                ring,
                &listener_new_cb_user_data);

        LOG_V(LOG_PRODUCER_DEFAULT, "Starting process ops loop");

        worker_iouring_thread_process_ops_loop(
                worker_user_data,
                &stats,
                ring);

        LOG_V(LOG_PRODUCER_DEFAULT, "Process ops loop ended, cleaning up worker");

        worker_iouring_cleanup(
                &listener_new_cb_user_data,
                ring);

        LOG_V(LOG_PRODUCER_DEFAULT, "Tearing down worker");
    }

    log_producer_local_free_worker_iouring();
    xalloc_free(log_producer_early_prefix_thread);

    return NULL;
}
