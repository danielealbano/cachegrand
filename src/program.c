#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
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

#include "utils_cpu.h"
#include "exttypes.h"
#include "misc.h"
#include "xalloc.h"
#include "log.h"
#include "hugepages.h"
#include "spinlock.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "signals_support.h"
#include "memory_fences.h"
#include "support/io_uring/io_uring_support.h"
#include "support/io_uring/io_uring_capabilities.h"
#include "network/protocol/network_protocol.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/channel/network_channel_iouring.h"
#include "worker/worker.h"
#include "worker/worker_iouring.h"
#include "config.h"
#include "slab_allocator.h"

#include "program.h"
#include "program_arguments.h"

#define TAG "program"

volatile bool program_terminate_event_loop = false;

network_channel_address_t program_addresses[] = { PROGRAM_NETWORK_ADDRESSES };
uint32_t program_addresses_count = PROGRAM_NETWORK_ADDRESSES_COUNT;

int program_signals[] = {  SIGUSR1,   SIGINT,   SIGHUP,   SIGTERM,   SIGQUIT  };
uint8_t program_signals_count = sizeof(program_signals) / sizeof(int);

static char* config_path_default = CACHEGRAND_CONFIG_PATH_DEFAULT;

void program_signal_handlers(
        int signal_number) {
    char *signal_name = SIGNALS_SUPPORT_NAME_WRAPPER(signal_number);

    int found_sig_index = -1;
    for(uint8_t i = 0; i < program_signals_count; i++) {
        if (program_signals[i] == signal_number) {
            found_sig_index = i;
            break;
        }
    }

    if (found_sig_index == -1) {
        LOG_V(
                TAG,
                "Received un-managed signal <%s (%d)>, ignoring",
                signal_name,
                signal_number);
        return;
    }

    LOG_V(
            TAG,
            "Received signal <%s (%d)>, requesting loop termination",
            signal_name,
            signal_number);
    program_request_terminate(&program_terminate_event_loop);
}

void program_register_signal_handlers() {
    struct sigaction action;
    action.sa_handler = program_signal_handlers;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

#ifndef DEBUG
    signals_support_register_sigsegv_fatal_handler();
#endif
    signal(SIGCHLD, SIG_IGN);

    for(uint8_t i = 0; i < program_signals_count; i++) {
        signals_support_register_signal_handler(program_signals[i], program_signal_handlers, NULL);
    }
}

worker_user_data_t* program_workers_initialize(
        volatile bool *terminate_event_loop,
        config_t* config,
        uint32_t workers_count) {
    int res;
    worker_user_data_t *workers_user_data;

    workers_user_data = xalloc_alloc_zero(sizeof(worker_user_data_t) * workers_count);

    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        worker_user_data_t *worker_user_data = &workers_user_data[worker_index];

        // TODO: needs to be changed to accept the config and setup all the settings via it
        worker_setup_user_data(
                worker_user_data,
                worker_index,
                terminate_event_loop,
                PROGRAM_NETWORK_MAX_CONNECTIONS_PER_WORKER,
                PROGRAM_NETWORK_CONNECTIONS_BACKLOG,
                program_addresses,
                program_addresses_count);

        LOG_V(TAG, "Creating worker <%u>", worker_index);

        // TODO: decide dynamically which kind of worker should start
        if (pthread_create(
                &worker_user_data->pthread,
                NULL,
                worker_iouring_thread_func,
                worker_user_data) != 0) {
            LOG_E(TAG, "Unable to start the worker <%u>", worker_index);
            LOG_E_OS_ERROR(TAG);
        }
    }

    return workers_user_data;
}

bool* program_get_terminate_event_loop() {
    return (bool*)&program_terminate_event_loop;
}

void program_request_terminate(
        volatile bool *terminate_event_loop) {
    *terminate_event_loop = true;
    HASHTABLE_MEMORY_FENCE_STORE();
}

bool program_should_terminate(
        volatile bool *terminate_event_loop) {
    HASHTABLE_MEMORY_FENCE_LOAD();
    return *terminate_event_loop;
}

void program_wait_loop(
        volatile bool *terminate_event_loop) {
    LOG_V(TAG, "Program loop started");

    // Wait for the software to terminate
    do {
        usleep(WORKER_LOOP_MAX_WAIT_TIME_MS * 1000);
    } while(!program_should_terminate(terminate_event_loop));

    LOG_V(TAG, "Program loop terminated");
}

