/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstdint>
#include <numa.h>

#include <benchmark/benchmark.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "config.h"
#include "random.h"
#include "intrinsics.h"
#include "xalloc.h"
#include "thread.h"
#include "memory_fences.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "spinlock.h"
#include "log/log.h"
#include "memory_fences.h"
#include "utils_cpu.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint64.h"
#include "epoch_gc.h"
#include "epoch_operation_queue.h"
#include "data_structures/hashtable_mpmc/hashtable_mpmc.h"
#include "mimalloc.h"

#define TEST_VALIDATE_KEYS 0
#define BENCH_HASHTABLE_MPMC_NEW_OP_SET_KEY_MIN_LENGTH 7
#define BENCH_HASHTABLE_MPMC_NEW_OP_SET_KEY_MAX_LENGTH 8

// Keys generation
#define TEST_HASHTABLE_MPMC_FUZZY_TESTING_KEYS_CHARACTER_SET \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    '1','2','3','4','5','6','7','8','9','0', '1','2','3','4','5','6','7','8','9','0', \
    '.',',','/','|','\'',';',']','[','<','>','?',':','"','{','}','!','@','$','%','^','&','*','(',')','_','-','=','+','#'

typedef struct benchmark_hashtable_mpmc_set_key benchmark_hashtable_mpmc_set_key_t;
struct benchmark_hashtable_mpmc_set_key {
    union {
        uint128_t _packed;
        struct {
            char *key;
            size_t key_length;
        } data;
    };
};

typedef struct benchmark_hashtable_mpmc_set_keyset_generate_thread_info benchmark_hashtable_mpmc_set_keyset_generate_thread_info_t;
struct benchmark_hashtable_mpmc_set_keyset_generate_thread_info {
    pthread_t thread;
    uint32_t thread_num;
    bool *stop;
    ring_bounded_queue_spsc_uint128_t *keys_queue;
    uint16_t key_min_length;
    uint16_t key_max_length;
};

void *benchmark_hashtable_mpmc_set_keyset_generate_thread_func(
        void *user_data) {
    auto ti = (benchmark_hashtable_mpmc_set_keyset_generate_thread_info_t*)user_data;
    char charset_list[] = {TEST_HASHTABLE_MPMC_FUZZY_TESTING_KEYS_CHARACTER_SET};
    size_t charset_size = sizeof(charset_list);

    thread_current_set_affinity(ti->thread_num);

    do {
        uint16_t key_length = (random_generate() % (ti->key_max_length - ti->key_min_length)) + ti->key_min_length;
        char *key = (char*)xalloc_alloc(key_length + 1);

        for(uint16_t letter_index = 0; letter_index < key_length; letter_index++) {
            key[letter_index] = charset_list[random_generate() % charset_size];
        }
        key[key_length] = 0;

        benchmark_hashtable_mpmc_set_key_t key_to_return = {
                .data = {
                    .key = key,
                    .key_length = key_length,
                },
        };

        uint32_t retries = 10;
        while(retries > 0 && !ring_bounded_queue_spsc_uint128_enqueue(ti->keys_queue, key_to_return._packed)) {
            sched_yield();
            usleep(100000);
            retries--;
        }

        if (retries == 0) {
            xalloc_free(key);
        }

        MEMORY_FENCE_LOAD();
    } while(*ti->stop == false);

    return nullptr;
}

