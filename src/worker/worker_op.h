#ifndef CACHEGRAND_WORKER_OP_H
#define CACHEGRAND_WORKER_OP_H

#ifdef __cplusplus
extern "C" {
#endif

#define WORKER_TIMER_LOOP_MS 250l

typedef bool (worker_op_timer_completion_cb_fp_t)(
        void* user_data);
typedef bool (worker_op_timer_fp_t)(
        worker_op_timer_completion_cb_fp_t* timer_completion_cb,
        long seconds,
        long long nanoseconds,
        void* user_data);

typedef network_channel_t* (worker_op_network_channel_new_fp_t)();

typedef network_channel_t* (worker_op_network_channel_multi_new_fp_t)(
        int count);

typedef network_channel_t* (worker_op_network_channel_multi_get_fp_t)(
        network_channel_t* channels,
        int index);

typedef void (worker_op_network_channel_free_fp_t)(
    network_channel_t *network_channel);

typedef bool (worker_op_network_error_completion_cb_fp_t)(
        network_channel_t *channel,
        int error_number,
        char* error_string,
        void* user_data);

typedef network_channel_t* (worker_op_network_accept_fp_t)(
        network_channel_t *listener_channel);

typedef bool (worker_op_network_close_completion_cb_fp_t)(
        network_channel_t *channel,
        void* user_data);
typedef bool (worker_op_network_close_fp_t)(
        worker_op_network_close_completion_cb_fp_t* network_close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* network_error_completion_cb,
        network_channel_t *channel,
        void* user_data);

typedef bool (worker_op_network_receive_completion_cb_fp_t)(
        network_channel_t *channel,
        size_t receive_length,
        void* user_data);
typedef bool (worker_op_network_receive_fp_t)(
        worker_op_network_receive_completion_cb_fp_t* network_receive_completion_cb,
        worker_op_network_close_completion_cb_fp_t* network_close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* network_error_completion_cb,
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length,
        void* user_data);

typedef bool (worker_op_network_send_completion_cb_fp_t)(
        network_channel_t *channel,
        size_t send_length,
        void* user_data);
typedef bool (worker_op_network_send_fp_t)(
        worker_op_network_send_completion_cb_fp_t* network_send_completion_cb,
        worker_op_network_close_completion_cb_fp_t* network_close_completion_cb,
        worker_op_network_error_completion_cb_fp_t* network_error_completion_cb,
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length,
        void* user_data);

typedef size_t (worker_op_network_channel_size_fp_t)();

extern worker_op_timer_fp_t* worker_op_timer;
extern worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
extern worker_op_network_channel_multi_new_fp_t* worker_op_network_channel_multi_new;
extern worker_op_network_channel_multi_get_fp_t* worker_op_network_channel_multi_get;
extern worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
extern worker_op_network_accept_fp_t* worker_op_network_accept;
extern worker_op_network_receive_fp_t* worker_op_network_receive;
extern worker_op_network_send_fp_t* worker_op_network_send;
extern worker_op_network_close_fp_t* worker_op_network_close;
extern worker_op_network_channel_size_fp_t* worker_op_network_channel_size;

bool worker_op_timer_completion_cb_loop(
        void* user_data);
bool worker_op_timer_loop_submit();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_OP_H
