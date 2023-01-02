/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.init
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>
#include <numa.h>

#include "mimalloc.h"
#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "random.h"
#include "thread.h"
#include "utils_cpu.h"
#include "xalloc.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "spinlock.h"
#include "intrinsics.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_uint128.h"
#include "epoch_gc.h"
#include "data_structures/hashtable_mpmc/hashtable_mpmc.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"

#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
#include "t1ha.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
#include "xxhash.h"
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
#include "hash/hash_crc32c.h"
#else
#error "Unsupported hash algorithm"
#endif

#define TEST_HASHTABLE_MPMC_FUZZY_TESTING_KEYS_CHARACTER_SET \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'q','w','e','r','t','y','u','i','o','p','a','s','d','f','g','h','j','k', 'l','z','x','c','v','b','n','m', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    'Q','W','E','R','T','Y','U','I','O','P','A','S','D','F','G','H','J','K', 'L','Z','X','C','V','B','N','M', \
    '1','2','3','4','5','6','7','8','9','0', '1','2','3','4','5','6','7','8','9','0', \
    '.',',','/','|','\'',';',']','[','<','>','?',':','"','{','}','!','@','$','%','^','&','*','(',')','_','-','=','+','#'

hashtable_mpmc_hash_t test_hashtable_mcmp_support_hash_calculate(
        hashtable_mpmc_key_t *key,
        hashtable_mpmc_key_length_t key_length) {
#if CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2 == 1
    return (hashtable_mpmc_hash_t)t1ha2_atonce(key, key_length, HASHTABLE_MPMC_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3 == 1
    return (hashtable_mpmc_hash_t)XXH3_64bits_withSeed(key, key_length, HASHTABLE_MPMC_HASH_SEED);
#elif CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_CRC32C == 1
    uint32_t crc32 = hash_crc32c(key, key_length, HASHTABLE_MPMC_HASH_SEED);
    hashtable_mpmc_hash_t hash = ((uint64_t)hash_crc32c(key, key_length, crc32) << 32u) | crc32;

    return hash;
#endif
}

#pragma GCC diagnostic ignored "-Wwrite-strings"

enum test_hashtable_mpmc_fuzzy_test_key_status {
    TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_DELETED,
    TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_INSERTED,
};
typedef enum test_hashtable_mpmc_fuzzy_test_key_status test_hashtable_mpmc_fuzzy_test_key_status_t;

typedef struct test_hashtable_mpmc_fuzzy_test_key_status_info test_hashtable_mpmc_fuzzy_test_key_status_info_t;
struct test_hashtable_mpmc_fuzzy_test_key_status_info {
    int locked;
    uint64_t operations;
    test_hashtable_mpmc_fuzzy_test_key_status_t key_status;
    uint64_t retries;
};

typedef struct test_hashtable_mpmc_fuzzy_test_thread_info test_hashtable_mpmc_fuzzy_test_thread_info_t;
struct test_hashtable_mpmc_fuzzy_test_thread_info {
    pthread_t thread;
    uint32_t cpu_index;
    bool_volatile_t *start;
    bool_volatile_t *stop;
    bool_volatile_t stopped;
    epoch_gc_t *epoch_gc_kv;
    epoch_gc_t *epoch_gc_data;
    hashtable_mpmc_t *hashtable;
    char *keys;
    uint32_t keys_count;
    uint32_t key_length_max;
    test_hashtable_mpmc_fuzzy_test_key_status_info_t *keys_status;
    uint64_volatile_t *ops_counter_total;
    uint64_volatile_t *ops_counter_read;
    uint64_volatile_t *ops_counter_insert;
    uint64_volatile_t *ops_counter_update;
    uint64_volatile_t *ops_counter_delete;
};

char* test_hashtable_mpmc_fuzzy_testing_keys_generate(
        uint32_t keys_count,
        uint16_t key_length) {
    char charset_list[] = {TEST_HASHTABLE_MPMC_FUZZY_TESTING_KEYS_CHARACTER_SET};
    size_t charset_size = sizeof(charset_list);

    uint64_t hash_track_dup_keys_capacity = keys_count * 4;
    auto hash_track_dup_keys = (uint64_t*)xalloc_alloc_zero(sizeof(uint64_t) * hash_track_dup_keys_capacity);
    char *keys = (char*)xalloc_alloc_zero(keys_count * (key_length + 1));

    random_init(intrinsics_tsc());
    uint64_t seed = random_generate();

    for(uint32_t key_index = 0; key_index < keys_count; key_index++) {
        uint32_t key_offset = key_index * (key_length + 1);
        char *key = &keys[key_offset];

        uint64_t hash = 0;
        uint64_t hash_track_dup_keys_index = 0;
        do {
            for(uint16_t letter_index = 0; letter_index < key_length; letter_index++) {
                key[letter_index] = charset_list[random_generate() % charset_size];
            }
            key[key_length] = 0;

            hash = t1ha2_atonce(key, key_length, seed);
            hash_track_dup_keys_index = hash % hash_track_dup_keys_capacity;
        } while(hash_track_dup_keys[hash_track_dup_keys_index] == 1);

        hash_track_dup_keys[hash_track_dup_keys_index] = 1;
    }

    xalloc_free(hash_track_dup_keys);

    return keys;
}

void test_hashtable_mpmc_fuzzy_testing_keys_free(char *keys) {
    xalloc_free(keys);
}

uint64_t test_hashtable_mpmc_fuzzy_testing_calc_value_from_key_index(
        uint64_t x) {
    x = (x ^ (x >> 31) ^ (x >> 62)) * UINT64_C(0x319642b2d24d8ec3);
    x = (x ^ (x >> 27) ^ (x >> 54)) * UINT64_C(0x96de1b173f119089);
    x = x ^ (x >> 30) ^ (x >> 60);

    return x;
}

void* test_hashtable_mpmc_fuzzy_testing_thread_func(
        void *user_data) {
    auto ti = (test_hashtable_mpmc_fuzzy_test_thread_info_t*)user_data;

    auto hashtable = ti->hashtable;
    auto keys = ti->keys;
    auto keys_status = ti->keys_status;
    auto keys_count  = ti->keys_count;

    thread_current_set_affinity(ti->cpu_index);

    random_init(intrinsics_tsc());

    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
    hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

    epoch_gc_thread_t *epoch_gc_kv_thread = epoch_gc_thread_init();
    epoch_gc_thread_register_global(ti->epoch_gc_kv, epoch_gc_kv_thread);
    epoch_gc_thread_register_local(epoch_gc_kv_thread);

    epoch_gc_thread_t *epoch_gc_data_thread = epoch_gc_thread_init();
    epoch_gc_thread_register_global(ti->epoch_gc_data, epoch_gc_data_thread);
    epoch_gc_thread_register_local(epoch_gc_data_thread);

    do {
        MEMORY_FENCE_LOAD();
    } while(!*ti->start);

    while(!*ti->stop) {
        uint32_t key_index;
        hashtable_mpmc_result_t result;

        // Try to acquire a key to work on
        int locked_expected, locked_new = 1;
        do {
            locked_expected = 0;
            key_index = random_generate() % keys_count;
        } while (!__atomic_compare_exchange_n(
                &keys_status[key_index].locked,
                &locked_expected,
                locked_new,
                false,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE));

        int action = random_generate() % 300;
        uint32_t key_offset = key_index * (ti->key_length_max + 1);
        char *key = &keys[key_offset];

        __atomic_fetch_add(&keys_status[key_index].operations, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(ti->ops_counter_total, 1, __ATOMIC_RELAXED);

        if (action < 100) {
            // Try to read
            uintptr_t return_value = 0;
            result = hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    strlen(key),
                    &return_value);

            __atomic_fetch_add(ti->ops_counter_read, 1, __ATOMIC_RELAXED);

            if (result == HASHTABLE_MPMC_RESULT_TRY_LATER) {
                keys_status[key_index].retries++;
            } else {
                keys_status[key_index].retries = 0;

                if (keys_status[key_index].key_status == TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_DELETED) {
                    if (result != HASHTABLE_MPMC_RESULT_FALSE) {
                        *ti->stop = true;
                        MEMORY_FENCE_STORE();

                        fprintf(
                                stdout,
                                "[%lu] >   the deleted key <%s (%lu)> has been found\n",
                                intrinsics_tsc(),
                                key,
                                strlen(key));
                        fflush(stdout);
                        FATAL("crash", "crash");
                    }

                    assert(result == HASHTABLE_MPMC_RESULT_FALSE);
                } else {
                    if (result != HASHTABLE_MPMC_RESULT_TRUE) {
                        *ti->stop = true;
                        MEMORY_FENCE_STORE();

                        fprintf(
                                stdout,
                                "[%lu] >   the inserted key <%s (%lu)> can't be found\n",
                                intrinsics_tsc(),
                                key,
                                strlen(key));
                        fflush(stdout);
                        FATAL("crash", "crash");
                    }

                    assert(result == HASHTABLE_MPMC_RESULT_TRUE);
                    assert(return_value == test_hashtable_mpmc_fuzzy_testing_calc_value_from_key_index(key_index));
                }
            }
        } else if (action >= 100 && action < 200) {
            // Try to delete
            result = hashtable_mpmc_op_delete(
                    hashtable,
                    key,
                    strlen(key));

            __atomic_fetch_add(ti->ops_counter_delete, 1, __ATOMIC_RELAXED);

            if (result == HASHTABLE_MPMC_RESULT_TRY_LATER) {
                keys_status[key_index].retries++;
            } else {
                keys_status[key_index].retries = 0;

                if (keys_status[key_index].key_status == TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_DELETED) {
                    if (result != HASHTABLE_MPMC_RESULT_FALSE) {
                        *ti->stop = true;
                        MEMORY_FENCE_STORE();

                        fprintf(
                                stdout,
                                "[%lu] >   the deleted key <%s (%lu)> has been re-deleted\n",
                                intrinsics_tsc(),
                                key,
                                strlen(key));
                        fflush(stdout);
                        FATAL("crash", "crash");
                    }

                    assert(result == HASHTABLE_MPMC_RESULT_FALSE);
                } else {
                    if (result != HASHTABLE_MPMC_RESULT_TRUE) {
                        *ti->stop = true;
                        MEMORY_FENCE_STORE();

                        fprintf(
                                stdout,
                                "[%lu] >   the existing key <%s (%lu)> cannot be deleted\n",
                                intrinsics_tsc(),
                                key,
                                strlen(key));
                        fflush(stdout);
                        FATAL("crash", "crash");
                    }

                    assert(result == HASHTABLE_MPMC_RESULT_TRUE);

                    keys_status[key_index].key_status = TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_DELETED;
                }
            }
        } else {
            // Try to insert or update
            char *key_copy = mi_strdup(key);
            bool return_created_new = false;
            bool return_value_updated = false;
            uintptr_t return_previous_value = 0;

            result = hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    strlen(key_copy),
                    test_hashtable_mpmc_fuzzy_testing_calc_value_from_key_index(key_index),
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value);

            if (result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING || result == HASHTABLE_MPMC_RESULT_TRY_LATER) {
                xalloc_free(key_copy);
                keys_status[key_index].retries++;
            } else {
                keys_status[key_index].retries = 0;
            }

            if (result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING) {
                if (hashtable_mpmc_upsize_is_allowed(hashtable)) {
                    hashtable_mpmc_upsize_prepare(hashtable);
                } else {
                    fprintf(stdout, ">   hashtable maximum size reached, can't upsize\n");
                    fflush(stdout);
                    FATAL("crash", "crash");
                }
            } else if (result != HASHTABLE_MPMC_RESULT_TRY_LATER) {
                if (keys_status[key_index].key_status == TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_DELETED) {
                    __atomic_fetch_add(ti->ops_counter_insert, 1, __ATOMIC_RELAXED);
                } else {
                    __atomic_fetch_add(ti->ops_counter_update, 1, __ATOMIC_RELAXED);
                }

                assert(result == HASHTABLE_MPMC_RESULT_TRUE);

                if (keys_status[key_index].key_status == TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_DELETED) {
                    if (return_created_new != true) {
                        *ti->stop = true;
                        MEMORY_FENCE_STORE();

                        fprintf(
                                stdout,
                                "[%lu] >   the deleted key <%s (%lu)> has been updated instead of inserted\n",
                                intrinsics_tsc(),
                                key,
                                strlen(key));
                        fflush(stdout);
                        FATAL("crash", "crash");
                    }

                    assert(return_created_new == true);
                    assert(return_value_updated == true);
                    assert(return_previous_value == 0);
                } else {
                    if (return_created_new != false) {
                        *ti->stop = true;
                        MEMORY_FENCE_STORE();

                        fprintf(
                                stdout,
                                "[%lu] >   the existing key <%s (%lu)> has been reinserted instead of updated\n",
                                intrinsics_tsc(),
                                key,
                                strlen(key));
                        fflush(stdout);
                        FATAL("crash", "crash");
                    }

                    assert(return_created_new == false);
                    assert(return_value_updated == true);
                    assert(return_previous_value == test_hashtable_mpmc_fuzzy_testing_calc_value_from_key_index(key_index));
                }

                keys_status[key_index].key_status = TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_INSERTED;
            }
        }

        if (keys_status[key_index].retries == 25) {
            fprintf(
                    stdout,
                    "[%lu] >   operations on the key <%s (%lu)> have been retried too many times (<%ld>), aborting\n",
                    intrinsics_tsc(),
                    key,
                    strlen(key),
                    keys_status[key_index].retries);
            fflush(stdout);
            FATAL("crash", "crash");
        }

        // Unlock the key status
        keys_status[key_index].locked = 0;
        MEMORY_FENCE_STORE();

        if (hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING) {
            hashtable_mpmc_upsize_migrate_block(hashtable);
        }

        epoch_gc_thread_set_epoch(
                epoch_gc_kv_thread,
                hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

        epoch_gc_thread_set_epoch(
                epoch_gc_data_thread,
                hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
    }

    ti->stopped = true;
    MEMORY_FENCE_STORE();

    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
    hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();

    epoch_gc_thread_collect_all(epoch_gc_kv_thread);
    epoch_gc_thread_collect_all(epoch_gc_data_thread);

    epoch_gc_thread_terminate(epoch_gc_kv_thread);
    epoch_gc_thread_unregister_local(epoch_gc_kv_thread);

    epoch_gc_thread_terminate(epoch_gc_data_thread);
    epoch_gc_thread_unregister_local(epoch_gc_data_thread);

    return nullptr;
}

void test_hashtable_mpmc_fuzzy_testing_run(
        hashtable_mpmc_t *hashtable,
        char *keys,
        uint32_t keys_count,
        uint32_t key_length_max,
        uint32_t threads,
        uint32_t duration) {
    uint64_t ops_counter_total = 0, ops_counter_read = 0, ops_counter_insert = 0, ops_counter_update = 0,
            ops_counter_delete = 0;
    timespec_t start_time, current_time, diff_time;
    bool start = false;
    bool stop = false;

    auto ti_list = (test_hashtable_mpmc_fuzzy_test_thread_info_t *) xalloc_alloc_zero(
            sizeof(test_hashtable_mpmc_fuzzy_test_thread_info_t) * threads);

    auto keys_info = (test_hashtable_mpmc_fuzzy_test_key_status_info_t *) xalloc_alloc_zero(
            sizeof(test_hashtable_mpmc_fuzzy_test_key_status_info_t) * keys_count);

    epoch_gc_t *epoch_gc_kv = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
    epoch_gc_t *epoch_gc_data = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_DATA);

    // Sets up and starts the threads - Begin
    {
        for (int i = 0; i < threads; i++) {
            auto ti = &ti_list[i];

            ti->cpu_index = i;
            ti->start = &start;
            ti->stop = &stop;
            ti->stopped = false;
            ti->hashtable = hashtable;
            ti->keys = keys;
            ti->keys_info = keys_info;
            ti->keys_count = keys_count;
            ti->key_length_max = key_length_max;
            ti->ops_counter_total = &ops_counter_total;
            ti->ops_counter_read = &ops_counter_read;
            ti->ops_counter_insert = &ops_counter_insert;
            ti->ops_counter_update = &ops_counter_update;
            ti->ops_counter_delete = &ops_counter_delete;
            ti->epoch_gc_kv = epoch_gc_kv;
            ti->epoch_gc_data = epoch_gc_data;

            if (pthread_create(
                    &ti->thread,
                    nullptr,
                    test_hashtable_mpmc_fuzzy_testing_thread_func,
                    ti) != 0) {
                REQUIRE(false);

                usleep(10000);
            }
        }
    }
    // Sets up and starts the threads - End

    // Starts the threads and wait for the requested duration - Begin
    {
        start = true;
        MEMORY_FENCE_STORE();

        clock_monotonic(&start_time);

        do {
            clock_monotonic(&current_time);
            sched_yield();

            clock_diff(&start_time, &current_time, &diff_time);
        } while (diff_time.tv_sec < duration);
    }
    // Starts the threads and wait for the requested duration - End

    // Wait for all the threads to stop - Begin
    {
        stop = true;
        MEMORY_FENCE_STORE();

        bool stopped;
        do {
            stopped = true;
            sched_yield();

            // wait for all the threads to stop
            for (int i = 0; i < threads && stopped; i++) {
                MEMORY_FENCE_LOAD();
                if (!ti_list[i].stopped) {
                    stopped = false;
                    continue;
                }
            }
        } while (!stopped);
    }
    // Wait for all the threads to stop - End

    // Wait for all the threads to reach the end of the thread func - Begin
    {
        for(int i = 0; i < threads; i++) {
            void *result;
            auto ti = &ti_list[i];
            pthread_join(ti->thread, &result);
        }
    }
    // Wait for all the threads to reach the end of the thread func - End

    // Keys Validation - Begin
    {
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        epoch_gc_thread_t *epoch_gc_kv_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc_kv, epoch_gc_kv_thread);
        epoch_gc_thread_register_local(epoch_gc_kv_thread);

        epoch_gc_thread_t *epoch_gc_data_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc_data, epoch_gc_data_thread);
        epoch_gc_thread_register_local(epoch_gc_data_thread);

        fprintf(stdout, "[%lu] > VALIDATING INSERTIONS\n", intrinsics_tsc());
        for (uint32_t key_index = 0; key_index < keys_count; key_index++) {
            test_hashtable_mpmc_fuzzy_test_key_status_info_t *key_info = &keys_info[key_index];

            if (key_info->key_status == TEST_HASHTABLE_MPMC_FUZZY_TEST_KEY_STATUS_DELETED) {
                continue;
            }

            uint32_t key_offset = key_index * (key_length_max + 1);
            char *key = &keys[key_offset];

            // Try to read
            uintptr_t return_value = 0;
            hashtable_mpmc_result_t result = hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    strlen(key),
                    &return_value);

            epoch_gc_thread_set_epoch(
                    epoch_gc_kv_thread,
                    hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

            epoch_gc_thread_set_epoch(
                    epoch_gc_data_thread,
                    hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());

            if (result != HASHTABLE_MPMC_RESULT_TRUE) {
                fprintf(
                        stdout,
                        "[%lu] >   the inserted key <%s (%lu)> n. <%u> can't be found\n",
                        intrinsics_tsc(),
                        key,
                        strlen(key),
                        key_index);
                fflush(stdout);
                FATAL("crash", "crash");
            }

            assert(result == HASHTABLE_MPMC_RESULT_TRUE);
            assert(return_value == test_hashtable_mpmc_fuzzy_testing_calc_value_from_key_index(key_index));
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();

        epoch_gc_thread_collect_all(epoch_gc_kv_thread);
        epoch_gc_thread_collect_all(epoch_gc_data_thread);

        epoch_gc_thread_terminate(epoch_gc_kv_thread);
        epoch_gc_thread_unregister_local(epoch_gc_kv_thread);

        epoch_gc_thread_terminate(epoch_gc_data_thread);
        epoch_gc_thread_unregister_local(epoch_gc_data_thread);
    }
    // Keys Validation - End

    fprintf(stdout, "[%lu] > SUMMARY\n", intrinsics_tsc());
    fprintf(stdout, "[%lu] >   ops_counter_total = %lu\n", intrinsics_tsc(), ops_counter_total);
    fprintf(stdout, "[%lu] >   ops_counter_read = %lu\n", intrinsics_tsc(), ops_counter_read);
    fprintf(stdout, "[%lu] >   ops_counter_insert = %lu\n", intrinsics_tsc(), ops_counter_insert);
    fprintf(stdout, "[%lu] >   ops_counter_update = %lu\n", intrinsics_tsc(), ops_counter_update);
    fprintf(stdout, "[%lu] >   ops_counter_delete = %lu\n", intrinsics_tsc(), ops_counter_delete);
    fflush(stdout);

    hashtable_mpmc_free(hashtable);
    xalloc_free(ti_list);
    xalloc_free(keys_status);

    epoch_gc_free(epoch_gc_kv);
    epoch_gc_free(epoch_gc_data);
}

