#ifndef CACHEGRAND_WORKER_IOURING_OP_H
#define CACHEGRAND_WORKER_IOURING_OP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct worker_iouring_context worker_iouring_context_t;
typedef struct worker_iouring_op_context worker_iouring_op_context_t;

typedef bool (worker_iouring_op_wrapper_completion_cb_fp_t)(
        worker_iouring_context_t* worker_iouring_context,
        worker_iouring_op_context_t* worker_iouring_op_context,
        io_uring_cqe_t *cqe,
        bool *free_op_context);

struct worker_iouring_op_context {
    struct {
        struct {
            worker_op_timer_completion_cb_fp_t* timer;
            worker_op_network_error_completion_cb_fp_t* network_error;
            worker_op_network_accept_completion_cb_fp_t* network_accept;
            worker_op_network_receive_completion_cb_fp_t* network_receive;
            worker_op_network_send_completion_cb_fp_t* network_send;
            worker_op_network_close_completion_cb_fp_t* network_close;
        } completion_cb;
        void* data;
    } user;
    struct {
        worker_iouring_op_wrapper_completion_cb_fp_t* completion_cb;

        union {
            struct {
                struct __kernel_timespec ts;
            } timeout;
            struct {
                network_channel_iouring_t *channel;
            } files_update;
            struct {
                network_channel_iouring_t *listener_channel;
                network_channel_iouring_t *new_channel;
            } network_accept;
            struct {
                network_channel_iouring_t *channel;
                char temp_receive_buffer[8];
            } network_close;
            struct {
                network_channel_iouring_t *channel;
                char* buffer;
                size_t buffer_length;
            } network_receive;
            struct {
                network_channel_iouring_t *channel;
                char* buffer;
                size_t buffer_length;
            } network_send;
        };
    } io_uring;
};

worker_iouring_op_context_t* worker_iouring_op_context_init(
        worker_iouring_op_wrapper_completion_cb_fp_t* completion_cb);

bool worker_iouring_op_timer(
        worker_iouring_context_t *context,
        worker_op_timer_completion_cb_fp_t *completion_cb,
        long seconds,
        long long nanoseconds,
        void* user_data);
bool worker_iouring_op_timer_wrapper(
        worker_op_timer_completion_cb_fp_t *cb,
        long seconds,
        long long nanoseconds,
        void* user_data);

void worker_iouring_op_register();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_IOURING_OP_H
