#ifndef CACHEGRAND_PROGRAM_H
#define CACHEGRAND_PROGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#define PROGRAM_NETWORK_MAX_CONNECTIONS_PER_WORKER  512
#define PROGRAM_NETWORK_CONNECTIONS_BACKLOG         100
#define PROGRAM_NETWORK_ADDRESSES \
        { "0.0.0.0", 12345 }, \
        { "::", 12345 }
#define PROGRAM_NETWORK_ADDRESSES_COUNT \
        (sizeof(((network_channel_address_t[]){ PROGRAM_NETWORK_ADDRESSES })) / sizeof(network_channel_address_t))

void program_signal_handlers(
        int sig);
void program_register_signal_handlers();

worker_user_data_t* program_workers_initialize(
        volatile bool *terminate_event_loop,
        pthread_attr_t *attr,
        uint32_t workers_count);

void program_request_terminate(
        volatile bool *terminate_event_loop);
bool program_should_terminate(
        volatile bool *terminate_event_loop);
void program_wait_loop(
        volatile bool *terminate_event_loop);

void program_workers_cleanup(
        worker_user_data_t* workers_user_data,
        uint32_t workers_count);
int program_main();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_PROGRAM_H
