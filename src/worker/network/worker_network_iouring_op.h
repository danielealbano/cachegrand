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
        struct sockaddr *addr,
        socklen_t addr_len,
        io_uring_cqe_t *cqe);

network_channel_t* worker_network_iouring_op_network_accept(
        network_channel_t *listener_channel);

bool worker_network_iouring_op_network_close(
        network_channel_t *channel,
        bool shutdown_may_fail);

int32_t worker_network_iouring_op_network_receive(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length);

int32_t worker_network_iouring_op_network_send(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length);

bool worker_network_iouring_initialize(
        __attribute__((unused)) worker_context_t *worker_context);

void worker_network_iouring_listeners_listen_pre(
        network_channel_t *listeners,
        uint8_t listeners_count);

bool worker_network_iouring_cleanup(
        __attribute__((unused)) __attribute__((unused)) network_channel_t *listeners,
        __attribute__((unused)) __attribute__((unused)) uint8_t listeners_count);

network_channel_t* worker_network_iouring_network_channel_new(
        network_channel_type_t type);

void worker_network_iouring_network_channel_free(
        network_channel_t* channel);

network_channel_t* worker_network_iouring_network_channel_multi_new(
        network_channel_type_t type,
        uint32_t count);

network_channel_t* worker_network_iouring_network_channel_multi_get(
        network_channel_t* channels,
        uint32_t index);

size_t worker_network_iouring_op_network_channel_size();

bool worker_network_iouring_op_register();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_IOURING_OP_H
