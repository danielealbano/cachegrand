#ifndef CACHEGRAND_WORKER_NETWORK_OP_H
#define CACHEGRAND_WORKER_NETWORK_OP_H

#ifdef __cplusplus
extern "C" {
#endif

void worker_network_post_network_channel_close(
        worker_context_t *context,
        network_channel_t *channel,
        void* user_data);

void worker_network_listeners_fiber_entrypoint(
        void *user_data);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_WORKER_NETWORK_OP_H
