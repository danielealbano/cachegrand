#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <liburing.h>

#include "cpu.h"
#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "log.h"
#include "io_uring_support.h"
#include "io_uring_supported.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "worker/worker.h"
#include "worker/worker_iouring.h"

LOG_PRODUCER_CREATE_DEFAULT("main", main)

network_channel_address_t addresses[] = {
        { "0.0.0.0", 12345 },
        { "::", 12345 }
};
uint32_t addresses_count = sizeof(addresses) / sizeof(network_channel_address_t);

worker_user_data_t* program_workers_initialize(
        volatile bool *terminate_event_loop,
        pthread_attr_t *attr,
        uint32_t workers_count) {
    int res;
    worker_user_data_t *workers_user_data;

    // TODO: implement a thread manager
    res = pthread_attr_init(attr);
    if (res != 0) {
        LOG_E(LOG_PRODUCER_DEFAULT, "Unable to start initialize the thread attributes");
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
        return NULL;
    }

    workers_user_data = xalloc_alloc_zero(sizeof(worker_user_data_t) * workers_count);

    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        worker_user_data_t *worker_user_data = &workers_user_data[worker_index];

        worker_setup_user_data(
                worker_user_data,
                worker_index,
                terminate_event_loop,
                512,
                100,
                addresses,
                addresses_count);

        LOG_V(LOG_PRODUCER_DEFAULT, "Creating worker <%u>", worker_index);

        // TODO: decide dynamically which kind of worker should start
        if (pthread_create(
                &worker_user_data->pthread,
                attr,
                worker_iouring_thread_func,
                worker_user_data) != 0) {
            LOG_E(LOG_PRODUCER_DEFAULT, "Unable to start the worker <%u>", worker_index);
            LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
        }
    }

    return workers_user_data;
}

void program_busy_loop(
        volatile bool *terminate_event_loop) {
    // Wait for the software to terminate
    do {
        sleep(1);
    } while(!*terminate_event_loop);
}

void program_workers_cleanup(
        worker_user_data_t* workers_user_data,
        uint32_t workers_count) {
    int res;
    int ret;

    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        res = pthread_join(workers_user_data[worker_index].pthread, (void**)&ret);
        if (res != 0) {
            LOG_E(LOG_PRODUCER_DEFAULT, "Unable to join the thread <%u>", worker_index);
            LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
        }
        LOG_V(LOG_PRODUCER_DEFAULT, "Worker <%u> ret <%u>", worker_index, ret);
    }
}

int program_main() {
    uint32_t workers_count;
    worker_user_data_t* workers_user_data;
    pthread_attr_t attr = {0};
    volatile bool terminate_event_loop = false;

    // TODO: load the config
    // TODO: initialize the log sinks
    // TODO: initialize the hashtable(s) (with or without LRU, when the support will be added)
    // TODO: initialize the storage
    // TODO: initialize the protocol parsers
    // TODO: initialize the network listeners and the protocol state machines
    // TODO: start the worker threads and invoke the worker thread main func

    if (io_uring_supported() == false) {
        LOG_E(LOG_PRODUCER_DEFAULT, "io_uring isn't supported, update the kernel to at least version 5.8.0 and enable io_uring");
        return 1;
    }

    workers_count = psnip_cpu_count();

    if ((workers_user_data = program_workers_initialize(
            &terminate_event_loop,
            &attr,
            workers_count)) == NULL) {
        return 1;
    }

    program_busy_loop(&terminate_event_loop);

    program_workers_cleanup(
            workers_user_data,
            workers_count);

    return 0;
}

int main() {
    return program_main();
}
