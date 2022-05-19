/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <netinet/in.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <liburing.h>
#include <assert.h>
#include <sys/resource.h>

#include "misc.h"
#include "pow2.h"
#include "utils_numa.h"
#include "exttypes.h"
#include "xalloc.h"
#include "pidfile.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_console.h"
#include "hugepages.h"
#include "spinlock.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "memory_fences.h"
#include "network/protocol/network_protocol.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "config.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "thread.h"
#include "hugepage_cache.h"
#include "slab_allocator.h"
#include "support/sentry/sentry_support.h"
#include "signal_handler_thread.h"

#include "program.h"
#include "program_arguments.h"

#define TAG "program"

volatile bool program_terminate_event_loop = false;

static char* config_path_default = CACHEGRAND_CONFIG_PATH_DEFAULT;

signal_handler_thread_context_t* program_signal_handler_thread_initialize(
        volatile bool *terminate_event_loop,
        program_context_t *program_context) {
    signal_handler_thread_context_t *signal_handler_thread_context;

    program_context->signal_handler_thread_context = signal_handler_thread_context =
            xalloc_alloc_zero(sizeof(signal_handler_thread_context_t));
    signal_handler_thread_context->terminate_event_loop = terminate_event_loop;

    LOG_V(TAG, "Creating signal handler thread");

    if (pthread_create(
            &signal_handler_thread_context->pthread,
            NULL,
            signal_handler_thread_func,
            signal_handler_thread_context) != 0) {
        LOG_E(TAG, "Unable to start the signal handler thread");
        LOG_E_OS_ERROR(TAG);

        return NULL;
    }

    return signal_handler_thread_context;
}

void program_workers_initialize_count(
        program_context_t *program_context) {
    program_context->workers_count =
            program_context->config->workers_per_cpus * program_context->selected_cpus_count;
}

worker_context_t* program_workers_initialize_context(
        volatile bool *terminate_event_loop,
        program_context_t *program_context)  {
    worker_context_t *workers_context;

    program_context->workers_context = workers_context =
            xalloc_alloc_zero(sizeof(worker_context_t) * program_context->workers_count);

    for(uint32_t worker_index = 0; worker_index < program_context->workers_count; worker_index++) {
        worker_context_t *worker_context = &workers_context[worker_index];

        worker_setup_context(
                worker_context,
                program_context->workers_count,
                worker_index,
                terminate_event_loop,
                program_context->config,
                program_context->hashtable,
                program_context->db);

        LOG_V(TAG, "Creating worker <%u>", worker_index);

        if (pthread_create(
                &worker_context->pthread,
                NULL,
                worker_thread_func,
                worker_context) != 0) {
            LOG_E(TAG, "Unable to start the worker <%u>", worker_index);
            LOG_E_OS_ERROR(TAG);

            break;
        }
    }

    return workers_context;
}

void program_workers_wait_start(
        program_context_t *program_context) {
    for(uint32_t worker_index = 0; worker_index < program_context->workers_count; worker_index++) {
        worker_wait_running(&program_context->workers_context[worker_index]);
    }
}

bool* program_get_terminate_event_loop() {
    return (bool*)&program_terminate_event_loop;
}

void program_request_terminate(
        volatile bool *terminate_event_loop) {
    *terminate_event_loop = true;
    MEMORY_FENCE_STORE();
}

bool program_should_terminate(
        const volatile bool *terminate_event_loop) {
    MEMORY_FENCE_LOAD();
    return *terminate_event_loop;
}

bool program_has_aborted_workers(
        worker_context_t* workers_context,
        uint32_t workers_count) {

    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        if (workers_context[worker_index].pthread == 0) {
            continue;
        }

        MEMORY_FENCE_LOAD();

        if (workers_context[worker_index].aborted) {
            return true;
        }
    }

    return false;
}

void program_wait_loop(
        worker_context_t* workers_context,
        uint32_t workers_count,
        const bool_volatile_t *terminate_event_loop) {
    LOG_V(TAG, "Wait loop started");

    // Wait for the software to terminate
    while(
            !program_should_terminate(terminate_event_loop) &&
            !program_has_aborted_workers(workers_context, workers_count)) {
        usleep(WORKER_LOOP_MAX_WAIT_TIME_MS * 1000);
    }

    LOG_V(TAG, "Wait loop terminated");
}