void program_workers_cleanup(
        worker_user_data_t* workers_user_data,
        uint32_t workers_count) {
    int res;
    LOG_V(TAG, "Cleaning up workers");

    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        LOG_V(TAG, "Waiting for worker <%lu> to terminate", worker_index);
        res = pthread_join(workers_user_data[worker_index].pthread, NULL);
        if (res != 0) {
            LOG_E(TAG, "Error while joining the worker <%u>", worker_index);
            LOG_E_OS_ERROR(TAG);
        } else {
            LOG_V(TAG, "Worker terminated", worker_index);
        }
    }

    xalloc_free(workers_user_data);
}

void program_update_config_from_arguments(
        program_arguments_t* program_arguments,
        config_t* config) {
    if (program_arguments->log_level != PROGRAM_ARGUMENTS_LOG_LEVEL_MAX) {
        for(int i = 0; i < config->log_sinks_count; i++) {
            config->log_sinks[i].level = (config_log_level_t)program_arguments->log_level;
        }
    }
}

config_t* program_parse_arguments_and_load_config(
        int argc,
        char** argv) {
    config_t* config;
    program_arguments_t* program_arguments = program_arguments_init();

    if (!program_arguments_parse(argc, argv, program_arguments)) {
        program_arguments_free(program_arguments);
        LOG_E(TAG, "Failed to parse the arguments, unable to continue");
        return NULL;
    }

    if ((config = config_load(program_arguments->config_file
            ? program_arguments->config_file
            : config_path_default)) == NULL) {
        program_arguments_free(program_arguments);
        LOG_E(TAG, "Failed to load the configuration, unable to continue");
        return NULL;
    }

    program_update_config_from_arguments(program_arguments, config);

    program_arguments_free(program_arguments);

    return config;
}

bool program_use_slab_allocator(
        config_t* config) {
    // TODO: estimate how much 2mb hugepages should be available to properly work with the current settings, just
    //       having 2mb hugepages is not enough to guarantee that memory will be available during the execution, it
    //       should be reserved
    int requested_hugepages = 0;
    bool use_slab_allocator;

    // use_slab_allocator is optional
    if (config->use_slab_allocator == NULL) {
        use_slab_allocator = true;
    } else {
        use_slab_allocator = *config->use_slab_allocator;
    }

    if (use_slab_allocator) {
        if (!hugepages_2mb_is_available(requested_hugepages)) {
            LOG_W(TAG, "Disabling slab allocator, 2mb hugepages not available, performances will be degraded");
            use_slab_allocator = false;
        }
    } else {
        LOG_W(TAG, "slab allocator disabled in config, performances will be degraded");
    }

    slab_allocator_enable(use_slab_allocator);

    return use_slab_allocator;
}

int program_main(
        int argc,
        char** argv) {
    config_t* config;
    bool use_slab_allocator;
    uint32_t workers_count;
    worker_user_data_t* workers_user_data;

    // TODO: refactor this function to make it actually testable

    if ((config = program_parse_arguments_and_load_config(argc, argv)) == NULL) {
        return 1;
    }

    // TODO: initialize the log sinks
    // TODO: initialize the protocol parsers
    // TODO: initialize the network listeners and the protocol state machines

    use_slab_allocator = program_use_slab_allocator(config);

    program_register_signal_handlers();

    // TODO: should be possible to pinpoint in the config which cores can be utilized, very handy for benchmarking in
    //       in combination with the isolcpus kernel init parameter
#if DEBUG == 1
    workers_count = 1;
#else
    workers_count = utils_cpu_count();
#endif

    // TODO: initialize the hashtable(s) (with or without LRU, when the support will be added)
    // TODO: initialize the storage
    // TODO: start the worker threads and invoke the worker thread main func

    if (use_slab_allocator) {
        slab_allocator_predefined_allocators_init();
    }

    if ((workers_user_data = program_workers_initialize(
            &program_terminate_event_loop,
            config,
            workers_count)) == NULL) {
        return 1;
    }

    program_wait_loop(&program_terminate_event_loop);

    program_workers_cleanup(
            workers_user_data,
            workers_count);

    if (use_slab_allocator) {
        slab_allocator_predefined_allocators_free();
    }

    config_free(config);

    return 0;
}
