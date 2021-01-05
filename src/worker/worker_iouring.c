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
#include "pow2.h"
#include "support/io_uring/io_uring_support.h"
#include "support/io_uring/io_uring_capabilities.h"
#include "network/io/network_io_common.h"
#include <network/protocol/network_protocol.h>
#include <protocols/resp3/protocol_resp3.h>
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "worker.h"

#include "worker_iouring.h"

#define WORKER_FDS_MAP_EMPTY 0

#define TAG "worker_iouring"

// TODO: this code is buggy, potentially the fd 0 is a valid fd and it would be considered as empty causing a lot of
//       headache during debugging and fixing, this code should use a bitmap in addition to keep track of the used
//       slots
static thread_local network_io_common_fd_t *fds_map_registered = NULL;
static thread_local network_io_common_fd_t *fds_map = NULL;
static thread_local uint32_t fds_map_count = 0;
static thread_local uint32_t fds_map_mask = 0;
static thread_local uint32_t fds_map_last_free = 0;

static thread_local bool op_files_update_link_support = false;

// TODO: All the mapped fds management should be moved into the support/io_uring/io_uring_support and should apply
//       in a transparent way the mapped fds when the IOSQE_FIXED_FILE is passed to the sqe

bool worker_fds_map_files_update(
        io_uring_t *ring,
        uint32_t index,
        network_io_common_fd_t fd) {

    bool ret;

    fds_map[index] = fd;

    if (likely(op_files_update_link_support)) {
        // TODO: use slab allocator
        network_channel_iouring_entry_user_data_t *iouring_userdata_new =
                network_channel_iouring_entry_user_data_new_with_mapped_fd(NETWORK_IO_IOURING_OP_FILES_UPDATE, index);

        io_uring_sqe_t *sqe = io_uring_support_get_sqe(ring);
        if (sqe != NULL) {
            // TODO: need to fix the implementation in support/io_uring/io_uring_support.c
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
        fds_map[index] = WORKER_FDS_MAP_EMPTY;

        LOG_E(
                TAG,
                "Failed to update the registered fd <%d> with index <%u> in the registered files", fd, index);
        LOG_E_OS_ERROR(TAG);
    }

    return ret;
}

int32_t worker_fds_map_find_free_index() {
    for(uint32_t i = 0; i < fds_map_count; i++) {
        uint32_t fds_map_index = (i + fds_map_last_free) & fds_map_mask;
        if (fds_map[fds_map_index] == WORKER_FDS_MAP_EMPTY) {
            return fds_map_index;
        }
    }

    LOG_E(
            TAG,
            "Unable to find a free slot for an fd in the fds map");

    return -1;
}

int32_t worker_fds_map_add_and_enqueue_files_update(
        io_uring_t *ring,
        network_io_common_fd_t fd) {
    int32_t index;

    if ((index = worker_fds_map_find_free_index()) < 0) {
        return WORKER_FDS_MAP_EMPTY;
    }

    LOG_D(
            TAG,
            "Registering fd <%d> with index <%d>", fd, index);

    if (!worker_fds_map_files_update(ring, index, fd)) {
        return -1;
    }

    fds_map_last_free = (index + 1) & fds_map_mask;

    return index;
}

network_io_common_fd_t worker_fds_map_remove(
        io_uring_t *ring,
        int index) {
    network_io_common_fd_t fd;

    fd = fds_map[index];
    fds_map[index] = WORKER_FDS_MAP_EMPTY;

    LOG_D(
            TAG,
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
        fds_map[fds_map_index] = WORKER_FDS_MAP_EMPTY;
        fds_map_registered[fds_map_index] = WORKER_FDS_MAP_EMPTY;
    }

    if (io_uring_register_files(ring, fds_map_registered, fds_map_count) < 0) {
        LOG_E(
                TAG,
                "Failed to register the fds_map with io_uring");
        LOG_E_OS_ERROR(TAG);

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
            LOG_E(TAG, "Unable to setup listener for <%s:%u>",
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
        if ((iouring_user_data = network_channel_iouring_entry_user_data_new(
                NETWORK_IO_IOURING_OP_ACCEPT)) == NULL) {
            // TODO: handle error properly
            continue;
        }

        // Import the listener channel as is
        iouring_user_data->channel = &listener_new_cb_user_data->listeners[listener_index];

        // Enforce the size of the new socket address to match the listener socket address, the listeners are going to
        // accept only connections of the same type (ipv4 -> ipv4 and ipv6 -> ipv6)
        iouring_user_data->listener_new_socket_address.size = iouring_user_data->channel->address.size;

        if ((iouring_user_data->mapped_fd = worker_fds_map_add_and_enqueue_files_update(
                ring,
                iouring_user_data->channel->fd)) < 0) {
            network_channel_iouring_entry_user_data_free(iouring_user_data);
            continue;
        }

        if (io_uring_support_sqe_enqueue_accept(
                ring,
                iouring_user_data->channel->fd,
                &iouring_user_data->listener_new_socket_address.socket.base,
                &iouring_user_data->listener_new_socket_address.size,
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
            TAG,
            "[OP:%u][FD IDX:%d][FD:%d] cqe->user_data = <0x%08lx>, cqe->res = <%s (%d)>, cqe->flags >> 16 = <%d>, cqe->flags >> 16 = <%d>",
            iouring_userdata->op,
            iouring_userdata->channel->fd,
            iouring_userdata->mapped_fd,
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
        fds_map[iouring_userdata_current->mapped_fd] = WORKER_FDS_MAP_EMPTY;
    }

    network_channel_iouring_entry_user_data_free(iouring_userdata_current);

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
                TAG,
                "Error while waiting for connections on listener <%s>",
                iouring_userdata_current->channel->address.str);
        worker_request_terminate(worker_user_data);

        return false;
    }

    new_fd = cqe->res;

    iouring_userdata_new = network_channel_iouring_entry_user_data_new(
            NETWORK_IO_IOURING_OP_RECV);
    iouring_userdata_new->channel = network_channel_new();
    iouring_userdata_new->channel->fd = new_fd;
    iouring_userdata_new->channel->protocol.type = iouring_userdata_current->channel->protocol.type;
    iouring_userdata_new->channel->type = NETWORK_CHANNEL_TYPE_CLIENT;

    LOG_V(
            TAG,
            "Listener <%s> accepting new connection from <%s>",
            iouring_userdata_current->address_str,
            iouring_userdata_new->address_str);
    // Import the address info from the data struct used by the listener, this doesn't have to be done before the
    // listener has to be enqueued again because the ring items are submitted only in the main loop of the worker but
    // it may easily cause plenty of headache in the future if it changes so better to do things in the proper sequence
    // and avoid having these kind of hidden hard dependencies
    memcpy(
            &iouring_userdata_new->channel->address,
            &iouring_userdata_current->listener_new_socket_address,
            sizeof(iouring_userdata_current->listener_new_socket_address));

    if (io_uring_support_sqe_enqueue_accept(
            ring,
            iouring_userdata_current->mapped_fd,
            &iouring_userdata_current->listener_new_socket_address.socket.base,
            &iouring_userdata_current->listener_new_socket_address.size,
            0,
            IOSQE_FIXED_FILE,
            (uintptr_t)iouring_userdata_current) == false) {
        LOG_E(
                TAG,
                "Can't start to listen again on listener <%s>",
                iouring_userdata_current->channel->address.str);

        network_channel_iouring_entry_user_data_free(iouring_userdata_new);

        // TODO: close client socket properly
        // TODO: close listener socket properly
        // TODO: remove the fd from the registered files
        // TODO: handle failure in a more appropriate way (ie. the world has ended, let other worker know)
        return false;
    }

    network_io_common_socket_address_str(
            &iouring_userdata_new->channel->address.socket.base,
            iouring_userdata_new->channel->address.str,
            sizeof(iouring_userdata_new->channel->address.str));

    LOG_V(
            LOG_PRODUCER_DEFAULT,
            "Listener <%s> accepting new connection from <%s>",
            iouring_userdata_current->channel->address.str,
            iouring_userdata_new->channel->address.str);

    // Because potentially network_channel_client_setup may fail, the address of new connection must be extracted and the
    // accept operation on the lister must be enqueue-ed. If the operation fails the newly created iouring_userdata_new
    // will be freed and the operation marked as failed.
    // Currently the main loop of the worker is ignoring the return value because there isn't anything really that
    // should be done apart continuing the execution and freeing up any allocated memory.
    if (network_channel_client_setup(
            new_fd,
            worker_user_data->core_index) == false) {
        LOG_E(
                TAG,
                "Can't accept the connection <%s> coming from listener <%s>",
                iouring_userdata_new->channel->address.str,
                iouring_userdata_current->listener_new_socket_address.str);

        // TODO: close client socket properly
        network_channel_iouring_entry_user_data_free(iouring_userdata_new);

        return false;
    }

    // TODO: Use SLAB allocator to fetch a new buffer
    iouring_userdata_new->recv_buffer.data = (char *)xalloc_alloc(NETWORK_CHANNEL_RECV_BUFFER_SIZE);
    iouring_userdata_new->send_buffer.data = (char *)xalloc_alloc(NETWORK_CHANNEL_SEND_BUFFER_SIZE);

    if ((iouring_userdata_new->mapped_fd = worker_fds_map_add_and_enqueue_files_update(
            ring,
            new_fd)) < 0) {
        LOG_E(
                TAG,
                "Can't accept the connection <%s> coming from listener <%s>, no more fds free (should never happen, bug!)",
                iouring_userdata_new->channel->address.str,
                iouring_userdata_current->listener_new_socket_address.str);

        // TODO: close client socket properly
        network_channel_iouring_entry_user_data_free(iouring_userdata_new);

        return false;
    }

    if (io_uring_support_sqe_enqueue_recv(
            ring,
            new_fd,
            iouring_userdata_new->recv_buffer.data,
            NETWORK_CHANNEL_PACKET_SIZE,
            0,
            0,
            (uintptr_t)iouring_userdata_new) == false) {
        LOG_E(
                TAG,
                "Can't start to read data from the connection <%s> coming from listener <%s>",
                iouring_userdata_new->channel->address.str,
                iouring_userdata_current->listener_new_socket_address.str);
        network_io_common_socket_close(fds_map[iouring_userdata_new->mapped_fd], true);
        worker_fds_map_remove(
                ring,
                iouring_userdata_new->mapped_fd);

        network_channel_iouring_entry_user_data_free(iouring_userdata_current);

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
            LOG_V(TAG, "Closing client <%s>", iouring_userdata_current->address_str);
        } else{
            LOG_E(
                    TAG,
                    "Error <%s (%d)>, closing client <%s>",
                    strerror(cqe->res * -1),
                    cqe->res,
                    iouring_userdata_current->channel->address.str);
        }

        stats->network.active_connections--;
        network_io_common_socket_close(fds_map[iouring_userdata_current->mapped_fd], true);
        worker_fds_map_remove(
                ring,
                iouring_userdata_current->mapped_fd);

        network_channel_iouring_entry_user_data_free(iouring_userdata_current);

        return true;
    }

    iouring_userdata_current->recv_buffer.size += cqe->res;

    char* read_buffer_with_offset =
            (char*)((uintptr_t)iouring_userdata_current->recv_buffer.data + iouring_userdata_current->recv_buffer.offset);














    read_buffer_with_offset[cqe->res] = 0;
    LOG_D(LOG_PRODUCER_DEFAULT, "[RECV] iouring_userdata_current->recv_buffer.offset = %lu", iouring_userdata_current->recv_buffer.offset);
    LOG_D(LOG_PRODUCER_DEFAULT, "%s", read_buffer_with_offset);

    int len = snprintf(iouring_userdata_current->recv_buffer.data, NETWORK_CHANNEL_PACKET_SIZE, "-ERR: you are unlucky!\r\n");

    iouring_userdata_current->op = NETWORK_IO_IOURING_OP_SEND;

    // TODO: handle errors (ie. close the connection)
    io_uring_support_sqe_enqueue_send(
            ring,
            iouring_userdata_current->mapped_fd,
            iouring_userdata_current->recv_buffer.data,
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

        network_io_common_socket_close(fds_map[iouring_userdata_current->mapped_fd], true);
        worker_fds_map_remove(
                ring,
                iouring_userdata_current->mapped_fd);

        network_channel_iouring_entry_user_data_free(iouring_userdata_current);

        return true;
    }

    iouring_userdata_current->op = NETWORK_IO_IOURING_OP_RECV;

    iouring_userdata_current->recv_buffer.offset += cqe->res;
    if ((iouring_userdata_current->recv_buffer.offset + NETWORK_CHANNEL_PACKET_SIZE) > NETWORK_CHANNEL_RECV_BUFFER_SIZE) {
        iouring_userdata_current->recv_buffer.offset = 0;
    }

    LOG_D(LOG_PRODUCER_DEFAULT, "[SEND] iouring_userdata_current->recv_buffer.offset = %lu", iouring_userdata_current->recv_buffer.offset);

    char* read_buffer_with_offset =
            (char*)((uintptr_t)iouring_userdata_current->recv_buffer.data + iouring_userdata_current->recv_buffer.offset);

    // TODO: handle errors (ie. close the connection)
    io_uring_support_sqe_enqueue_recv(
            ring,
            iouring_userdata_current->mapped_fd,
            read_buffer_with_offset,
            NETWORK_CHANNEL_PACKET_SIZE,
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
                        TAG,
                        "Unknown operation <%u> on <%s>, ignoring...",
                        iouring_userdata_current->op,
                        iouring_userdata_current->channel->address.str);
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

    //Set the thread prefix to be used in the logs
    char* log_producer_early_prefix_thread = worker_log_producer_set_early_prefix_thread(worker_user_data);

    LOG_I(TAG, "Worker starting");

    // TODO: The affinity has to be set by the caller
    worker_user_data->core_index = worker_thread_set_affinity(worker_user_data->worker_index);

    // TODO: The listeners have to be initialized by the caller, each listener has to be paired up with a protocol parser
    //       and a protocol state machine and this code has to be shared across the different kind of workers (io_uring,
    //       epoll, iocp, kqueue/kevent, dpdk, etc.)
    LOG_I(TAG, "Initializing listeners");
    listener_new_cb_user_data.backlog = worker_user_data->backlog;
    listener_new_cb_user_data.core_index = worker_user_data->core_index;
    worker_iouring_network_listeners_initialize(
            worker_user_data,
            &listener_new_cb_user_data);

    // Reports the available features in io_uring
    if (worker_user_data->worker_index == 0) {
        char available_features_str[512] = {0};
        LOG_V(
                TAG,
                "io_uring available features: <%s>",
                io_uring_support_features_str(available_features_str, sizeof(available_features_str)));
    }

    LOG_V(TAG, "Initializing local ring for io_uring");

    ring = worker_iouring_initialize_iouring(
            worker_user_data->core_index,
            worker_user_data->max_connections,
            worker_user_data->addresses_count);

    if (ring != NULL) {
        LOG_I(TAG, "Checking if io_uring can link file updates ops");
        if ((op_files_update_link_support = io_uring_capabilities_is_linked_op_files_update_supported())) {
            LOG_I(TAG, "> linking supported");
        } else {
            LOG_W(TAG, "> linking not supported, accepting new connections will incur in a performance penalty");
        }

        LOG_I(TAG, "Enqueing listeners");

        worker_iouring_listeners_enqueue(
                ring,
                &listener_new_cb_user_data);

        LOG_I(TAG, "Starting events process loop");

        worker_iouring_thread_process_ops_loop(
                worker_user_data,
                &stats,
                ring);

        LOG_I(TAG, "Process events loop ended, cleaning up worker");

        worker_iouring_cleanup(
                &listener_new_cb_user_data,
                ring);

        LOG_I(TAG, "Tearing down worker");
    }

    xalloc_free(log_producer_early_prefix_thread);

    // TODO: the various slab allocated memory can be freed ONLY after every thing else has been terminated, especially
    //       the io_uring ring otherwise the kernel may try to access memory in the userland that is going to contain
    //       garbage and that's very bad

    return NULL;
}
