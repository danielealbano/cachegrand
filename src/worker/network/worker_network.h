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
        uint8_t core_index,
        config_network_t *config_network,
        network_channel_t **listeners,
        uint8_t *listeners_count);

void worker_network_listeners_listen(
        network_channel_t *listeners,
        uint8_t listeners_count);


bool worker_network_buffer_has_enough_space(
        network_channel_buffer_t *read_buffer,
        size_t read_length);

bool worker_network_buffer_needs_rewind(
        network_channel_buffer_t *read_buffer,
        size_t read_length);

void worker_network_buffer_rewind(
        network_channel_buffer_t *read_buffer);

network_op_result_t worker_network_receive(
        network_channel_t *channel,
        network_channel_buffer_t *read_buffer,
        size_t read_length);

network_op_result_t worker_network_send(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length);

network_op_result_t worker_network_close(
        network_channel_t *channel,
        bool shutdown_may_fail);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_H
