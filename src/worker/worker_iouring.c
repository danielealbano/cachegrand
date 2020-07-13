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
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "worker.h"

#include "worker_iouring.h"

LOG_PRODUCER_CREATE_THREAD_LOCAL_DEFAULT("worker_iouring", worker_iouring)

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
        uint32_t max_connections,
        uint32_t network_addresses_count) {
    io_uring_t *ring;
    io_uring_params_t params = {0};
    ring = io_uring_support_init(
            worker_iouring_calculate_entries(max_connections, network_addresses_count),
            &params, NULL);

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

    for(
            uint32_t listener_index = 0;
            listener_index < listener_new_cb_user_data->listeners_count;
            listener_index++) {
        network_channel_listener_t *listener = &listener_new_cb_user_data->listeners[listener_index];

        iouring_user_data = network_channel_iouring_entry_user_data_new_with_fd(
                NETWORK_IO_IOURING_OP_ACCEPT,
                listener->fd);
        iouring_user_data->address_size = listener->address_size;

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

        io_uring_support_sqe_enqueue_accept(
                ring,
                listener->fd,
                &iouring_user_data->new_socket_address.base,
                &iouring_user_data->address_size,
                0,
                (uintptr_t)iouring_user_data);
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

void worker_iouring_cqe_report_error(
        worker_user_data_t *worker_user_data,
        io_uring_cqe_t *cqe) {
    network_channel_iouring_entry_user_data_t *iouring_userdata;
    iouring_userdata = (network_channel_iouring_entry_user_data_t*)cqe->user_data;

    LOG_E(
            LOG_PRODUCER_DEFAULT,
            "[OP:%u][FD:%u] cqe->user_data = <0x%08lx>, cqe->res = <%s (%d)>, cqe->flags >> 16 = <%d>, cqe->flags >> 16 = <%d>",
            iouring_userdata->op,
            iouring_userdata->fd,
            cqe->user_data,
            strerror(cqe->res * -1),
            cqe->res,
            cqe->flags >> 16u,
            cqe->flags & 0xFFFFu);
}

bool worker_iouring_process_op_timeout(
        worker_user_data_t *worker_user_data,
        worker_stats_t* stats,
        io_uring_t* ring,
        io_uring_cqe_t *cqe) {
    network_channel_iouring_entry_user_data_t *iouring_userdata_current;

    if (cqe == NULL) {
        iouring_userdata_current = network_channel_iouring_entry_user_data_new(NETWORK_IO_IOURING_OP_TIMEOUT);
    } else {
        iouring_userdata_current = (network_channel_iouring_entry_user_data_t *)cqe->user_data;
    }

    return io_uring_support_sqe_enqueue_timeout(
            ring,
            1,
            1,
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
                "TODO");
        worker_request_terminate(worker_user_data);

        return false;
    }

    new_fd = cqe->res;

    iouring_userdata_new = network_channel_iouring_entry_user_data_new_with_fd(
            NETWORK_IO_IOURING_OP_RECV,
            new_fd);
    network_io_common_socket_address_str(
            &iouring_userdata_current->new_socket_address.base,
            (char *)&iouring_userdata_new->address_str,
            sizeof(iouring_userdata_new->address_str));

    if (io_uring_support_sqe_enqueue_accept(
            ring,
            iouring_userdata_current->fd,
            &iouring_userdata_current->new_socket_address.base,
            &iouring_userdata_current->address_size,
            0,
            (uintptr_t)iouring_userdata_current) == false) {
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
        // TODO: implement ad hoc function to free the iouring_entry_user_data (when will implement the SLAB allocator
        //       that function will have to rely on it)
        xalloc_free(iouring_userdata_new);

        return false;
    }

    stats->network.active_connections++;
    stats->network.accepted_connections_total++;
    stats->network.accepted_connections_per_second++;

    // Use SLAB allocator to fetch a new buffer
    iouring_userdata_new->buffer = (char *) xalloc_alloc(NETWORK_CHANNEL_PACKET_BUFFER_SIZE);

    LOG_V(
            LOG_PRODUCER_DEFAULT,
            "Listener <%s> accepting new connection from <%s>",
            iouring_userdata_current->address_str,
            iouring_userdata_new->address_str);

    return io_uring_support_sqe_enqueue_recv(
            ring,
            iouring_userdata_new->fd,
            iouring_userdata_new->buffer,
            NETWORK_CHANNEL_PACKET_BUFFER_SIZE,
            (uintptr_t)iouring_userdata_new);
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
        }

        stats->network.active_connections--;
        network_io_common_socket_close(iouring_userdata_current->fd, true);

        if (iouring_userdata_current->buffer) {
            xalloc_free(iouring_userdata_current->buffer);
        }
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
            len,
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
        network_io_common_socket_close(iouring_userdata_current->fd, true);
        return true;
    }

    iouring_userdata_current->op = NETWORK_IO_IOURING_OP_RECV;

    // TODO: handle errors (ie. close the connection)
    io_uring_support_sqe_enqueue_recv(
            ring,
            iouring_userdata_current->fd,
            iouring_userdata_current->buffer,
            NETWORK_CHANNEL_PACKET_BUFFER_SIZE,
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

    worker_iouring_process_op_timeout(
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
                worker_iouring_cqe_report_error(worker_user_data, cqe);
            }

            iouring_userdata_current = (network_channel_iouring_entry_user_data_t*)cqe->user_data;

            if (iouring_userdata_current->op == NETWORK_IO_IOURING_OP_TIMEOUT) {
                worker_iouring_process_op_timeout(
                        worker_user_data,
                        stats,
                        ring,
                        cqe);
                // do nothing
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
                LOG_E(
                        LOG_PRODUCER_DEFAULT,
                        "Unknown operation <%u> on <%s>",
                        iouring_userdata_current->op,
                        iouring_userdata_current->address_str);
                worker_request_terminate(worker_user_data);
                break;
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
            worker_user_data->max_connections,
            worker_user_data->addresses_count);

    LOG_V(LOG_PRODUCER_DEFAULT, "Enqueing listeners");

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

    log_producer_local_free_worker_iouring();
    xalloc_free(log_producer_early_prefix_thread);

    return NULL;
}