benchmark_hashtable_mpmc_set_key_t* benchmark_hashtable_mpmc_set_keyset_generate(
        uint32_t keys_count,
        uint16_t key_min_length,
        uint16_t key_max_length) {
    auto stop = false;
    auto n_threads = utils_cpu_count();
    auto hashtable_track_dup_keys = hashtable_spsc_new(
            keys_count * 2,
            512,
            true,
            false);

    auto *keyset = (benchmark_hashtable_mpmc_set_key_t*)xalloc_alloc_zero(
            keys_count * sizeof(benchmark_hashtable_mpmc_set_key_t));

    auto threads_info = (benchmark_hashtable_mpmc_set_keyset_generate_thread_info_t*)calloc(
            n_threads, sizeof(benchmark_hashtable_mpmc_set_keyset_generate_thread_info_t));
    if (threads_info == nullptr) {
        perror("calloc");
    }

    // Always leave the hw thread 0 for the processing loop
    thread_current_set_affinity(0);

    // Initialize the producer threads
    for(uint32_t thread_num = 1; thread_num < n_threads; thread_num++) {
        ring_bounded_queue_spsc_uint128_t *keys_queue = ring_bounded_queue_spsc_uint128_init(128 * 1024);

        threads_info[thread_num].thread_num = thread_num;
        threads_info[thread_num].key_min_length = key_min_length;
        threads_info[thread_num].key_max_length = key_max_length;
        threads_info[thread_num].stop = &stop;
        threads_info[thread_num].keys_queue = keys_queue;

        if (pthread_create(
                &threads_info[thread_num].thread,
                nullptr,
                benchmark_hashtable_mpmc_set_keyset_generate_thread_func,
                &threads_info[thread_num]) != 0) {
            perror("pthread_create");
        }
    }

    fprintf(stdout, "Generating keys...");
    fflush(stdout);
    uint32_t processed_keys = 0;
    int prev_perc = 0;
    do {
        // Loop over all the threads to process all the keys in the ring queues
        for(uint32_t thread_num = 1; thread_num < n_threads; thread_num++) {
            bool found = false;
            auto keys_queue = threads_info[thread_num].keys_queue;

            do {
                benchmark_hashtable_mpmc_set_key_t key_to_return = {
                        ._packed = ring_bounded_queue_spsc_uint128_dequeue(keys_queue, &found),
                };

                // Check if a key was available
                if (!found) {
                    continue;
                }

                // Check if the key is unique
                if (hashtable_spsc_op_get_cs(
                        hashtable_track_dup_keys,
                        key_to_return.data.key,
                        key_to_return.data.key_length) != nullptr) {
                    xalloc_free(key_to_return.data.key);
                    continue;
                }

                assert(hashtable_spsc_op_try_set_cs(
                        hashtable_track_dup_keys,
                        key_to_return.data.key,
                        key_to_return.data.key_length,
                        (void*)1));

                keyset[processed_keys]._packed = key_to_return._packed;

                processed_keys++;

                int new_perc = (int)(((float)processed_keys / (float)keys_count) * 100.0f);
                if (new_perc > prev_perc) {
                    prev_perc = new_perc;
                    fprintf(stdout, "%d%% ", new_perc);
                    fflush(stdout);
                }

                if (processed_keys == keys_count) {
                    break;
                }
            } while(found);

            if (processed_keys == keys_count) {
                break;
            }
        }
    } while(processed_keys < keys_count);

    stop = true;
    MEMORY_FENCE_STORE();

    // Wait for all the threads to stop
    for(uint32_t thread_num = 1; thread_num < n_threads; thread_num++) {
        void *ret;
        int res = pthread_join(threads_info[thread_num].thread, &ret);
        if (res != 0) {
            perror("pthread_join");
        }
    }

    // Frees up all the extra keys generated
    for(uint32_t thread_num = 1; thread_num < n_threads; thread_num++) {
        bool found = false;
        auto keys_queue = threads_info[thread_num].keys_queue;

        do {
            benchmark_hashtable_mpmc_set_key_t key_to_return = {
                    ._packed = ring_bounded_queue_spsc_uint128_dequeue(keys_queue, &found),
            };

            // Check if a key was available
            if (!found) {
                continue;
            }

            xalloc_free(key_to_return.data.key);
        } while (found);
    }

    // Free up the memory
    free(threads_info);
    hashtable_spsc_free(hashtable_track_dup_keys);

    fprintf(stdout, " done\n");
    fflush(stdout);

    return keyset;
}

void benchmark_hashtable_mpmc_set_keyset_free(benchmark_hashtable_mpmc_set_key_t *keys) {
    // Frees only the external container, not the actual keys, as they are going to be owned by the hashtable
    xalloc_free(keys);
}

