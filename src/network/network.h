#ifndef CACHEGRAND_NETWORK_H
#define CACHEGRAND_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

enum network_op_result {
    NETWORK_OP_RESULT_OK,
    NETWORK_OP_RESULT_CLOSE_SOCKET,
    NETWORK_OP_RESULT_ERROR,
};
typedef enum network_op_result network_op_result_t;

bool network_buffer_has_enough_space(
        network_channel_buffer_t *read_buffer,
        size_t read_length);

bool network_buffer_needs_rewind(
        network_channel_buffer_t *read_buffer,
        size_t read_length);

void network_buffer_rewind(
        network_channel_buffer_t *read_buffer);

network_op_result_t network_receive(
        network_channel_t *channel,
        network_channel_buffer_t *buffer,
        size_t receive_length);

network_op_result_t network_receive_internal(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        size_t *read_length);

network_op_result_t network_send(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length);

network_op_result_t network_send_internal(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length,
        size_t *sent_length);

bool network_should_flush(
        network_channel_t *channel);

network_op_result_t network_flush(
        network_channel_t *channel);

network_op_result_t network_send_direct(
        network_channel_t *channel,
        network_channel_buffer_data_t *buffer,
        size_t buffer_length);

network_op_result_t network_close(
        network_channel_t *channel,
        bool shutdown_may_fail);

network_op_result_t network_close_internal(
        network_channel_t *channel,
        bool shutdown_may_fail);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_NETWORK_H
