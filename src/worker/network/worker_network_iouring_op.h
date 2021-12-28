#ifndef CACHEGRAND_WORKER_NETWORK_IOURING_OP_H
#define CACHEGRAND_WORKER_NETWORK_IOURING_OP_H

#ifdef __cplusplus
extern "C" {
#endif

network_channel_t* worker_network_iouring_op_network_accept(
        network_channel_t *listener_channel);

void worker_network_iouring_op_register();

bool worker_network_iouring_initialize(
        worker_context_t *worker_context);

void worker_network_iouring_listeners_listen_pre(
        worker_context_t *worker_context);

void worker_network_iouring_cleanup(
        worker_context_t *worker_context);

network_channel_t* worker_network_iouring_network_channel_new();

network_channel_t* worker_network_iouring_network_channel_multi_new(
        int count);

void worker_network_iouring_network_channel_free(
        network_channel_t* channel);

network_channel_t* worker_network_iouring_network_channel_multi_get(
        network_channel_t* channels,
        int index);

size_t worker_network_iouring_op_network_channel_size();

void worker_network_iouring_op_register();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_IOURING_OP_H