// It is possible to control the amount of threads used for the test tuning the two defines below
#define TEST_THREADS_RANGE_BEGIN (1)
#define TEST_THREADS_RANGE_END (utils_cpu_count())

// These two are kept static and external because
// - Google Benchmark invokes the setup multiple times, once per thread, but it doesn't have an entry point invoked
//   only once to setup non-shared elements for the test
// - the order of the threads started by Google Benchmark is undefined and therefore the code relies on the first thread
//   to setup the keyset and the db and then on having these two static pointers set to NULL to understand when they
//   have been configured by the thread 0. The other threads will wait for these allocations to be made.
volatile static hashtable_mpmc_t *static_hashtable = nullptr;
volatile static benchmark_hashtable_mpmc_set_key_t *static_keyset = nullptr;
volatile static epoch_gc_t *static_epoch_gc_ht_kv = nullptr;
volatile static epoch_gc_t *static_epoch_gc_ht_data = nullptr;

int main(int argc, char** argv) {
    thread_current_set_affinity(0);

    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    ::benchmark::RunSpecifiedBenchmarks();

    return 0;
}

class HashtableOpSetInsertFixture : public benchmark::Fixture {
private:
    hashtable_mpmc_t *_hashtable = nullptr;
    benchmark_hashtable_mpmc_set_key_t *_keyset = nullptr;
    uint64_t _requested_keyset_size = 0;
    epoch_gc_thread_t *_epoch_gc_ht_kv_thread;
    epoch_gc_thread_t *_epoch_gc_ht_data_thread;

public:
    hashtable_mpmc_t *GetHashtable() {
        return this->_hashtable;
    }

    benchmark_hashtable_mpmc_set_key_t *GetKeyset() {
        return this->_keyset;
    }

    epoch_gc_thread_t *GetEpochGcHtKvThread() {
        return this->_epoch_gc_ht_kv_thread;
    }

    epoch_gc_thread_t *GetEpochGcHtDataThread() {
        return this->_epoch_gc_ht_data_thread;
    }

    [[nodiscard]] uint64_t GetRequestedKeysetSize() const {
        return this->_requested_keyset_size;
    }

    void RunningThreadsIncrement() {
        running_threads.fetch_add(1);
    }

    void RunningThreadsDecrement() {
        running_threads.fetch_sub(1);
    }

    void RunningThreadsWait() {
        while(running_threads.load() > 0) {
            usleep(100000);
        }
    }