void program_workers_cleanup(
        worker_context_t* context,
        uint32_t workers_count) {
    int res;
    LOG_V(TAG, "Cleaning up workers");

    for(uint32_t worker_index = 0; worker_index < workers_count; worker_index++) {
        if (context[worker_index].pthread == 0) {
            continue;
        }

        LOG_V(
                TAG,
                "Waiting for worker <%u> to terminate",
                worker_index);
        res = pthread_join(
                context[worker_index].pthread,
                NULL);

        if (res != 0) {
            LOG_E(
                    TAG,
                    "Error while joining the worker <%u>",
                    worker_index);
            LOG_E_OS_ERROR(TAG);
        } else {
            LOG_V(
                    TAG,
                    "Worker <%u> terminated",
                    worker_index);
        }
    }
}

void program_signal_handler_thread_cleanup(
        signal_handler_thread_context_t *context) {
    int res;

    LOG_V(
            TAG,
            "Cleaning signal handler thread");
    LOG_V(
            TAG,
            "Waiting for signal handler thread to terminate");

    res = pthread_join(
            context->pthread,
            NULL);
    if (res != 0) {
        LOG_E(
                TAG,
                "Error while joining the signal handler thread");
        LOG_E_OS_ERROR(TAG);
    } else {
        LOG_V(
                TAG,
                "Signal handler thread terminated");
    }
}

