#ifndef CACHEGRAND_WORKER_NETWORK_OP_H
#define CACHEGRAND_WORKER_NETWORK_OP_H

#ifdef __cplusplus
extern "C" {
#endif


bool worker_network_op_completion_cb_network_error_client(
        network_channel_t *channel,
        int error_number,
        char* error_message,
        void* user_data);

bool worker_network_op_completion_cb_network_error_listener(
        network_channel_t *channel,
        int error_number,
        char* error_message,
        void* user_data);

bool worker_network_op_completion_cb_network_accept(
        network_channel_t *listener_channel,
        network_channel_t *new_channel,
        void *user_data);

bool worker_network_op_completion_cb_network_receive(
        network_channel_t *channel,
        size_t receive_length,
        void* user_data);

bool worker_network_op_completion_cb_network_send(
        network_channel_t *channel,
        size_t send_length,
        void* user_data);

bool worker_network_op_completion_cb_network_close(
        network_channel_t *channel,
        void* user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_OP_H