    void SetUp(const ::benchmark::State& state) override {
        char error_message[150] = {0};

        thread_current_set_affinity(state.thread_index());

        // Calculate the requested keyset size
        double requested_load_factor = (double) state.range(1) / 100.0f;
        this->_requested_keyset_size = (uint64_t) (((double) state.range(0)) * requested_load_factor);

        if (state.thread_index() == 0) {
            uint64_t random_seed = random_generate();

            if (!static_epoch_gc_ht_kv) {
                static_epoch_gc_ht_kv = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
                static_epoch_gc_ht_data = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_DATA);
            }

            // Initialize the key set
            static_keyset = benchmark_hashtable_mpmc_set_keyset_generate(
                    this->_requested_keyset_size,
                    BENCH_HASHTABLE_MPMC_NEW_OP_SET_KEY_MIN_LENGTH,
                    BENCH_HASHTABLE_MPMC_NEW_OP_SET_KEY_MAX_LENGTH);

            MEMORY_FENCE_STORE();

            // Set up the hashtable
            volatile hashtable_mpmc_t *hashtable_temp = hashtable_mpmc_init(
                    state.range(0),
                    state.range(0),
                    HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

            if (!hashtable_temp) {
                sprintf(
                        error_message,
                        "Failed to allocate the hashtable, unable to continue");
                ((::benchmark::State &) state).SkipWithError(error_message);
                return;
            }

            static_hashtable = hashtable_temp;

            MEMORY_FENCE_STORE();
        }

        if (state.thread_index() != 0) {
            while (!static_hashtable) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_keyset) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_epoch_gc_ht_kv) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_epoch_gc_ht_data) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        this->_epoch_gc_ht_kv_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global((epoch_gc_t*)static_epoch_gc_ht_kv, this->_epoch_gc_ht_kv_thread);
        epoch_gc_thread_register_local(this->_epoch_gc_ht_kv_thread);

        this->_epoch_gc_ht_data_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global((epoch_gc_t*)static_epoch_gc_ht_data, this->_epoch_gc_ht_data_thread);
        epoch_gc_thread_register_local(this->_epoch_gc_ht_data_thread);

        this->_hashtable = (hashtable_mpmc_t *)static_hashtable;
        this->_keyset = (benchmark_hashtable_mpmc_set_key_t *)static_keyset;

        this->RunningThreadsIncrement();
    }

    void TearDown(const ::benchmark::State& state) override {
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();

        epoch_gc_thread_terminate(this->_epoch_gc_ht_kv_thread);
        epoch_gc_thread_unregister_local(this->_epoch_gc_ht_kv_thread);

        epoch_gc_thread_terminate(this->_epoch_gc_ht_data_thread);
        epoch_gc_thread_unregister_local(this->_epoch_gc_ht_data_thread);

        this->RunningThreadsDecrement();
        this->RunningThreadsWait();

        if (state.thread_index() == 0) {
            // TODO: free up the epoch gc for ht kv and ht data

            if (this->_hashtable != nullptr) {
                // Free the storage
                hashtable_mpmc_free(this->_hashtable);
            }

            if (this->_keyset != nullptr) {
                // Free the keys
                benchmark_hashtable_mpmc_set_keyset_free(this->_keyset);
            }

            static_hashtable = nullptr;
            static_keyset = nullptr;

            MEMORY_FENCE_STORE();
        }

        this->_hashtable = nullptr;
        this->_keyset = nullptr;
        this->_requested_keyset_size = 0;
        this->_epoch_gc_ht_kv_thread = nullptr;
        this->_epoch_gc_ht_data_thread = nullptr;
    }

    static std::atomic<int> running_threads;
};

std::atomic<int> HashtableOpSetInsertFixture::running_threads(0);

