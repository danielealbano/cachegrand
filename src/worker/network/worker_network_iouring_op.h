#ifndef CACHEGRAND_WORKER_NETWORK_IOURING_OP_H
#define CACHEGRAND_WORKER_NETWORK_IOURING_OP_H

#ifdef __cplusplus
extern "C" {
#endif

void worker_network_iouring_op_network_post_close(
        network_channel_iouring_t *channel);

network_channel_t* worker_network_iouring_op_network_accept_setup_new_channel(
        worker_iouring_context_t *context,
        network_channel_iouring_t *listener_channel,
        network_channel_iouring_t *new_channel,
        io_uring_cqe_t *cqe);

network_channel_t* worker_network_iouring_op_network_accept(
        network_channel_t *listener_channel);

bool worker_network_iouring_op_network_close(
        network_channel_t *channel,
        bool shutdown_may_fail);

size_t worker_network_iouring_op_network_receive(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length);

size_t worker_network_iouring_op_network_send(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length);

bool worker_network_iouring_initialize(
        worker_context_t *worker_context);

void worker_network_iouring_listeners_listen_pre(
        worker_context_t *worker_context);

void worker_network_iouring_cleanup(
        worker_context_t *worker_context);

network_channel_t* worker_network_iouring_network_channel_new();

void worker_network_iouring_network_channel_free(
        network_channel_t* channel);

network_channel_t* worker_network_iouring_network_channel_multi_new(
        uint32_t count);

network_channel_t* worker_network_iouring_network_channel_multi_get(
        network_channel_t* channels,
        uint32_t index);

size_t worker_network_iouring_op_network_channel_size();

void worker_network_iouring_op_register();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_IOURING_OP_H
