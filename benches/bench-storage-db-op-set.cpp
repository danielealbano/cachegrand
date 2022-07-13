/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <numa.h>

#include <benchmark/benchmark.h>

#include "misc.h"
#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "clock.h"
#include "config.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "slab_allocator.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "fiber.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"

#include "../tests/support.h"
#include "../tests/hashtable/fixtures-hashtable.h"

#include "bench-support.h"

#define KEYSET_MAX_SIZE             (double)0x7FFFFFFFu * (double)0.75
#define KEYSET_GENERATOR_METHOD     TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_RANDOM_STR_RANDOM_LENGTH

#define SET_BENCH_ARGS_HT_SIZE_AND_KEYS() \
    Args({0x0000FFFFu, 75})-> \
    Args({0x000FFFFFu, 75})-> \
    Args({0x001FFFFFu, 75})-> \
    Args({0x007FFFFFu, 75})-> \
    Args({0x00FFFFFFu, 75})-> \
    Args({0x01FFFFFFu, 50})-> \
    Args({0x01FFFFFFu, 75})-> \
    Args({0x07FFFFFFu, 50})-> \
    Args({0x07FFFFFFu, 75})-> \
    Args({0x0FFFFFFFu, 50})-> \
    Args({0x0FFFFFFFu, 75})-> \
    Args({0x1FFFFFFFu, 50})-> \
    Args({0x1FFFFFFFu, 75})-> \
    Args({0x3FFFFFFFu, 50})-> \
    Args({0x3FFFFFFFu, 75})-> \
    Args({0x7FFFFFFFu, 50})-> \
    Args({0x7FFFFFFFu, 75})

#define SET_BENCH_THREADS \
    Threads(1)-> \
    Threads(2)-> \
    Threads(4)-> \
    Threads(8)-> \
    Threads(16)-> \
    Threads(32)-> \
    Threads(64)-> \
    Threads(128)-> \
    Threads(256)-> \
    Threads(512)-> \
    Threads(1024)-> \
    Threads(2048)

#define SET_BENCH_ITERATIONS \
    Iterations(1)->\
    Repetitions(25)->\
    DisplayAggregatesOnly(true)

#define CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS() \
    UseRealTime()-> \
    SET_BENCH_ARGS_HT_SIZE_AND_KEYS()-> \
    SET_BENCH_ITERATIONS-> \
    SET_BENCH_THREADS

static char* keyset = NULL;
static uint64_t keyset_size = 0;

static void storage_db_op_set_keyset_init_notatest(benchmark::State& state) {
    keyset_size = KEYSET_MAX_SIZE + 1;

    keyset = test_support_init_keys(
            keyset_size,
            KEYSET_GENERATOR_METHOD,
            544498304);

    state.SkipWithError("Not a test, skipping");
}

static void storage_db_op_set_keyset_cleanup_notatest(benchmark::State& state) {
    test_support_free_keys(keyset, keyset_size);

    keyset = NULL;
    keyset_size = 0;

    state.SkipWithError("Not a test, skipping");
}