void program_update_config_from_arguments(
        program_arguments_t* program_arguments,
        config_t* config) {
    if (program_arguments->log_level != PROGRAM_ARGUMENTS_LOG_LEVEL_MAX) {
        config_log_level_t config_log_levels[] = {
                CONFIG_LOG_LEVEL_DEBUG, CONFIG_LOG_LEVEL_VERBOSE, CONFIG_LOG_LEVEL_INFO,
                CONFIG_LOG_LEVEL_WARNING, CONFIG_LOG_LEVEL_ERROR
        };
        size_t config_log_levels_len = sizeof(config_log_levels) / sizeof(config_log_level_t);

        bool set_remaining_bits = false;
        config_log_level_t config_log_level = 0;
        for(int i = 0; i < config_log_levels_len; i++) {
            if ((config_log_level_t)program_arguments->log_level == config_log_levels[i]) {
                set_remaining_bits = true;
            }

            if (set_remaining_bits) {
                config_log_level |= config_log_levels[i];
            }
        }

        for(int i = 0; i < config->logs_count; i++) {
            config->logs[i].level = config_log_level;
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
        program_context_t* program_context) {
    // TODO: estimate how much 2mb hugepages should be available to properly work with the current settings, just
    //       having 2mb hugepages is not enough to guarantee that memory will be available during the execution, it
    //       should be reserved
    int requested_hugepages = 0;
    bool use_slab_allocator;

    // use_slab_allocator is optional
    if (program_context->config->use_slab_allocator == NULL) {
        use_slab_allocator = true;
    } else {
        use_slab_allocator = *program_context->config->use_slab_allocator;
    }

    if (use_slab_allocator) {
        if (!hugepages_2mb_is_available(requested_hugepages)) {
            LOG_W(TAG, "Not enough 2mb hugepages, disabling slab allocator, performances will be degraded");
            use_slab_allocator = false;
        }
    } else {
        LOG_W(TAG, "slab allocator disabled in config, performances will be degraded");
    }

    if (use_slab_allocator) {
        hugepage_cache_init();
    }

    slab_allocator_enable(use_slab_allocator);

    program_context->use_slab_allocator = use_slab_allocator;

    return use_slab_allocator;
}

void program_setup_initial_log_sink_console() {
    log_level_t level = LOG_LEVEL_ALL;
    log_sink_settings_t settings = { 0 };
    settings.console.use_stdout_for_errors = false;

#if NDEBUG == 1
    level -= LOG_LEVEL_DEBUG;
#endif

    log_sink_register(log_sink_console_init(level, &settings));
}

void program_config_setup_log_sinks(
        config_t* config) {
    // Free up the temporary registered log sink (the console one)
    log_sink_registered_free();

    // Iterate over the log sinks defined in the configuration
    for(int i = 0; i < config->logs_count; i++) {
        log_level_t log_levels;
        log_sink_settings_t log_sink_settings = { 0 };

        config_log_t* config_log = &config->logs[i];

        log_sink_type_t log_sink_type = (log_sink_type_t)config_log->type;

        // Setup log levels from config - first pass - set all if requested
        log_levels = (config_log->level & CONFIG_LOG_LEVEL_ALL) == CONFIG_LOG_LEVEL_ALL
                     ? LOG_LEVEL_ALL
                     : 0;

        // Setup log levels from config - second pass - set the directly specified log levels
        log_levels |= (log_level_t)config_log->level & LOG_LEVEL_ALL;

        // Setup log levels from config - third pass - apply negation (no-* log levels in config)
        log_levels &= (~(config_log->level >> 8));

        // Setup log levels from config - fourth pass - always drop LOG_LEVEL_DEBUG_INTERNALS, not settable
        log_levels &= ~LOG_LEVEL_DEBUG_INTERNALS;

#if NDEBUG == 1
        // Setup log levels from config - fourth pass - always drop LOG_LEVEL_DEBUG in release builds, not available
        log_levels &= ~LOG_LEVEL_DEBUG;
#endif

        switch (log_sink_type) {
            case LOG_SINK_TYPE_CONSOLE:
                log_sink_settings.console.use_stdout_for_errors = true;
                break;

            case LOG_SINK_TYPE_FILE:
                log_sink_settings.file.path = config_log->file->path;
                break;

            default:
                // To catch mismatches
                assert(false);
        }

        log_sink_register(log_sink_factory(log_sink_type, log_levels, &log_sink_settings));
    }
}

bool program_config_thread_affinity_set_selected_cpus(
        program_context_t* program_context) {
    if (config_cpus_parse(
            utils_numa_cpu_configured_count(),
            program_context->config->cpus,
            program_context->config->cpus_count,
            &program_context->selected_cpus,
            &program_context->selected_cpus_count) == false) {
        return false;
    }

    thread_affinity_set_selected_cpus(
            program_context->selected_cpus,
            program_context->selected_cpus_count);

    return true;
}

bool program_config_setup_hashtable(
        program_context_t* program_context) {
    hashtable_config_t* hashtable_config = hashtable_mcmp_config_init();
    hashtable_config->can_auto_resize = false;
    hashtable_config->initial_size = pow2_next(program_context->config->database->max_keys);

    program_context->hashtable = hashtable_mcmp_init(hashtable_config);

    if (!program_context->hashtable) {
        hashtable_mcmp_config_free(hashtable_config);
        program_context->hashtable = NULL;
    }

    return program_context->hashtable;
}

bool program_config_setup_storage_db(
        program_context_t* program_context) {
    // TODO: read the configuration from the config file
    storage_db_config_t *config = storage_db_config_new();
    config->shard_size_mb = program_context->config->storage->shard_size_mb;

    if (program_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING_FILE) {
        config->backend.file.basedir_path = program_context->config->storage->io_uring->path;

        if (program_context->config->storage->backend == CONFIG_STORAGE_BACKEND_IO_URING_FILE) {
            config->backend_type = STORAGE_DB_BACKEND_TYPE_FILE;
        } else {
            config->backend_type = STORAGE_DB_BACKEND_TYPE_BLOCK_DEVICE;
        }
    }

    program_context->db = storage_db_new(config);
    if (!program_context->db) {
        storage_db_config_free(config);
    }

    return program_context->db;
}

void program_setup_sentry(
        program_context_t program_context) {
    if (program_context.config->sentry == NULL ||
        program_context.config->sentry->enable == false) {
        return;
    }

    sentry_support_init(
            program_context.config->sentry->data_path,
            program_context.config->sentry->dsn);
}

bool program_setup_pidfile(
        program_context_t program_context) {
    if (program_context.config->pidfile_path == NULL) {
        return true;
    }

    return pidfile_create(program_context.config->pidfile_path);
}

bool program_setup_ulimit_wrapper(
        __rlimit_resource_t resource,
        ulong value) {
    struct rlimit limit;

    limit.rlim_cur = value;
    limit.rlim_max = value;

    return setrlimit(resource, &limit) == 0;
}

bool program_setup_ulimit_nofile() {
    // TODO: this should come from the config but 0x80000 (524288) is a value extremely high that will cover for all
    //       the practical use cases, the current cachegrand storage architecture uses a small amount of file
    //       descriptors therefore the vast majority are for the network and with such a high number a system should
    //       be able to handle more than half a million of active connections (taking into account the linger time
    //       more than 15 IP addresses should be used before saturating the file descriptors).
    LOG_V(TAG, "> Setting max opened file ulimit to %d", PROGRAM_ULIMIT_NOFILE);
    if (program_setup_ulimit_wrapper(RLIMIT_NOFILE, PROGRAM_ULIMIT_NOFILE) == false) {
        LOG_E(TAG, "Unable to set max opened file ulimit");
        LOG_E_OS_ERROR(TAG);
    }

    return true;
}

bool program_setup_ulimit_memlock() {
    LOG_V(TAG, "> Setting max lockable memory ulimit to %lu", PROGRAM_ULIMIT_MEMLOCK);
    if (program_setup_ulimit_wrapper(RLIMIT_MEMLOCK, PROGRAM_ULIMIT_MEMLOCK) == false) {
        LOG_E(TAG, "Unable to set the lockable memory ulimit");
        LOG_E_OS_ERROR(TAG);
    }

    return true;
}

bool program_setup_ulimit() {
    LOG_V(TAG, "Configuring process ulimits");
    return program_setup_ulimit_nofile() && program_setup_ulimit_memlock();
}

void program_cleanup(
        program_context_t* program_context) {
    // TODO: free storage backend

    if (program_context->workers_context) {
        program_workers_cleanup(
                program_context->workers_context,
                program_context->workers_count);
        xalloc_free(program_context->workers_context);
    }

    if (program_context->signal_handler_thread_context) {
        program_signal_handler_thread_cleanup(
                program_context->signal_handler_thread_context);
        xalloc_free(program_context->signal_handler_thread_context);
    }

    if (program_context->db) {
        storage_db_close(program_context->db);
        storage_db_free(program_context->db);
    }

    if (program_context->hashtable) {
        hashtable_mcmp_free(program_context->hashtable);
    }

    if (program_context->slab_allocator_inited) {
        slab_allocator_predefined_allocators_free();
        hugepage_cache_init();
    }

    if (program_context->selected_cpus) {
        xalloc_free(program_context->selected_cpus);
        program_context->selected_cpus_count = 0;
    }

    if (program_context->config) {
        config_free(program_context->config);
    }

    log_sink_registered_free();

    if (pidfile_is_owned()) {
        pidfile_close(pidfile_get_fd());
    }

    sentry_support_shutdown();
}

int program_main(
        int argc,
        char** argv) {
    static program_context_t program_context = { 0 };

    // TODO: refactor this function to make it actually testable

    // Initialize the console log sink to be able to print logs, will update it with the settings from the config at
    // later stage
    program_setup_initial_log_sink_console();

    if ((program_context.config = program_parse_arguments_and_load_config(argc, argv)) == NULL) {
        return 1;
    }

    if (program_setup_ulimit() == false) {
        LOG_E(TAG, "Unable to setup the system ulimits");
        program_cleanup(&program_context);
        return 1;
    }

    program_setup_sentry(program_context);

    if (program_setup_pidfile(program_context) == false) {
        program_cleanup(&program_context);
        return 1;
    }

    if (program_config_thread_affinity_set_selected_cpus(&program_context) == false) {
        LOG_E(TAG, "Unable to setup cpu affinity");
        program_cleanup(&program_context);
        return 1;
    }

    // Signal handling
    signal(SIGCHLD, SIG_IGN);

    // Enable, if allowed in the config and if the hugepages are available, the slab allocator
    program_use_slab_allocator(&program_context);

    // Calculate workers count
    program_workers_initialize_count(&program_context);

    // Initialize the log sinks defined in the configuration, if any is defined. The function will take care of dropping
    // the temporary log sink defined initially
    program_config_setup_log_sinks(program_context.config);

    if (program_context.use_slab_allocator) {
        slab_allocator_predefined_allocators_init();
        program_context.slab_allocator_inited = true;
    }

    if (program_config_setup_hashtable(&program_context) == false) {
        LOG_E(TAG, "Unable to initialize the hashtable");
        program_cleanup(&program_context);
        return 1;
    }

    if (program_config_setup_storage_db(&program_context) == false) {
        LOG_E(TAG, "Unable to initialize the database");
        program_cleanup(&program_context);
        return 1;
    }

    if (program_signal_handler_thread_initialize(
            &program_terminate_event_loop,
            &program_context) == NULL) {
        program_cleanup(&program_context);
        return 1;
    }

    if (program_workers_initialize_context(
            &program_terminate_event_loop,
            &program_context) == NULL) {
        program_cleanup(&program_context);
        return 1;
    }

    program_workers_wait_start(&program_context);
    program_wait_loop(
            program_context.workers_context,
            program_context.workers_count,
            &program_terminate_event_loop);

    // The program_request_terminate is invoked to be sure that if the termination is being triggered because a worker
    // thread is aborting, every other thread is also notified and will terminate the execution
    program_request_terminate(&program_terminate_event_loop);

    LOG_I(TAG, "Terminating");

    program_cleanup(&program_context);

    return 0;
}
