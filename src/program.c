/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
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
#include <string.h>
#include <stdlib.h>
#include <mimalloc.h>

#include "misc.h"
#include "pow2.h"
#include "utils_numa.h"
#include "exttypes.h"
#include "clock.h"
#include "xalloc.h"
#include "memory_fences.h"
#include "pidfile.h"
#include "log/log.h"
#include "log/sink/log_sink.h"
#include "log/sink/log_sink_console.h"
#include "hugepages.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "config.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "epoch_gc.h"
#include "epoch_gc_worker.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/network_tls.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "fiber/fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "thread.h"
#include "hugepage_cache.h"
#include "memory_allocator/ffma.h"
#include "support/sentry/sentry_support.h"
#include "signal_handler_thread.h"

#include "program.h"
#include "program_arguments.h"
#include "program_ulimit.h"

#define TAG "program"

static program_context_t program_context_global = { 0 };
bool_volatile_t program_terminate_event_loop = false;

static char* config_path_default = CACHEGRAND_CONFIG_PATH_DEFAULT;

program_context_t *program_get_context() {
    return &program_context_global;
}

void program_reset_context() {
    memset(&program_context_global, 0, sizeof(program_context_t));
}

signal_handler_thread_context_t* program_signal_handler_thread_initialize(
        bool_volatile_t *terminate_event_loop,
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

bool program_epoch_gc_workers_initialize(
        bool_volatile_t *terminate_event_loop,
        program_context_t *program_context) {

    int epoch_gc_workers_count = (int)EPOCH_GC_OBJECT_TYPE_MAX;
    program_context->epoch_gc_workers_context =
            xalloc_alloc_zero(epoch_gc_workers_count * sizeof(epoch_gc_worker_context_t));

    LOG_V(TAG, "Creating epoch gc workers");

    for(int object_type_index = 0; object_type_index < epoch_gc_workers_count; object_type_index++) {
        epoch_gc_worker_context_t *epoch_gc_worker_context =
                &program_context->epoch_gc_workers_context[object_type_index];

        epoch_gc_worker_context->epoch_gc = epoch_gc_init(object_type_index);
        epoch_gc_worker_context->terminate_event_loop = terminate_event_loop;
        epoch_gc_worker_context->stats.collected_objects = 0;

        if (pthread_create(
                &epoch_gc_worker_context->pthread,
                NULL,
                epoch_gc_worker_func,
                epoch_gc_worker_context) != 0) {
            LOG_E(TAG, "Unable to start the epoch gc worker for object type index <%d>", object_type_index);
            LOG_E_OS_ERROR(TAG);
            return false;
        }
    }

    return true;
}

void program_workers_initialize_count(
        program_context_t *program_context) {
    program_context->workers_count =
            program_context->config->workers_per_cpus * program_context->selected_cpus_count;
}

worker_context_t* program_workers_initialize_context(
        bool_volatile_t *terminate_event_loop,
        program_context_t *program_context)  {
    timespec_t started_on_timestamp = { 0 };
    worker_context_t *workers_context;

    clock_monotonic(&started_on_timestamp);

    LOG_I(TAG, "Starting <%u> workers", program_context->workers_count);

    program_context->workers_context = workers_context =
            xalloc_alloc_zero(sizeof(worker_context_t) * program_context->workers_count);

    for(uint32_t worker_index = 0; worker_index < program_context->workers_count; worker_index++) {
        worker_context_t *worker_context = &workers_context[worker_index];

        worker_setup_context(
                worker_context,
                &started_on_timestamp,
                program_context->workers_count,
                worker_index,
                terminate_event_loop,
                program_context->config,
                program_context->db);

        LOG_V(TAG, "Setting up worker <%u>", worker_index);

        if (pthread_create(
                &worker_context->pthread,
                NULL,
                worker_thread_func,
                worker_context) != 0) {
            LOG_E(TAG, "Unable to start the worker <%u>", worker_index);
            LOG_E_OS_ERROR(TAG);

            break;
        }

        // Wait for the worker to start to ensure sequential creation
        worker_wait_running(worker_context);

        // If one worker has aborted it makes no sense to continue the initialization
        if (worker_context->aborted) {
            LOG_E(TAG, "Worker <%d> aborted the initialization, can't continue", worker_index);
            break;
        }

        // If one worker is not marked as running and is not aborted something went dramatically wrong, better to abort
        if (!worker_context->running) {
            LOG_E(TAG, "Worker <%d> not running after the initialization, can't continue", worker_index);
            break;
        }

        LOG_V(TAG, "Worker <%u> successfully started", worker_index);
    }

    return workers_context;
}

bool program_workers_ensure_started(
        program_context_t *program_context) {
    MEMORY_FENCE_LOAD();
    for(uint32_t worker_index = 0; worker_index < program_context->workers_count; worker_index++) {
        worker_context_t *worker_context = &program_context->workers_context[worker_index];
        if (!worker_context->running || worker_context->aborted) {
            return false;
        }
    }

    return true;
}

bool* program_get_terminate_event_loop() {
    return (bool*)&program_terminate_event_loop;
}

void program_request_terminate(
        bool_volatile_t *terminate_event_loop) {
    *terminate_event_loop = true;
    MEMORY_FENCE_STORE();
}

bool program_should_terminate(
        const bool_volatile_t *terminate_event_loop) {
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

void program_epoch_gc_workers_cleanup(
        epoch_gc_worker_context_t *epoch_gc_workers_context,
        uint32_t epoch_gc_workers_count) {
    int res;

    LOG_V(
            TAG,
            "Cleaning signal handler thread");
    LOG_V(
            TAG,
            "Waiting for signal handler thread to terminate");

    for(uint32_t index = 0; index < epoch_gc_workers_count; index++) {
        epoch_gc_worker_context_t *epoch_gc_worker_context = &epoch_gc_workers_context[index];

        if (epoch_gc_worker_context->epoch_gc == NULL) {
            continue;
        }

        res = pthread_join(
                epoch_gc_worker_context->pthread,
                NULL);

        if (res != 0) {
            LOG_E(TAG, "Error while joining the epoch gc worker for object type <%d>", index);
            LOG_E_OS_ERROR(TAG);
        } else {
            LOG_V(TAG, "Epoch gc worker for object type <%d> terminated", index);
        }

        epoch_gc_free(epoch_gc_worker_context->epoch_gc);
    }

    LOG_V(TAG, "Epoch gc workers terminated");
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

void program_list_tls_available_cipher_suites() {
    size_t cipher_suite_longest_name_length = 0;
    fprintf(stdout, "Available TLS cipher suites:\n\n");
    network_tls_mbedtls_cipher_suite_info_t *cipher_suites_info =
            network_tls_mbedtls_get_all_cipher_suites_info();

    // Calculate the length of the longest cipher suite name
    for(
            network_tls_mbedtls_cipher_suite_info_t *cipher_suite_info = cipher_suites_info;
            cipher_suite_info->name != NULL;
            cipher_suite_info++) {
        size_t cipher_suite_name_length = strlen(cipher_suite_info->name);
        if (cipher_suite_name_length > cipher_suite_longest_name_length) {
            cipher_suite_longest_name_length = cipher_suite_name_length;
        }
    }

    fprintf(
            stdout,
            "+-%.*s-+-%.11s-+-%.11s-+-%.10s-+\n",
            (int)cipher_suite_longest_name_length,
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------");

    fprintf(
            stdout,
            "| %-*s | %11s | %11s | %10s |\n",
            (int)cipher_suite_longest_name_length,
            "Cipher Suite",
            "Min Version",
            "Max Version",
            "Offloading");

    fprintf(
            stdout,
            "+-%.*s-+-%.11s-+-%.11s-+-%.10s-+\n",
            (int)cipher_suite_longest_name_length,
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------");

    // Print out the table
    for(
            network_tls_mbedtls_cipher_suite_info_t *cipher_suite_info = cipher_suites_info;
            cipher_suite_info->name != NULL;
            cipher_suite_info++) {
        fprintf(
                stdout,
                "| %-*s | %11s | %11s | %10s |\n",
                (int)cipher_suite_longest_name_length,
                cipher_suite_info->name,
                network_tls_min_version_to_string(cipher_suite_info->min_version),
                network_tls_max_version_to_string(cipher_suite_info->max_version),
                cipher_suite_info->offloading ? "kTLS" : "");
    }

    fprintf(
            stdout,
            "+-%.*s-+-%.11s-+-%.11s-+-%.10s-+\n",
            (int)cipher_suite_longest_name_length,
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------",
            "----------------------------------------------------------------------------------------------------");

    fflush(stdout);
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

    if (program_arguments->list_tls_available_cipher_suites) {
        program_list_tls_available_cipher_suites();
        exit(0);
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

bool program_use_huge_pages(
        program_context_t* program_context) {
    // TODO: estimate how much 2mb hugepages should be available to properly work with the current settings, just
    //       having 2mb hugepages is not enough to guarantee that memory will be available during the execution, it
    //       should be reserved
    int requested_hugepages = 0;
    bool use_huge_pages;

    // use_huge_pages is optional
    if (program_context->config->use_huge_pages == NULL) {
        use_huge_pages = false;
    } else {
        use_huge_pages = *program_context->config->use_huge_pages;
    }

    if (use_huge_pages) {
        if (!hugepages_2mb_is_available(requested_hugepages)) {
            LOG_W(TAG, "Not enough 2mb hugepages, disabling fast fixed memory allocator, performances will be degraded");
            use_huge_pages = false;
        }
    } else {
        LOG_W(TAG, "Fast fixed memory allocator disabled in config, performances will be degraded");
    }

    if (use_huge_pages) {
        hugepage_cache_init();
        ffma_enable(use_huge_pages);
    } else {
        ffma_enable(false);
    }

    program_context->use_huge_pages = use_huge_pages;

    return use_huge_pages;
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

bool program_config_setup_storage_db(
        program_context_t* program_context) {
    storage_db_config_t *config = storage_db_config_new();

    config->max_keys = program_context->config->database->max_keys;

    if (program_context->config->database->backend == CONFIG_DATABASE_BACKEND_FILE) {
        config->backend.file.shard_size_mb = program_context->config->database->file->shard_size_mb;
        config->backend.file.basedir_path = program_context->config->database->file->path;
        config->backend_type = STORAGE_DB_BACKEND_TYPE_FILE;
    } else if (program_context->config->database->backend == CONFIG_DATABASE_BACKEND_MEMORY) {
        config->backend_type = STORAGE_DB_BACKEND_TYPE_MEMORY;
    }

    program_context->db = storage_db_new(config, program_context->workers_count);
    if (!program_context->db) {
        storage_db_config_free(config);
    }

    return program_context->db;
}

void program_setup_sentry(
        program_context_t *program_context) {
    if (program_context->config->sentry == NULL ||
        program_context->config->sentry->enable == false) {
        return;
    }

    sentry_support_init(program_context->config->sentry->data_path);
}

bool program_setup_pidfile(
        program_context_t *program_context) {
    if (program_context->config->pidfile_path == NULL) {
        return true;
    }

    return pidfile_create(program_context->config->pidfile_path);
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
        storage_db_free(program_context->db, program_context->workers_count);
    }

    if (program_context->epoch_gc_workers_context) {
        program_epoch_gc_workers_cleanup(
                program_context->epoch_gc_workers_context,
                program_context->epoch_gc_workers_count);
        xalloc_free(program_context->epoch_gc_workers_context);
    }

    if (program_context->fast_memory_allocator_initialized) {
        hugepage_cache_free();
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
    int return_res = 1;
    program_context_t *program_context = &program_context_global;

    // TODO: refactor this function to make it actually testable

    // Initialize the console log sink to be able to print logs, will update it with the settings from the config at
    // later stage
    program_setup_initial_log_sink_console();

    if ((program_context->config = program_parse_arguments_and_load_config(argc, argv)) == NULL) {
        goto end;
    }

    // Report some general information
    LOG_I(
            TAG,
            "%s version %s (built on %s)",
            CACHEGRAND_CMAKE_CONFIG_NAME,
            CACHEGRAND_CMAKE_CONFIG_VERSION_GIT,
            CACHEGRAND_CMAKE_CONFIG_BUILD_DATE_TIME);
    LOG_I(
            TAG,
            "> %s build, compiled using %s v%s",
            CACHEGRAND_CMAKE_CONFIG_BUILD_TYPE,
            CACHEGRAND_CMAKE_CONFIG_C_COMPILER_ID,
            CACHEGRAND_CMAKE_CONFIG_C_COMPILER_VERSION);
    LOG_I(
            TAG,
            "> Hashing algorithm in use %s",
            CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2
            ? "t1ha2"
            : (CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3
               ? "xxh3" :
               "crc32c"));
    LOG_I(
            TAG,
            "> TLS: %s (kernel offloading %s)",
            network_tls_mbedtls_version(),
            network_tls_is_ulp_tls_supported() ? "enabled" : "disabled");
    if (!network_tls_is_ulp_tls_supported()) {
        LOG_I(
                TAG,
                "       Try to load the tls kernel module with \"modprobe tls\" and restart %s",
                CACHEGRAND_CMAKE_CONFIG_NAME);
    }

    LOG_I(
            TAG,
            "> Clock resolution: %ld ms",
            clock_realtime_coarse_get_resolution_ms());

    // Initialize the log sinks defined in the configuration, if any is defined. The function will take care of dropping
    // the temporary log sink defined initially
    program_config_setup_log_sinks(program_context->config);

    program_ulimit_setup();

    program_setup_sentry(program_context);

    // If it fails to create the pidfile reports an error and continues the execution, no need to check for the result
    // of the operation
    program_setup_pidfile(program_context);

    if (program_config_thread_affinity_set_selected_cpus(program_context) == false) {
        LOG_E(TAG, "Unable to setup cpu affinity");
        goto end;
    }

    // Signal handling
    signal(SIGCHLD, SIG_IGN);

    // If enabled in the config and if the hugepages are available enables the usage of hugepages to take advantage, for
    // example, of the fast fixed memory allocator and to run the code from the hugepages after relocating it there
    program_use_huge_pages(program_context);

    // Calculate workers count
    program_workers_initialize_count(program_context);

    if (program_context->use_huge_pages) {
        program_context->fast_memory_allocator_initialized = true;
    }

    if (program_config_setup_storage_db(program_context) == false) {
        LOG_E(TAG, "Unable to initialize the database");
        goto end;
    }

    if (program_signal_handler_thread_initialize(
            &program_terminate_event_loop,
            program_context) == NULL) {
        goto end;
    }

    if (program_epoch_gc_workers_initialize(
            &program_terminate_event_loop,
            program_context) == false) {
        goto end;
    }

    if (program_workers_initialize_context(
            &program_terminate_event_loop,
            program_context) == NULL) {
        goto end;
    }

    if (!program_workers_ensure_started(program_context)) {
        LOG_E(TAG, "One or more workers didn't start correctly, can't continue");
        goto end;
    }

    LOG_I(TAG, "Ready to accept connections");

    program_wait_loop(
            program_context->workers_context,
            program_context->workers_count,
            &program_terminate_event_loop);

    return_res = 0;

end:
    // The program_request_terminate is invoked to be sure that if the termination is being triggered because a worker
    // thread is aborting, every other thread is also notified and will terminate the execution
    program_request_terminate(&program_terminate_event_loop);

    LOG_V(TAG, "Terminating");

    program_cleanup(program_context);

#if FFMA_DEBUG_ALLOCS_FREES == 1
    ffma_debug_allocs_frees_end();
#endif

    return return_res;
}