static void storage_db_op_set_new(benchmark::State& state) {
    static uint64_t requested_keyset_size;
    worker_context_t *worker_context;
    bool result;
    char error_message[150] = {0};
    static storage_db *db;
    storage_db_config_t *db_config;
    int threads_count = state.threads();

    if (bench_support_check_if_too_many_threads_per_core(state.threads(), BENCHES_MAX_THREADS_PER_CORE)) {
        sprintf(error_message, "Too many threads per core, max allowed <%d>", BENCHES_MAX_THREADS_PER_CORE);
        state.SkipWithError(error_message);
        return;
    }

    test_support_set_thread_affinity(state.thread_index());

    if (state.thread_index() == 0) {
        db = NULL;

        // Setup the storage db
        storage_db_config_t *db_config = storage_db_config_new();
        db_config->max_keys = state.range(0);
        db_config->backend_type = STORAGE_DB_BACKEND_TYPE_MEMORY;
        db = storage_db_new(db_config, threads_count);
        if (!db) {
            storage_db_config_free(db_config);
            return;
        }

        if ((worker_context = worker_context_get())) {
            storage_db_free(worker_context->db, worker_context->workers_count);
        }
    }

    // Setup the worker context, as it's required by the storage db
    if ((worker_context = worker_context_get()) == NULL) {
        worker_context = (worker_context_t *)slab_allocator_mem_alloc(sizeof(worker_context_t));
        worker_context_set(worker_context);
    }

    worker_context->workers_count = threads_count;
    worker_context->worker_index = state.thread_index();
    worker_context->worker_index = state.thread_index();
    worker_context->db = db;

    if (state.thread_index() == 0) {
        double requested_load_factor = (double)state.range(1) / 100.0f;
        requested_keyset_size = (double)state.range(0) * requested_load_factor;
        if (requested_keyset_size > keyset_size) {
            sprintf(
                    error_message,
                    "The requested keyset size of <%lu> is greater than the one available of <%lu>, can't continue",
                    requested_keyset_size,
                    keyset_size);
            state.SkipWithError(error_message);
            return;
        }

        // Flush the cpu DCACHE related to the portion of the keyset used by the bench before starting the test
        test_support_flush_data_cache(
                keyset,
                TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * requested_keyset_size);
    }

    for (auto _ : state) {
        for(long int i = state.thread_index(); i < requested_keyset_size; i += state.threads()) {
            uint64_t keyset_offset = TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i;
            char* key = keyset + keyset_offset;
            size_t key_length = strlen(key);

            result = storage_db_set_small_value(
                    db,
                    key,
                    key_length,
                    key,
                    key_length);

            if (!result) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s> with index <%ld> for the thread <%d>",
                        key,
                        i,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }
        }
    }

    // VALIDATION
    for(long int i = state.thread_index(); i < requested_keyset_size; i += state.threads()) {
        char buffer[TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL + 1] = { 0 };
        uint64_t keyset_offset = TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i;
        char *key = keyset + keyset_offset;
        size_t key_length = strlen(key);

        storage_db_entry_index_t *entry_index = storage_db_get_entry_index(db, key, key_length);

        if (db->config->backend_type != STORAGE_DB_BACKEND_TYPE_MEMORY) {
            if (entry_index->key_length != key_length) {
                sprintf(
                        error_message,
                        "Mismatching key length for key <%s> at index <%ld> for the thread <%d>",
                        key,
                        i,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }
        }

        // Try to acquire a reader lock, don't check if the value has been deleted as no deletions are carried out
        storage_db_entry_index_status_increase_readers_counter(
                entry_index,
                NULL);

        bool res = storage_db_entry_chunk_read(
                db,
                storage_db_entry_value_chunk_get(entry_index, 0),
                buffer);
        storage_db_entry_index_status_decrease_readers_counter(entry_index, NULL);

        if (!res) {
            sprintf(
                    error_message,
                    "Unable to set the key <%s> with index <%ld> for the thread <%d>",
                    key,
                    i,
                    state.thread_index());
            state.SkipWithError(error_message);
            break;
        }

        if (strncmp(buffer, key, key_length) != 0) {
            sprintf(
                    error_message,
                    "Mismatching value for key at index <%ld> for the thread <%d>, expected <%s> found <%.*s>",
                    i,
                    state.thread_index(),
                    key,
                    (int)key_length,
                    buffer);
            state.SkipWithError(error_message);
            break;
        }
    }

    if (state.thread_index() == 0) {
        bench_support_collect_hashtable_stats_and_update_state(state, db->hashtable);
    }
}

BENCHMARK(storage_db_op_set_keyset_init_notatest)
    ->Iterations(1)
    ->Threads(1)
    ->Repetitions(1);

BENCHMARK(storage_db_op_set_new)
    ->CONFIGURE_BENCH_MT_HT_SIZE_AND_KEYS();

BENCHMARK(storage_db_op_set_keyset_cleanup_notatest)
    ->Iterations(1)
    ->Threads(1)
    ->Repetitions(1);
