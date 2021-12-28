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
        network_channel_t *channel);

bool worker_network_send(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length);

bool worker_network_close(
        network_channel_t *channel);

bool worker_network_protocol_process_events(
        network_channel_t *channel,
        worker_network_channel_user_data_t *worker_network_channel_user_data);

void worker_network_close_connection_on_send(
        network_channel_t *channel,
        bool close_connection_on_send);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_H
