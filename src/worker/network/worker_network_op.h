#ifndef CACHEGRAND_WORKER_NETWORK_OP_H
#define CACHEGRAND_WORKER_NETWORK_OP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct worker_module_context worker_module_context_t;
struct worker_module_context {
    void *network_tls_config;
};

typedef network_channel_t* (worker_op_network_channel_new_fp_t)(
        network_channel_type_t type);

typedef network_channel_t* (worker_op_network_channel_multi_new_fp_t)(
        network_channel_type_t type,
        uint32_t count);

typedef network_channel_t* (worker_op_network_channel_multi_get_fp_t)(
        network_channel_t* channels,
        uint32_t index);

typedef void (worker_op_network_channel_free_fp_t)(
        network_channel_t *network_channel);

typedef network_channel_t* (worker_op_network_accept_fp_t)(
        network_channel_t *listener_channel);

typedef bool (worker_op_network_close_fp_t)(
        network_channel_t *channel,
        bool shutdown_may_fail);

typedef int32_t (worker_op_network_receive_fp_t)(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length);

typedef int32_t (worker_op_network_send_fp_t)(
        network_channel_t *channel,
        char* buffer,
        size_t buffer_length);

typedef size_t (worker_op_network_channel_size_fp_t)();

worker_module_context_t *worker_module_contexts_initialize(
        config_t *config);

void worker_module_context_free(
        config_t *config,
        worker_module_context_t *worker_module_context);

bool worker_network_listeners_initialize(
        uint32_t worker_index,
        uint8_t core_index,
        config_t *config,
        worker_module_context_t *worker_module_context,
        network_channel_t **listeners,
        uint8_t *listeners_count);

void worker_network_listeners_listen(
        fiber_t **listeners_fibers,
        network_channel_t *listeners,
        uint8_t listeners_count);

void worker_network_listeners_fiber_entrypoint(
        void *user_data);

// Network operations
extern worker_op_network_channel_new_fp_t* worker_op_network_channel_new;
extern worker_op_network_channel_multi_new_fp_t* worker_op_network_channel_multi_new;
extern worker_op_network_channel_multi_get_fp_t* worker_op_network_channel_multi_get;
extern worker_op_network_channel_free_fp_t* worker_op_network_channel_free;
extern worker_op_network_accept_fp_t* worker_op_network_accept;
extern worker_op_network_receive_fp_t* worker_op_network_receive;
extern worker_op_network_send_fp_t* worker_op_network_send;
extern worker_op_network_close_fp_t* worker_op_network_close;
extern worker_op_network_channel_size_fp_t* worker_op_network_channel_size;

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_OP_H