BENCHMARK_DEFINE_F(HashtableOpSetInsertFixture, hashtable_mpmc_op_set_insert)(benchmark::State& state) {
    hashtable_mpmc_t *hashtable;
    uint64_t requested_keyset_size;
    benchmark_hashtable_mpmc_set_key_t *keyset;
    epoch_gc_thread_t *epoch_gc_ht_kv_thread, *epoch_gc_ht_data_thread;
    char error_message[150] = {0};

    thread_current_set_affinity(state.thread_index());

    // Fetch the information from the fixtures needed for the test
    hashtable = this->GetHashtable();
    keyset = this->GetKeyset();
    requested_keyset_size = this->GetRequestedKeysetSize();
    epoch_gc_ht_kv_thread = this->GetEpochGcHtKvThread();
    epoch_gc_ht_data_thread = this->GetEpochGcHtDataThread();

    for (auto _ : state) {
        for(
                uint64_t key_index = state.thread_index();
                key_index < requested_keyset_size;
                key_index += state.threads()) {
            hashtable_mpmc_result_t result;
            bool created_new, value_updated;
            uintptr_t previous_value;

            benchmark::DoNotOptimize((result = hashtable_mpmc_op_set(
                    hashtable,
                    keyset[key_index].data.key,
                    keyset[key_index].data.key_length,
                    (uintptr_t)key_index,
                    &created_new,
                    &value_updated,
                    &previous_value)));

            assert(created_new == true);
            assert(value_updated == true);

#if TEST_VALIDATE_KEYS == 1
            if (result != HASHTABLE_MPMC_RESULT_TRUE) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s> with index <%ld> for the thread <%d>",
                        keyset[key_index].data.key,
                        key_index,
                        state.thread_index());
                state.SkipWithError(error_message);
                break;
            }
#endif

            epoch_gc_thread_set_epoch(
                    epoch_gc_ht_kv_thread,
                    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

            epoch_gc_thread_set_epoch(
                    epoch_gc_ht_data_thread,
                    hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
        }
    }

#if TEST_VALIDATE_KEYS == 1
    for(
            uint64_t key_index = state.thread_index();
            key_index < requested_keyset_size;
            key_index += state.threads()) {
        uintptr_t data = 0;

        hashtable_mpmc_result_t result = hashtable_mpmc_op_get(
                hashtable,
                keyset[key_index].data.key,
                keyset[key_index].data.key_length,
                &data);

        if (result != HASHTABLE_MPMC_RESULT_TRUE) {
            sprintf(
                    error_message,
                    "Unable to find the key <%s (%lu)> with index <%ld> for the thread <%d>",
                    keyset[key_index].data.key,
                    keyset[key_index].data.key_length,
                    key_index,
                    state.thread_index());
            state.SkipWithError(error_message);
            break;
        }

        if (data != key_index) {
            sprintf(
                    error_message,
                    "The key <%s> with index <%ld> for the thread <%d> holds the value <%ld> but the expected one is <%ld>",
                    keyset[key_index].data.key,
                    key_index,
                    state.thread_index(),
                    data,
                    key_index);
            state.SkipWithError(error_message);
            break;
        }

        epoch_gc_thread_set_epoch(
                epoch_gc_ht_kv_thread,
                hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

        epoch_gc_thread_set_epoch(
                epoch_gc_ht_data_thread,
                hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
    }
#endif
}

class HashtableOpSetUpdateFixture : public benchmark::Fixture {
private:
    hashtable_mpmc_t *_hashtable = nullptr;
    benchmark_hashtable_mpmc_set_key_t *_keyset = nullptr;
    uint64_t _requested_keyset_size = 0;
    epoch_gc_thread_t *_epoch_gc_ht_kv_thread;
    epoch_gc_thread_t *_epoch_gc_ht_data_thread;

public:
    static std::atomic<int> running_threads;

    hashtable_mpmc_t *GetHashtable() {
        return this->_hashtable;
    }

    benchmark_hashtable_mpmc_set_key_t *GetKeyset() {
        return this->_keyset;
    }

    epoch_gc_thread_t *GetEpochGcHtKvThread() {
        return this->_epoch_gc_ht_kv_thread;
    }

    epoch_gc_thread_t *GetEpochGcHtDataThread() {
        return this->_epoch_gc_ht_data_thread;
    }

    [[nodiscard]] uint64_t GetRequestedKeysetSize() const {
        return this->_requested_keyset_size;
    }

    void RunningThreadsIncrement() {
        running_threads.fetch_add(1);
    }

    void RunningThreadsDecrement() {
        running_threads.fetch_sub(1);
    }

    void RunningThreadsWait() {
        while(running_threads.load() > 0) {
            usleep(100000);
        }
    }

    void SetUp(const ::benchmark::State& state) override {
        uint64_t random_seed = 0;
        char error_message[150] = {0};

        thread_current_set_affinity(state.thread_index());

        // Calculate the requested keyset size
        double requested_load_factor = (double) state.range(1) / 100.0f;
        this->_requested_keyset_size = (uint64_t) (((double) state.range(0)) * requested_load_factor);

        if (state.thread_index() == 0) {
            random_seed = random_generate();

            if (!static_epoch_gc_ht_kv) {
                static_epoch_gc_ht_kv = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
                static_epoch_gc_ht_data = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_DATA);
            }

            // Initialize the key set
            static_keyset = benchmark_hashtable_mpmc_set_keyset_generate(
                    this->_requested_keyset_size,
                    BENCH_HASHTABLE_MPMC_NEW_OP_SET_KEY_MIN_LENGTH,
                    BENCH_HASHTABLE_MPMC_NEW_OP_SET_KEY_MAX_LENGTH);

            MEMORY_FENCE_STORE();

            // Set up the hashtable
            volatile hashtable_mpmc_t *hashtable_temp = hashtable_mpmc_init(
                    state.range(0),
                    state.range(0),
                    HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

            if (!hashtable_temp) {
                sprintf(
                        error_message,
                        "Failed to allocate the hashtable, unable to continue");
                ((::benchmark::State &) state).SkipWithError(error_message);
                return;
            }

            static_hashtable = hashtable_temp;

            MEMORY_FENCE_STORE();
        }

        if (state.thread_index() != 0) {
            while (!static_hashtable) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_keyset) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_epoch_gc_ht_kv) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }

            while (!static_epoch_gc_ht_data) {
                MEMORY_FENCE_LOAD();
                sched_yield();
            }
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        this->_epoch_gc_ht_kv_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global((epoch_gc_t*)static_epoch_gc_ht_kv, this->_epoch_gc_ht_kv_thread);
        epoch_gc_thread_register_local(this->_epoch_gc_ht_kv_thread);

        this->_epoch_gc_ht_data_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global((epoch_gc_t*)static_epoch_gc_ht_data, this->_epoch_gc_ht_data_thread);
        epoch_gc_thread_register_local(this->_epoch_gc_ht_data_thread);

        for(
                uint64_t key_index = state.thread_index();
                key_index < this->_requested_keyset_size;
                key_index += state.threads()) {
            bool created_new, value_updated;
            uintptr_t previous_value;
            hashtable_mpmc_result_t result = hashtable_mpmc_op_set(
                    (hashtable_mpmc_t*)static_hashtable,
                    mi_strdup(static_keyset[key_index].data.key),
                    static_keyset[key_index].data.key_length,
                    (uintptr_t)key_index,
                    &created_new,
                    &value_updated,
                    &previous_value);

            assert(created_new == true);
            assert(value_updated == true);

            if (result != HASHTABLE_MPMC_RESULT_TRUE) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s (%lu)> with index <%ld> for the thread <%d> during preparation with result <%d>, unable to continue",
                        static_keyset[key_index].data.key,
                        static_keyset[key_index].data.key_length,
                        key_index,
                        state.thread_index(),
                        result);

                ((::benchmark::State &) state).SkipWithError(error_message);
                break;
            }

            epoch_gc_thread_set_epoch(
                    this->_epoch_gc_ht_kv_thread,
                    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

            epoch_gc_thread_set_epoch(
                    this->_epoch_gc_ht_data_thread,
                    hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
        }

        this->_hashtable = (hashtable_mpmc_t *)static_hashtable;
        this->_keyset = (benchmark_hashtable_mpmc_set_key_t *)static_keyset;

        this->RunningThreadsIncrement();
    }

    void TearDown(const ::benchmark::State& state) override {
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();

        epoch_gc_thread_terminate(this->_epoch_gc_ht_kv_thread);
        epoch_gc_thread_unregister_local(this->_epoch_gc_ht_kv_thread);

        epoch_gc_thread_terminate(this->_epoch_gc_ht_data_thread);
        epoch_gc_thread_unregister_local(this->_epoch_gc_ht_data_thread);

        this->RunningThreadsDecrement();
        this->RunningThreadsWait();

        if (state.thread_index() == 0) {
            // TODO: free up the epoch gc for ht kv and ht data

            if (this->_hashtable != nullptr) {
                // Free the storage
                hashtable_mpmc_free(this->_hashtable);
            }

            if (this->_keyset != nullptr) {
                // Free the keys
                benchmark_hashtable_mpmc_set_keyset_free(this->_keyset);
            }

            static_hashtable = nullptr;
            static_keyset = nullptr;

            MEMORY_FENCE_STORE();
        }

        this->_hashtable = nullptr;
        this->_keyset = nullptr;
        this->_requested_keyset_size = 0;
        this->_epoch_gc_ht_kv_thread = nullptr;
        this->_epoch_gc_ht_data_thread = nullptr;
    }
};

