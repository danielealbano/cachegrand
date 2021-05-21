#ifndef CACHEGRAND_WORKER_NETWORK_H
#define CACHEGRAND_WORKER_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

void worker_network_listeners_initialize(
        worker_context_t *worker_context);

void worker_network_listeners_listen(
        worker_context_t *worker_context);

bool worker_network_receive(
        network_channel_t *channel,
        void* user_data);

bool worker_network_send(
        network_channel_t *channel,
        network_channel_buffer_t *buffer,
        size_t buffer_length,
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_H
