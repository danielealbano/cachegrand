#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "cmake_config.h"

#include "log.h"
#include "cpu.h"
#include "xalloc.h"
#include "random.h"
#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_hash.h"
#include "hashtable/hashtable_op_set.h"

#include "test-support.h"

test_key_same_bucket_t* test_same_hash_mod_fixtures_generate(
        hashtable_bucket_count_t bucket_count,
        const char* key_prefix,
        uint32_t count) {
    char* key_test;
    char* key_test_copy;
    char* key_model;
    size_t key_test_size;
    test_key_same_bucket_t* test_key_same_bucket_fixtures;
    bool reference_hash_set = false;
    hashtable_bucket_index_t reference_bucket_index;

    uint32_t matches_counter = 0;

    // Build the key model
    key_model = (char*)malloc(strlen(key_prefix) + 5);
    sprintf(key_model, "%s%%06d", key_prefix);

    // Build the struct to hold the fixtures (+1 to keep the last as NULL)
    test_key_same_bucket_fixtures = (test_key_same_bucket_t*)malloc(sizeof(test_key_same_bucket_t) * (count + 1));
    memset(test_key_same_bucket_fixtures, 0, sizeof(test_key_same_bucket_t) * (count + 1));

    // Build the temporary key
    key_test_size = strlen(key_prefix) + 7;
    key_test = (char*)malloc(key_test_size);
    memset(key_test, 0, key_test_size);

    fprintf(stdout, "test_same_hash_mod_fixtures_generate(%lu, \"%s\", %d)\n", bucket_count, key_prefix, count);
    fprintf(stdout, ">       \"%s%*s\" (%-3s) | %-18s | %-10s\n",
            "KEY", (int)key_test_size - 3, "", "LEN", "HASH", "HALF HASH");
    fflush(stdout);

    for(uint64_t i = 0; i<=999999; i++) {
        bool add_fixture = false;

        snprintf(key_test, key_test_size, key_model, i);
        hashtable_hash_t hash_test = hashtable_support_hash_calculate(key_test, strlen(key_test));

        if (!reference_hash_set) {
            reference_bucket_index = hash_test % bucket_count;
            add_fixture = reference_hash_set = true;
        } else {
            add_fixture = reference_bucket_index == hash_test % bucket_count;
        }

        if (add_fixture) {
            key_test_copy = (char*)malloc(key_test_size);
            memset(key_test_copy, 0, key_test_size);
            strncpy(key_test_copy, key_test, key_test_size);

            test_key_same_bucket_fixtures[matches_counter].key = key_test_copy;
            test_key_same_bucket_fixtures[matches_counter].key_len = strlen(key_test);
            test_key_same_bucket_fixtures[matches_counter].key_hash = hash_test;
            test_key_same_bucket_fixtures[matches_counter].key_hash_half =
                    hashtable_support_hash_half(hash_test) | 0x80000000;

            fprintf(stdout, "> %03u - \"%s%*s\" (%03lu) | 0x%016lx | 0x%08xu\n",
                    matches_counter,
                    key_test_copy,
                    (int)(key_test_size - strlen(key_test_copy)),
                    "",
                    strlen(key_test_copy),
                    hash_test,
                    test_key_same_bucket_fixtures[matches_counter].key_hash_half);
            fflush(stdout);

            matches_counter++;

            if (matches_counter == count) {
                break;
            }
        }
    }

    fprintf(stdout, "reference_bucket_index = %lu\n", reference_bucket_index);
    fflush(stdout);

    free(key_model);
    free(key_test);

    assert(matches_counter == count);

    return test_key_same_bucket_fixtures;
}

void test_same_hash_mod_fixtures_free(
        test_key_same_bucket_t* test_key_same_bucket) {
    uint32_t i = 0;

    while(test_key_same_bucket[i].key != NULL) {
        free(test_key_same_bucket[i].key);
        i++;
    }

    free(test_key_same_bucket);
}

void test_support_set_thread_affinity(int thread_index) {
#if !defined(__MINGW32__)
    int res;
    cpu_set_t cpuset;
    pthread_t thread;

    uint32_t logical_core_count = psnip_cpu_count();
    uint32_t logical_core_index = thread_index % logical_core_count;

    CPU_ZERO(&cpuset);
    CPU_SET(logical_core_index, &cpuset);

    thread = pthread_self();
    res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        perror("pthread_setaffinity_np");
    }
#endif
}

char* test_support_build_keys_random_max_length(uint64_t count) {
    char keys_character_set_list[] = {TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_LIST };
    char* keys = (char*)xalloc_mmap_alloc(count * TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH);

    char* keys_current = keys;
    for(uint64_t i = 0; i < count; i++) {
        for(uint8_t i2 = 0; i2 < TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH - 1; i2++) {
            *keys_current = keys_character_set_list[random_generate() % TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_SIZE];
            keys_current++;
        }
        *keys_current=0;
        keys_current++;

        assert((keys_current - keys) % TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH != 0);
    }

    return keys;
}

char* test_support_build_keys_random_random_length(uint64_t count) {
    char keys_character_set_list[] = {TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_LIST};
    char *keys = (char *) xalloc_mmap_alloc(count * TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH);

    char *keys_current = keys;

    for (uint64_t i = 0; i < count; i++) {
        uint8_t i2;
        uint8_t length =
                ((random_generate() % (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH - TEST_SUPPORT_RANDOM_KEYS_MIN_LENGTH)) + TEST_SUPPORT_RANDOM_KEYS_MIN_LENGTH) -
                1;
        for (i2 = 0; i2 < length; i2++) {
            *keys_current = keys_character_set_list[random_generate() % TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_SIZE];
            keys_current++;
        }
        *keys_current = 0;
        keys_current += TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH - length;

        assert((keys_current - keys) % TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH != 0);
    }

    return keys;
}

void test_support_free_keys(char* keys, uint64_t count) {
    xalloc_mmap_free(keys, count * TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH);
}

char* test_support_init_keys(uint64_t keys_count, uint8_t keys_generator_method) {
    if (keys_generator_method == TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_MAX_LENGTH) {
        return test_support_build_keys_random_max_length(keys_count);
    } else {
        return test_support_build_keys_random_random_length(keys_count);
    }
}

hashtable_t* test_support_init_hashtable(uint64_t initial_size) {
    hashtable_config_t* hashtable_config = hashtable_config_init();
    hashtable_config->initial_size = initial_size;
    hashtable_config->can_auto_resize = false;

    return hashtable_init(hashtable_config);
}

bool test_support_hashtable_prefill(hashtable_t* hashtable, char* keys, uint64_t value, uint64_t insert_count) {
    for(long int i = 0; i < insert_count; i++) {
        char* key = keys + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH * i);

        bool result = hashtable_op_set(
                hashtable,
                key,
                strlen(key),
                value);

        if (!result) {
            return false;
        }
    }

    return true;
}
