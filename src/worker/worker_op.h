#ifndef CACHEGRAND_WORKER_OP_H
#define CACHEGRAND_WORKER_OP_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_TIMER_LOOP_MS 500l

typedef bool (worker_op_timer_fp_t)(
        long seconds,
        long long nanoseconds);

typedef network_channel_t* (worker_op_network_channel_new_fp_t)();

typedef network_channel_t* (worker_op_network_channel_multi_new_fp_t)(
        uint32_t count);

typedef network_channel_t* (worker_op_network_channel_multi_get_fp_t)(
        network_channel_t* channels,
        uint32_t index);

typedef void (worker_op_network_channel_free_fp_t)(
    network_channel_t *network_channel);

typedef network_channel_t* (worker_op_network_accept_fp_t)(
        network_channel_t *listener_channel);

typedef bool (worker_op_network_close_fp_t)(
        network_channel_t *channel,
        bool shutdown_may_fail);

typedef size_t (worker_op_network_receive_fp_t)(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length);

typedef size_t (worker_op_network_send_fp_t)(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length);

typedef size_t (worker_op_network_channel_size_fp_t)();

typedef storage_channel_t* (worker_op_storage_open_fp_t)(
        char *path,
        storage_io_common_open_flags_t flags,
        storage_io_common_open_mode_t mode);

typedef size_t (worker_op_storage_read_fp_t)(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset);

typedef size_t (worker_op_storage_write_fp_t)(
        storage_channel_t *channel,
        storage_io_common_iovec_t *iov,
        size_t iov_nr,
        off_t offset);

typedef bool (worker_op_storage_flush_fp_t)(
        storage_channel_t *channel);

typedef bool (worker_op_storage_close_fp_t)(
        storage_channel_t *channel);

void worker_timer_fiber_entrypoint(
        void *user_data);

void worker_timer_setup(
        worker_context_t* worker_context);

extern worker_op_timer_fp_t* worker_op_timer;

// Network operations
extern worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
extern worker_op_network_channel_multi_new_fp_t* worker_op_network_channel_multi_new;
extern worker_op_network_channel_multi_get_fp_t* worker_op_network_channel_multi_get;
extern worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
extern worker_op_network_accept_fp_t* worker_op_network_accept;
extern worker_op_network_receive_fp_t* worker_op_network_receive;
extern worker_op_network_send_fp_t* worker_op_network_send;
extern worker_op_network_close_fp_t* worker_op_network_close;
extern worker_op_network_channel_size_fp_t* worker_op_network_channel_size;

// File operations
extern worker_op_storage_open_fp_t* worker_op_storage_open;
extern worker_op_storage_read_fp_t* worker_op_storage_read;
extern worker_op_storage_write_fp_t* worker_op_storage_write;
extern worker_op_storage_flush_fp_t* worker_op_storage_flush;
extern worker_op_storage_close_fp_t* worker_op_storage_close;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_OP_H