std::atomic<int> HashtableOpSetUpdateFixture::running_threads(0);

BENCHMARK_DEFINE_F(HashtableOpSetUpdateFixture, hashtable_mpmc_op_set_update)(benchmark::State& state) {
    hashtable_mpmc_t *hashtable;
    uint64_t requested_keyset_size;
    epoch_gc_thread_t *epoch_gc_ht_kv_thread;
    epoch_gc_thread_t *epoch_gc_ht_data_thread;
    benchmark_hashtable_mpmc_set_key_t *keyset;
    char error_message[150] = {0};

    thread_current_set_affinity(state.thread_index());

    // Fetch the information from the fixtures needed for the test
    hashtable = this->GetHashtable();
    keyset = this->GetKeyset();
    requested_keyset_size = this->GetRequestedKeysetSize();
    epoch_gc_ht_kv_thread = this->GetEpochGcHtKvThread();
    epoch_gc_ht_data_thread = this->GetEpochGcHtDataThread();

    for (auto _ : state) {
        for(
                uint64_t key_index = state.thread_index();
                key_index < requested_keyset_size;
                key_index += state.threads()) {
            bool created_new, value_updated;
            uintptr_t previous_value;
            hashtable_mpmc_result_t result;
            benchmark::DoNotOptimize((result = hashtable_mpmc_op_set(
                    hashtable,
                    keyset[key_index].data.key,
                    keyset[key_index].data.key_length,
                    (uintptr_t)key_index,
                    &created_new,
                    &value_updated,
                    &previous_value)));

            assert(created_new == false);
            assert(value_updated == true);
            assert(previous_value == (uintptr_t)key_index);

#if TEST_VALIDATE_KEYS == 1
            if (result != HASHTABLE_MPMC_RESULT_TRUE) {
                sprintf(
                        error_message,
                        "Unable to set the key <%s> with index <%ld> for the thread <%d> during benchmark with result <%d>, unable to continue",
                        keyset[key_index].data.key,
                        key_index,
                        state.thread_index(),
                        result);
                state.SkipWithError(error_message);
                break;
            }
#endif

            epoch_gc_thread_set_epoch(
                    epoch_gc_ht_kv_thread,
                    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

            epoch_gc_thread_set_epoch(
                    epoch_gc_ht_data_thread,
                    hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
        }
    }


#if TEST_VALIDATE_KEYS == 1
    for(
            uint64_t key_index = state.thread_index();
            key_index < requested_keyset_size;
            key_index += state.threads()) {
        uintptr_t data = 0;

        hashtable_mpmc_result_t result = hashtable_mpmc_op_get(
                hashtable,
                keyset[key_index].data.key,
                keyset[key_index].data.key_length,
                &data);

        if (result != HASHTABLE_MPMC_RESULT_TRUE) {
            sprintf(
                    error_message,
                    "Unable to find the key <%s (%lu)> with index <%ld> for the thread <%d>",
                    keyset[key_index].data.key,
                    keyset[key_index].data.key_length,
                    key_index,
                    state.thread_index());
            state.SkipWithError(error_message);
            break;
        }

        if (data != key_index) {
            sprintf(
                    error_message,
                    "The key <%s> with index <%ld> for the thread <%d> holds the value <%ld> but the expected one is <%ld>",
                    keyset[key_index].data.key,
                    key_index,
                    state.thread_index(),
                    data,
                    key_index);
            state.SkipWithError(error_message);
            break;
        }

        epoch_gc_thread_set_epoch(
                epoch_gc_ht_kv_thread,
                hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

        epoch_gc_thread_set_epoch(
                epoch_gc_ht_data_thread,
                hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
    }
#endif
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b
            ->ArgsProduct({
                                  { 0x0000FFFFu, 0x000FFFFFu, 0x001FFFFFu, 0x007FFFFFu },
                                  { 50, 75 },
                          })
            ->ThreadRange(TEST_THREADS_RANGE_BEGIN, TEST_THREADS_RANGE_END)
            ->Iterations(1)
            ->Repetitions(25)
            ->DisplayAggregatesOnly(false);
}

BENCHMARK_REGISTER_F(HashtableOpSetInsertFixture, hashtable_mpmc_op_set_insert)
        ->Apply(BenchArguments);

BENCHMARK_REGISTER_F(HashtableOpSetUpdateFixture, hashtable_mpmc_op_set_update)
        ->Apply(BenchArguments);