TEST_CASE("data_structures/hashtable_mpmc/hashtable_mpmc.c", "[data_structures][hashtable_mpmc]") {
    char *key = "This Is A Key - not embedded";
    char *key_different_case = "THIS IS A KEY - NOT EMBEDDED";
    hashtable_mpmc_key_length_t key_length = strlen(key);
    hashtable_mpmc_hash_t key_hash = test_hashtable_mcmp_support_hash_calculate(key, key_length);
    hashtable_mpmc_hash_half_t key_hash_half = key_hash & 0XFFFFFFFF;

    char *key2 = "This Is Another Key - not embedded";
    hashtable_mpmc_key_length_t key2_length = strlen(key2);
    hashtable_mpmc_hash_t key2_hash = test_hashtable_mcmp_support_hash_calculate(key2, key2_length);
    hashtable_mpmc_hash_half_t key2_hash_half = key2_hash & 0XFFFFFFFF;

    char *key_embed = "embedded key";
    hashtable_mpmc_key_length_t key_embed_length = strlen(key_embed);
    hashtable_mpmc_hash_t key_embed_hash = test_hashtable_mcmp_support_hash_calculate(
            key_embed,
            key_embed_length);
    hashtable_mpmc_hash_half_t key_embed_hash_half = key_embed_hash & 0XFFFFFFFF;

    SECTION("hashtable_mpmc_data_init") {
        hashtable_mpmc_data_t *hashtable_data = hashtable_mpmc_data_init(10);

        REQUIRE(hashtable_data != nullptr);
        REQUIRE(hashtable_data->buckets_count == 16);
        REQUIRE(hashtable_data->buckets_count_mask == 16 - 1);
        REQUIRE(hashtable_data->buckets_count_real == 16 + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE);
        REQUIRE(hashtable_data->struct_size ==
                sizeof(hashtable_mpmc_data_t) +
                (sizeof(hashtable_mpmc_bucket_t) * (16 + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE)));

        hashtable_mpmc_data_free(hashtable_data);
    }

    SECTION("hashtable_mpmc_init") {
        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(10, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        REQUIRE(hashtable->data != nullptr);
        REQUIRE(hashtable->data->buckets_count == 16);
        REQUIRE(hashtable->data->buckets_count_mask == 16 - 1);
        REQUIRE(hashtable->data->buckets_count_real == 16 + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE);
        REQUIRE(hashtable->data->struct_size ==
                sizeof(hashtable_mpmc_data_t) +
                (sizeof(hashtable_mpmc_bucket_t) * (16 + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE)));
        REQUIRE(hashtable->buckets_count_max == 32);
        REQUIRE(hashtable->upsize_preferred_block_size == HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
        REQUIRE(hashtable->upsize.from == nullptr);
        REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_NOT_UPSIZING);

        hashtable_mpmc_free(hashtable);
    }

    SECTION("hashtable_mpmc_support_hash_half") {
        REQUIRE(hashtable_mpmc_support_hash_half(key_hash) == key_hash_half);
    }

    SECTION("hashtable_mpmc_support_bucket_index_from_hash") {
        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(10, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        REQUIRE(hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash) ==
                ((key_hash >> 32) & hashtable->data->buckets_count_mask));

        hashtable_mpmc_free(hashtable);
    }

    SECTION("hashtable_mpmc_support_find_bucket_and_key_value") {
        hashtable_mpmc_bucket_t return_bucket;
        hashtable_mpmc_bucket_index_t return_bucket_index;

        char *key_copy = mi_strdup(key);

        auto key_value = (hashtable_mpmc_data_key_value_t *) xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        key_value->key.external.key = key_copy;
        key_value->key.external.key_length = key_length;
        key_value->value = 12345;
        key_value->hash = key_hash;
        key_value->key_is_embedded = false;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(
                16,
                32,
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index_max =
                hashtable_key_bucket_index + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
        hashtable_mpmc_bucket_index_t hashtable_key_embed_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_embed_hash);

        SECTION("bucket found") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index));
            REQUIRE(return_bucket._packed == hashtable->data->buckets[hashtable_key_bucket_index]._packed);
            REQUIRE(return_bucket_index == hashtable_key_bucket_index);
        }

        SECTION("bucket found - temporary") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_volatile_t *) ((uintptr_t) (key_value) | 0x01);

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    true,
                    &return_bucket,
                    &return_bucket_index));
            REQUIRE(return_bucket._packed == hashtable->data->buckets[hashtable_key_bucket_index]._packed);
            REQUIRE(return_bucket_index == hashtable_key_bucket_index);
        }

        SECTION("bucket found - embedded key") {
            key_value->key_is_embedded = true;
            strncpy(key_value->key.embedded.key, key_embed, key_embed_length);
            key_value->key.embedded.key_length = key_embed_length;
            key_value->hash = key_embed_hash;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.hash_half = key_embed_hash_half;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_embed_hash,
                    key_embed_hash_half,
                    key_embed,
                    key_embed_length,
                    false,
                    &return_bucket,
                    &return_bucket_index));
            REQUIRE(return_bucket._packed == hashtable->data->buckets[hashtable_key_embed_bucket_index]._packed);
            REQUIRE(return_bucket_index == hashtable_key_embed_bucket_index);
        }

        SECTION("bucket not found - not existing") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key2_hash,
                    key2_hash_half,
                    key2,
                    key2_length,
                    false,
                    &return_bucket,
                    &return_bucket_index) == false);
        }

        SECTION("bucket not found - temporary") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_volatile_t *) ((uintptr_t) (key_value) | 0x01);

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index) == false);
        }

        SECTION("bucket not found - not in range") {
            hashtable->data->buckets[hashtable_key_bucket_index_max].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index_max].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index_max].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index) == false);
        }

        SECTION("bucket not found - hashtable empty") {
            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index) == false);
        }

        hashtable_mpmc_free(hashtable);
    }

    SECTION("hashtable_mpmc_support_acquire_empty_bucket_for_insert") {
        hashtable_mpmc_bucket_t bucket_to_overwrite;
        hashtable_mpmc_bucket_index_t found_bucket_index;
        hashtable_mpmc_data_key_value_t *new_key_value = nullptr;
        char *key_copy = mi_strdup(key);
        char *value1 = "first value";

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(
                16,
                32,
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index_max =
                hashtable_key_bucket_index + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;
        hashtable_mpmc_bucket_index_t hashtable_key_embed_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_embed_hash);

        SECTION("bucket found") {
            hashtable_mpmc_result_t result = hashtable_mpmc_support_acquire_empty_bucket_for_insert(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &new_key_value,
                    &bucket_to_overwrite,
                    &found_bucket_index);

            REQUIRE(result == HASHTABLE_MPMC_RESULT_TRUE);
        }

        SECTION("bucket not found - nothing in range") {
            hashtable_mpmc_data_t *hashtable_mpmc_data_current = hashtable->data;
            for (
                    hashtable_mpmc_bucket_index_t index = hashtable_key_bucket_index;
                    index < hashtable_key_bucket_index_max;
                    index++) {
                hashtable->data->buckets[index].data.hash_half = 12345;
                hashtable->data->buckets[index].data.key_value =
                        (hashtable_mpmc_data_key_value_t*)(12345 & HASHTABLE_MPMC_POINTER_TAG_MASK_INVERTED);
            }

            hashtable_mpmc_result_t result = hashtable_mpmc_support_acquire_empty_bucket_for_insert(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &new_key_value,
                    &bucket_to_overwrite,
                    &found_bucket_index);

            REQUIRE(result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING);

            // Hashes have to be set back to zero before freeing up the hashtable otherwise hashtable_mpmc_free will try
            // to free the bucket, which is invalid, and will cause a segfault
            for (
                    hashtable_mpmc_bucket_index_t index = hashtable_key_bucket_index;
                    index < hashtable_key_bucket_index_max;
                    index++) {
                hashtable_mpmc_data_current->buckets[index].data.hash_half = 0;
                hashtable->data->buckets[index].data.key_value = nullptr;
            }

            xalloc_free(new_key_value);
        }

        SECTION("bucket not found - hashtable full") {
            hashtable_mpmc_data_t *hashtable_mpmc_data_current = hashtable->data;
            for (
                    hashtable_mpmc_bucket_index_t index = 0;
                    index < hashtable->data->buckets_count_real;
                    index++) {
                hashtable->data->buckets[index].data.hash_half = 12345;
                hashtable->data->buckets[index].data.key_value =
                        (hashtable_mpmc_data_key_value_t*)(12345 & HASHTABLE_MPMC_POINTER_TAG_MASK_INVERTED);
            }

            hashtable_mpmc_result_t result = hashtable_mpmc_support_acquire_empty_bucket_for_insert(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &new_key_value,
                    &bucket_to_overwrite,
                    &found_bucket_index);

            REQUIRE(result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING);

            // Hashes have to be set back to zero before freeing up the hashtable otherwise hashtable_mpmc_free will try
            // to free the bucket, which is invalid, and will cause a segfault
            for (
                    hashtable_mpmc_bucket_index_t index = 0;
                    index < hashtable->data->buckets_count_real;
                    index++) {
                hashtable_mpmc_data_current->buckets[index].data.hash_half = 0;
                hashtable->data->buckets[index].data.key_value = nullptr;
            }

            xalloc_free(new_key_value);
        }

        hashtable_mpmc_free(hashtable);
    }

    SECTION("hashtable_mpmc_support_validate_insert") {
        hashtable_mpmc_bucket_t bucket_to_overwrite;
        hashtable_mpmc_bucket_index_t found_bucket_index;
        hashtable_mpmc_data_key_value_t *new_key_value = nullptr;
        char *key_copy = mi_strdup(key);

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(
                16,
                32,
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);

        hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
        hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                (hashtable_mpmc_data_key_value_volatile_t *) (HASHTABLE_MPMC_POINTER_TAG_TEMPORARY);

        SECTION("insert validated") {
            hashtable_mpmc_result_t result = hashtable_mpmc_support_validate_insert(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key_copy,
                    key_length,
                    hashtable_key_bucket_index);

            REQUIRE(result == HASHTABLE_MPMC_RESULT_TRUE);
        }

        SECTION("insert not validated") {
            auto key_value = (hashtable_mpmc_data_key_value_t *) xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
            key_value->key.external.key = key_copy;
            key_value->key.external.key_length = key_length;
            key_value->value = 12345;
            key_value->hash = key_hash;
            key_value->key_is_embedded = false;

            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.key_value =
                    (hashtable_mpmc_data_key_value_volatile_t *) ((uintptr_t) key_value |
                                                                  HASHTABLE_MPMC_POINTER_TAG_TEMPORARY);

            hashtable_mpmc_result_t result = hashtable_mpmc_support_validate_insert(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key_copy,
                    key_length,
                    hashtable_key_bucket_index);

            REQUIRE(result == HASHTABLE_MPMC_RESULT_FALSE);

            hashtable->data->buckets[hashtable_key_bucket_index + 1]._packed = 0;
            xalloc_free(key_value);
        }

        hashtable->data->buckets[hashtable_key_bucket_index]._packed = 0;
        hashtable_mpmc_free(hashtable);
        xalloc_free(key_copy);
    }

    SECTION("hashtable_mpmc_upsize_is_allowed") {
        SECTION("not allowed") {
            hashtable_mpmc_t *hashtable = hashtable_mpmc_init(
                    32,
                    32,
                    HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
            REQUIRE(hashtable_mpmc_upsize_is_allowed(hashtable) == false);

            hashtable_mpmc_free(hashtable);
        }

        SECTION("allowed") {
            hashtable_mpmc_t *hashtable = hashtable_mpmc_init(
                    16,
                    32,
                    HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
            REQUIRE(hashtable_mpmc_upsize_is_allowed(hashtable) == true);

            hashtable_mpmc_free(hashtable);
        }
    }

    SECTION("hashtable_mpmc_upsize_prepare") {
        hashtable_mpmc_t *hashtable_small = hashtable_mpmc_init(
                16,
                32,
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        hashtable_mpmc_t *hashtable_large = hashtable_mpmc_init(
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE * 16,
                UINT64_MAX,
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        SECTION("preparation successful") {
            REQUIRE(hashtable_mpmc_upsize_prepare(hashtable_small));

            REQUIRE(hashtable_small->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING);
            REQUIRE(hashtable_small->upsize.from != NULL);
            REQUIRE(hashtable_small->upsize.remaining_blocks == 1);
            REQUIRE(hashtable_small->upsize.total_blocks == 1);
            REQUIRE(hashtable_small->upsize.threads_count == 0);
            REQUIRE(hashtable_small->upsize.block_size == 272);
        };

        SECTION("preparation successful - multiple blocks") {
            REQUIRE(hashtable_mpmc_upsize_prepare(hashtable_large));

            REQUIRE(hashtable_large->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING);
            REQUIRE(hashtable_large->upsize.from != NULL);
            REQUIRE(hashtable_large->upsize.remaining_blocks == 17);
            REQUIRE(hashtable_large->upsize.total_blocks == 17);
            REQUIRE(hashtable_large->upsize.threads_count == 0);
            REQUIRE(hashtable_large->upsize.block_size == 15436);
        };

        SECTION("preparation failed - already upsizing") {
            hashtable_small->upsize.status = HASHTABLE_MPMC_STATUS_UPSIZING;

            REQUIRE(!hashtable_mpmc_upsize_prepare(hashtable_small));
        };

        SECTION("preparation failed - preparing to upsize") {
            hashtable_small->upsize.status = HASHTABLE_MPMC_STATUS_PREPARE_FOR_UPSIZE;

            REQUIRE(!hashtable_mpmc_upsize_prepare(hashtable_small));
        };

        hashtable_mpmc_free(hashtable_small);
        hashtable_mpmc_free(hashtable_large);
    }

    SECTION("hashtable_mpmc_op_set") {
        char *value1 = "first value";
        char *value2 = "second value";
        char *key_copy = mi_strdup(key);
        char *key_copy2 = mi_strdup(key);
        char *key_embed_copy = mi_strdup(key_embed);
        char *key2_copy = mi_strdup(key2);
        bool return_created_new = false;
        bool return_value_updated = false;
        uintptr_t return_previous_value = 0;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(16, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);
        hashtable_mpmc_bucket_index_t hashtable_key2_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key2_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_embed_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_embed_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index_max =
                hashtable_key_bucket_index + HASHTABLE_MPMC_LINEAR_SEARCH_RANGE;

        epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc, epoch_gc_thread);
        epoch_gc_thread_register_local(epoch_gc_thread);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        SECTION("value set - insert") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t) value1);
        }

        SECTION("value set - insert - tombstone") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_t *) HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE;

            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t) value1);
        }

        SECTION("value set - insert - embedded key") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_embed_copy,
                    key_embed_length,
                    (uintptr_t) value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.hash_half == key_embed_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->key_is_embedded == true);
            REQUIRE(strncmp(
                    (char *) hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->key.embedded.key,
                    key_embed,
                    key_embed_length) == 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->key.embedded.key_length ==
                    key_embed_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->hash == key_embed_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value->value ==
                    (uintptr_t) value1);
        }

        SECTION("value set - update") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy2,
                    key_length,
                    (uintptr_t) value2,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(!return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(return_previous_value == (uintptr_t) value1);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t) value2);
        }

        SECTION("value set - insert two keys") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key2_copy,
                    key2_length,
                    (uintptr_t) value2,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == key_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key == key_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->key.external.key_length ==
                    key_length);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->hash == key_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value->value == (uintptr_t) value1);

            REQUIRE(return_created_new);
            REQUIRE(return_value_updated);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index]._packed != 0);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value != nullptr);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.hash_half == key2_hash_half);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->key_is_embedded == false);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->key.external.key ==
                    key2_copy);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->key.external.key_length ==
                    key2_length);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->hash == key2_hash);
            REQUIRE(hashtable->data->buckets[hashtable_key2_bucket_index].data.key_value->value == (uintptr_t) value2);
        }

        SECTION("value set - upsize") {
            hashtable_mpmc_data_t *hashtable_mpmc_data_current = hashtable->data;
            for (
                    hashtable_mpmc_bucket_index_t index = hashtable_key_bucket_index;
                    index < hashtable_key_bucket_index_max;
                    index++) {
                hashtable->data->buckets[index].data.hash_half = 12345;
            }

            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING);

            // Hashes have to be set back to zero before freeing up the hashtable
            for (
                    hashtable_mpmc_bucket_index_t index = hashtable_key_bucket_index;
                    index < hashtable_key_bucket_index_max;
                    index++) {
                hashtable_mpmc_data_current->buckets[index].data.hash_half = 0;
            }

            REQUIRE(hashtable->upsize.from == nullptr);
            REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_NOT_UPSIZING);
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();
        hashtable_mpmc_free(hashtable);

        epoch_gc_thread_unregister_local(epoch_gc_thread);
        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_thread_free(epoch_gc_thread);
        epoch_gc_free(epoch_gc);
    }

    SECTION("hashtable_mpmc_op_get") {
        char *key_copy = mi_strdup(key);
        uintptr_t return_value = 0;

        auto key_value = (hashtable_mpmc_data_key_value_t *) xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        key_value->key.external.key = key_copy;
        key_value->key.external.key_length = key_length;
        key_value->value = 12345;
        key_value->hash = key_hash;
        key_value->key_is_embedded = false;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(16, 32, HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);
        hashtable_mpmc_bucket_index_t hashtable_key_embed_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_embed_hash);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        SECTION("value found - existing key") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(return_value == 12345);
        }

        SECTION("value found - embedded key") {
            key_value->key_is_embedded = true;
            strncpy(key_value->key.embedded.key, key_embed, key_embed_length);
            key_value->key.embedded.key_length = key_embed_length;
            key_value->hash = key_embed_hash;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.hash_half = key_embed_hash_half;
            hashtable->data->buckets[hashtable_key_embed_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key_embed,
                    key_embed_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(return_value == 12345);
        }

        SECTION("value found - after tombstone key") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_t *) HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(return_value == 12345);
        }

        SECTION("value not found - existing key with different case") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key_different_case,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        SECTION("value not found - non-existent key") {
            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        SECTION("value not found - temporary") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value =
                    (hashtable_mpmc_data_key_value_volatile_t *) ((uintptr_t) (key_value) | 0x01);

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        SECTION("value not found - empty (without tombstone) before") {
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index + 1].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_get(
                    hashtable,
                    key,
                    key_length,
                    &return_value) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();
        hashtable_mpmc_free(hashtable);
    }

    SECTION("hashtable_mpmc_op_delete") {
        char *key_copy = mi_strdup(key);

        auto key_value = (hashtable_mpmc_data_key_value_t *) xalloc_alloc(sizeof(hashtable_mpmc_data_key_value_t));
        key_value->key.external.key = key_copy;
        key_value->key.external.key_length = key_length;
        key_value->value = 12345;
        key_value->hash = key_hash;
        key_value->key_is_embedded = false;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(
                16,
                32,
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
        epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc, epoch_gc_thread);
        epoch_gc_thread_register_local(epoch_gc_thread);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);

        SECTION("value deleted - existing key") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_delete(hashtable, key, key_length) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id == 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half == 0);
            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index].data.key_value ==
                    (hashtable_mpmc_data_key_value_t *) HASHTABLE_MPMC_POINTER_TAG_TOMBSTONE);

            epoch_gc_thread_advance_epoch_tsc(epoch_gc_thread);
            REQUIRE(epoch_gc_thread_collect_all(epoch_gc_thread) == 1);
        }

        SECTION("value not deleted - existing key with different case") {
            hashtable->data->buckets[hashtable_key_bucket_index].data.transaction_id.id = 0;
            hashtable->data->buckets[hashtable_key_bucket_index].data.hash_half = key_hash_half;
            hashtable->data->buckets[hashtable_key_bucket_index].data.key_value = key_value;

            REQUIRE(hashtable_mpmc_op_delete(
                    hashtable,
                    key_different_case,
                    key_length) == HASHTABLE_MPMC_RESULT_FALSE);

            REQUIRE(hashtable->data->buckets[hashtable_key_bucket_index]._packed != 0);
        }

        SECTION("value not deleted - non-existent key") {
            REQUIRE(hashtable_mpmc_op_delete(
                    hashtable,
                    key,
                    key_length) == HASHTABLE_MPMC_RESULT_FALSE);
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();
        hashtable_mpmc_free(hashtable);

        epoch_gc_thread_unregister_local(epoch_gc_thread);
        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_thread_free(epoch_gc_thread);
        epoch_gc_free(epoch_gc);
    }

    SECTION("hashtable_mpmc_upsize_migrate_bucket") {
        uintptr_t value1 = 12345;
        uintptr_t value2 = 54321;
        auto key_copy = mi_strdup(key);
        auto key2_copy = mi_strdup(key2);
        bool return_created_new = false;
        bool return_value_updated = false;
        hashtable_mpmc_bucket_t return_bucket, return_bucket2, return_bucket_orig, return_bucket_orig2;
        hashtable_mpmc_bucket_index_t return_bucket_index, return_bucket_index2;
        uintptr_t return_previous_value = 0, return_value = 0;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(
                16,
                32,
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
        epoch_gc_t *epoch_gc = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
        epoch_gc_thread_t *epoch_gc_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc, epoch_gc_thread);
        epoch_gc_thread_register_local(epoch_gc_thread);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        hashtable_mpmc_bucket_index_t hashtable_key_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key_hash);
        hashtable_mpmc_bucket_index_t hashtable_key2_bucket_index =
                hashtable_mpmc_support_bucket_index_from_hash(hashtable->data, key2_hash);

        SECTION("migrate 1 bucket") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket_orig,
                    &return_bucket_index));

            REQUIRE(hashtable_mpmc_upsize_prepare(hashtable));

            REQUIRE(hashtable_mpmc_upsize_migrate_bucket(
                    hashtable->upsize.from,
                    hashtable->data,
                    hashtable_key_bucket_index));

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index));

            REQUIRE(return_bucket.data.key_value == return_bucket_orig.data.key_value);
            REQUIRE(return_bucket.data.hash_half == return_bucket_orig.data.hash_half);
        }

        SECTION("migrate 2 buckets") {
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key_copy,
                    key_length,
                    (uintptr_t) value1,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);
            REQUIRE(hashtable_mpmc_op_set(
                    hashtable,
                    key2_copy,
                    key2_length,
                    (uintptr_t) value2,
                    &return_created_new,
                    &return_value_updated,
                    &return_previous_value) == HASHTABLE_MPMC_RESULT_TRUE);

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket_orig,
                    &return_bucket_index));
            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key2_hash,
                    key2_hash_half,
                    key2,
                    key2_length,
                    false,
                    &return_bucket_orig2,
                    &return_bucket_index2));

            REQUIRE(hashtable_mpmc_upsize_prepare(hashtable));

            REQUIRE(hashtable_mpmc_upsize_migrate_bucket(
                    hashtable->upsize.from,
                    hashtable->data,
                    hashtable_key_bucket_index));
            REQUIRE(hashtable_mpmc_upsize_migrate_bucket(
                    hashtable->upsize.from,
                    hashtable->data,
                    hashtable_key2_bucket_index));

            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key_hash,
                    key_hash_half,
                    key,
                    key_length,
                    false,
                    &return_bucket,
                    &return_bucket_index));
            REQUIRE(hashtable_mpmc_support_find_bucket_and_key_value(
                    hashtable->data,
                    key2_hash,
                    key2_hash_half,
                    key2,
                    key2_length,
                    false,
                    &return_bucket2,
                    &return_bucket_index));

            REQUIRE(return_bucket.data.key_value == return_bucket_orig.data.key_value);
            REQUIRE(return_bucket.data.hash_half == return_bucket_orig.data.hash_half);
            REQUIRE(return_bucket2.data.key_value == return_bucket_orig2.data.key_value);
            REQUIRE(return_bucket2.data.hash_half == return_bucket_orig2.data.hash_half);
        }

        SECTION("migrate all the inserted buckets") {
            char *key_temp = nullptr;

            uint32_t count = 0;
            for (uint32_t index = 0; index < hashtable->data->buckets_count_real; index++) {
                size_t key_temp_length = snprintf(key_temp, 0, "key-%05d", index) + 1;
                key_temp = (char *) xalloc_alloc_zero(key_temp_length);
                snprintf(key_temp, key_temp_length, "key-%05d", index);

                hashtable_mpmc_result_t result = hashtable_mpmc_op_set(
                        hashtable,
                        key_temp,
                        key_temp_length,
                        (uintptr_t) index + 1,
                        &return_created_new,
                        &return_value_updated,
                        &return_previous_value);

                REQUIRE(result != HASHTABLE_MPMC_RESULT_FALSE);

                if (result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING) {
                    break;
                }

                count++;
            }

            REQUIRE(hashtable_mpmc_upsize_prepare(hashtable));
            REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING);

            for (uint32_t index = 0; index < hashtable->upsize.from->buckets_count_real; index++) {
                if (hashtable->upsize.from->buckets[index]._packed == 0) {
                    continue;
                }

                REQUIRE(hashtable_mpmc_upsize_migrate_bucket(
                        hashtable->upsize.from,
                        hashtable->data,
                        index));
            }

            // Force set the status of the hashtable to upsized as the GET while upsizing hasn't been tested yet at this
            // point
            hashtable->upsize.status = HASHTABLE_MPMC_STATUS_NOT_UPSIZING;
            hashtable->upsize.from = nullptr;

            for (uint32_t index = 0; index < count; index++) {
                size_t key_temp_length = snprintf(key_temp, 0, "key-%05d", index) + 1;
                key_temp = (char *) xalloc_alloc_zero(key_temp_length);
                snprintf(key_temp, key_temp_length, "key-%05d", index);

                REQUIRE(hashtable_mpmc_op_get(
                        hashtable,
                        key_temp,
                        key_temp_length,
                        &return_value) == HASHTABLE_MPMC_RESULT_TRUE);
                REQUIRE(return_value == (uintptr_t) index + 1);

                xalloc_free(key_temp);
            }
        }

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();
        hashtable_mpmc_free(hashtable);

        epoch_gc_thread_unregister_local(epoch_gc_thread);
        epoch_gc_thread_unregister_global(epoch_gc_thread);
        epoch_gc_thread_free(epoch_gc_thread);
        epoch_gc_free(epoch_gc);
    }

    SECTION("hashtable_mpmc_upsize_migrate_block") {
        char *key_temp = nullptr;
        bool return_created_new = false;
        bool return_value_updated = false;
        hashtable_mpmc_bucket_t return_bucket;
        hashtable_mpmc_bucket_index_t return_bucket_index;
        uintptr_t return_previous_value = 0, return_value = 0;
        uint32_t inserted_keys_count = 0;
        uint32_t keys_to_insert = 256 * 1024;

        hashtable_mpmc_t *hashtable = hashtable_mpmc_init(
                1024,
                keys_to_insert * 2,
                HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);
        epoch_gc_t *epoch_gc_kv = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_KEY_VALUE);
        epoch_gc_thread_t *epoch_gc_kv_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc_kv, epoch_gc_kv_thread);
        epoch_gc_thread_register_local(epoch_gc_kv_thread);

        epoch_gc_t *epoch_gc_data = epoch_gc_init(EPOCH_GC_OBJECT_TYPE_HASHTABLE_DATA);
        epoch_gc_thread_t *epoch_gc_data_thread = epoch_gc_thread_init();
        epoch_gc_thread_register_global(epoch_gc_data, epoch_gc_data_thread);
        epoch_gc_thread_register_local(epoch_gc_data_thread);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_init();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_init();

        SECTION("migrate all the blocks and try to read") {
            // Fill up the hashtable
            for (uint32_t index = 0; index < keys_to_insert; index++) {
                size_t key_temp_length = snprintf(key_temp, 0, "key-%05d", index) + 1;
                key_temp = (char *) xalloc_alloc_zero(key_temp_length);
                snprintf(key_temp, key_temp_length, "key-%05d", index);

                hashtable_mpmc_result_t result = hashtable_mpmc_op_set(
                        hashtable,
                        key_temp,
                        key_temp_length,
                        (uintptr_t) index + 1,
                        &return_created_new,
                        &return_value_updated,
                        &return_previous_value);

                if (result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING) {
                    REQUIRE(hashtable_mpmc_upsize_prepare(hashtable));
                    REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING);
                    index--;
                    continue;
                }

                REQUIRE(result == HASHTABLE_MPMC_RESULT_TRUE);

                if (hashtable->upsize.remaining_blocks > 0) {
                    REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING);
                    REQUIRE(hashtable_mpmc_upsize_migrate_block(hashtable) > 0);
                }

                inserted_keys_count++;

                epoch_gc_thread_set_epoch(
                        epoch_gc_kv_thread,
                        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

                epoch_gc_thread_set_epoch(
                        epoch_gc_data_thread,
                        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
            }

            do {
                hashtable_mpmc_upsize_migrate_block(hashtable);
            } while (hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING && hashtable->upsize.remaining_blocks > 0);

            REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_NOT_UPSIZING);

            for (uint32_t index = 0; index < inserted_keys_count; index++) {
                size_t key_temp_length = snprintf(key_temp, 0, "key-%05d", index) + 1;
                key_temp = (char *) xalloc_alloc_zero(key_temp_length);
                snprintf(key_temp, key_temp_length, "key-%05d", index);

                REQUIRE(hashtable_mpmc_op_get(
                        hashtable,
                        key_temp,
                        key_temp_length,
                        &return_value) == HASHTABLE_MPMC_RESULT_TRUE);
                REQUIRE(return_value == (uintptr_t) index + 1);

                xalloc_free(key_temp);

                epoch_gc_thread_set_epoch(
                        epoch_gc_kv_thread,
                        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

                epoch_gc_thread_set_epoch(
                        epoch_gc_data_thread,
                        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
            }
        }

        SECTION("migrate all the blocks and try to update") {
            // Fill up the hashtable
            for (uint32_t index = 0; index < keys_to_insert; index++) {
                size_t key_temp_length = snprintf(key_temp, 0, "key-%05d", index) + 1;
                key_temp = (char *) xalloc_alloc_zero(key_temp_length);
                snprintf(key_temp, key_temp_length, "key-%05d", index);

                hashtable_mpmc_result_t result = hashtable_mpmc_op_set(
                        hashtable,
                        key_temp,
                        key_temp_length,
                        (uintptr_t) index + 1,
                        &return_created_new,
                        &return_value_updated,
                        &return_previous_value);

                if (result == HASHTABLE_MPMC_RESULT_NEEDS_RESIZING) {
                    REQUIRE(hashtable_mpmc_upsize_prepare(hashtable));
                    REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING);
                    index--;
                    continue;
                }

                REQUIRE(result == HASHTABLE_MPMC_RESULT_TRUE);

                if (hashtable->upsize.remaining_blocks > 0) {
                    REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING);
                    REQUIRE(hashtable_mpmc_upsize_migrate_block(hashtable) > 0);
                }

                inserted_keys_count++;

                epoch_gc_thread_set_epoch(
                        epoch_gc_kv_thread,
                        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

                epoch_gc_thread_set_epoch(
                        epoch_gc_data_thread,
                        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
            }

            do {
                hashtable_mpmc_upsize_migrate_block(hashtable);
            } while (hashtable->upsize.status == HASHTABLE_MPMC_STATUS_UPSIZING && hashtable->upsize.remaining_blocks > 0);

            REQUIRE(hashtable->upsize.status == HASHTABLE_MPMC_STATUS_NOT_UPSIZING);

            for (uint32_t index = 0; index < inserted_keys_count; index++) {
                size_t key_temp_length = snprintf(key_temp, 0, "key-%05d", index) + 1;
                key_temp = (char *) xalloc_alloc_zero(key_temp_length);
                snprintf(key_temp, key_temp_length, "key-%05d", index);

                hashtable_mpmc_result_t result = hashtable_mpmc_op_set(
                        hashtable,
                        key_temp,
                        key_temp_length,
                        (uintptr_t) index + 1,
                        &return_created_new,
                        &return_value_updated,
                        &return_previous_value);

                REQUIRE(result == HASHTABLE_MPMC_RESULT_TRUE);
                REQUIRE(return_created_new == false);
                REQUIRE(return_value_updated == true);
                REQUIRE(return_previous_value == (uintptr_t) index + 1);

                epoch_gc_thread_set_epoch(
                        epoch_gc_kv_thread,
                        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_get_latest_epoch());

                epoch_gc_thread_set_epoch(
                        epoch_gc_data_thread,
                        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_get_latest_epoch());
            }
        }

        epoch_gc_thread_advance_epoch_tsc(epoch_gc_kv_thread);
        epoch_gc_thread_advance_epoch_tsc(epoch_gc_data_thread);
        REQUIRE(epoch_gc_thread_collect_all(epoch_gc_kv_thread) == 0);
        REQUIRE(epoch_gc_thread_collect_all(epoch_gc_data_thread) > 0);

        hashtable_mpmc_thread_epoch_operation_queue_hashtable_key_value_free();
        hashtable_mpmc_thread_epoch_operation_queue_hashtable_data_free();
        hashtable_mpmc_free(hashtable);

        epoch_gc_thread_unregister_local(epoch_gc_kv_thread);
        epoch_gc_thread_unregister_global(epoch_gc_kv_thread);
        epoch_gc_thread_free(epoch_gc_kv_thread);
        epoch_gc_free(epoch_gc_kv);

        epoch_gc_thread_unregister_local(epoch_gc_data_thread);
        epoch_gc_thread_unregister_global(epoch_gc_data_thread);
        epoch_gc_thread_free(epoch_gc_data_thread);
        epoch_gc_free(epoch_gc_data);
    }

    SECTION("fuzzy testing") {
        SECTION("fixed key count") {
            // TODO: this test should be improved, the threads spanwed by test_hashtable_mpmc_fuzzy_testing_run simply do an
            //       assert but this impacts the ease of testing, they should instead set an error, stop the processing and
            //       bubble up the error back to the caller and then here (the caller) should use REQUIRE to validate the
            //       result.
            uint32_t test_duration = 3;
            uint32_t test_keys_count = 4 * 1024 * 1024;
            uint16_t test_key_length = 12;

            char *test_keys = test_hashtable_mpmc_fuzzy_testing_keys_generate(
                    test_keys_count,
                    test_key_length);

            SECTION("no upsize") {
                hashtable_mpmc_t *test_hashtable = hashtable_mpmc_init(
                        test_keys_count * 2,
                        test_keys_count * 2,
                        HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

                SECTION("single thread") {
                    test_hashtable_mpmc_fuzzy_testing_run(
                            test_hashtable,
                            test_keys,
                            test_keys_count,
                            test_key_length,
                            1,
                            test_duration);
                }

                SECTION("multi thread") {
                    test_hashtable_mpmc_fuzzy_testing_run(
                            test_hashtable,
                            test_keys,
                            test_keys_count,
                            test_key_length,
                            utils_cpu_count() * 2,
                            test_duration);
                }
            }

            SECTION("with upsize") {
                hashtable_mpmc_t *test_hashtable = hashtable_mpmc_init(
                        16 * 1024,
                        test_keys_count * 2,
                        HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE);

                SECTION("single thread") {
                    test_hashtable_mpmc_fuzzy_testing_run(
                            test_hashtable,
                            test_keys,
                            test_keys_count,
                            test_key_length,
                            1,
                            test_duration);
                }

                SECTION("multi thread") {
                    test_hashtable_mpmc_fuzzy_testing_run(
                            test_hashtable,
                            test_keys,
                            test_keys_count,
                            test_key_length,
                            utils_cpu_count() * 2,
                            test_duration);
                }
            }

            test_hashtable_mpmc_fuzzy_testing_keys_free(test_keys);
        }

        SECTION("random key count and multiple test runs") {
            uint32_t test_runs = 10;
            uint32_t test_duration = 5;
            uint32_t test_threads = utils_cpu_count() * 4;
            uint32_t test_hashtable_initial_size = 16 * 1024;
            uint32_t test_hashtable_upsize_block_size = HASHTABLE_MPMC_UPSIZE_BLOCK_SIZE;

            SECTION("multi thread") {
                for (uint32_t test_run = 0; test_run < test_runs; test_run++) {
                    uint32_t test_keys_count = 1 * 1024 * 1024;
                    uint16_t test_key_length = 8;
                    uint32_t test_hashtable_max_size = test_keys_count * 2;

                    fprintf(
                            stdout,
                            "[%lu] > test_run <%u/%u>\n",
                            intrinsics_tsc(),
                            test_run + 1,
                            test_runs);
                    fflush(stdout);

                    fprintf(stdout, "[%lu] >   generating <%u> keys\n", intrinsics_tsc(), test_keys_count);
                    char *test_keys = test_hashtable_mpmc_fuzzy_testing_keys_generate(
                            test_keys_count,
                            test_key_length);
                    fprintf(stdout, "[%lu] >   keys generated\n", intrinsics_tsc());

                    hashtable_mpmc_t *test_hashtable = hashtable_mpmc_init(
                            test_hashtable_initial_size,
                            test_hashtable_max_size,
                            test_hashtable_upsize_block_size);

                    test_hashtable_mpmc_fuzzy_testing_run(
                            test_hashtable,
                            test_keys,
                            test_keys_count,
                            test_key_length,
                            test_threads,
                            test_duration);

                    test_hashtable_mpmc_fuzzy_testing_keys_free(test_keys);

                    fprintf(stdout, "[%lu] >   test run completed\n", intrinsics_tsc());
                    fflush(stdout);
                }
            }
        }
    }
}
