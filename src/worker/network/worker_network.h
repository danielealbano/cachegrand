#ifndef CACHEGRAND_WORKER_NETWORK_H
#define CACHEGRAND_WORKER_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

enum network_op_result {
    NETWORK_OP_RESULT_OK,
    NETWORK_OP_RESULT_CLOSE_SOCKET,
    NETWORK_OP_RESULT_ERROR,
};
typedef enum network_op_result network_op_result_t;

void worker_network_listeners_initialize(
        worker_context_t *worker_context);

void worker_network_listeners_listen(
        worker_context_t *worker_context);

network_op_result_t worker_network_receive(
        network_channel_t *channel);

network_op_result_t worker_network_send(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length);

network_op_result_t worker_network_close(
        network_channel_t *channel);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_H
