/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.init
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <numa.h>

#include "data_structures/hashtable/spsc/hashtable_spsc.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE("data_structures/hashtable/spsc/hashtable_spsc.c", "[data_structures][hashtable][hashtable_spsc]") {
    char *key = "This Is A Key";
    char *key_different_case = "THIS IS A KEY";
    hashtable_spsc_key_length_t key_length = strlen(key);
    uint32_t key_hash = fnv_32_hash(key, key_length);
    uint32_t key_different_case_hash = fnv_32_hash(key_different_case, key_length);
    uint32_t key_ci_hash = fnv_32_hash_ci(key, key_length);
    uint32_t key_different_case_ci_hash = fnv_32_hash_ci(key, key_length);

    char *key2 = "This Is Another Key";
    char *key2_different_case = "THIS IS ANOTHER KEY";
    hashtable_spsc_key_length_t key2_length = strlen(key2);
    uint32_t key2_hash = fnv_32_hash(key2, key2_length);
    uint32_t key2_ci_hash = fnv_32_hash_ci(key2, key2_length);

    SECTION("hashtable_spsc_new") {
        SECTION("valid buckets_count, default max range, stop_on_not_set true, free_keys_on_deallocation false") {
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    10,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    true,
                    false);

            REQUIRE(hashtable->buckets_count == 10);
            REQUIRE(hashtable->buckets_count_pow2 == 16);
            REQUIRE(hashtable->buckets_count_real == 16 + HASHTABLE_SPSC_DEFAULT_MAX_RANGE);
            REQUIRE(hashtable->stop_on_not_set == true);
            REQUIRE(hashtable->free_keys_on_deallocation == false);
            REQUIRE(hashtable->max_range == HASHTABLE_SPSC_DEFAULT_MAX_RANGE);

            hashtable_spsc_free(hashtable);
        }

        SECTION("valid buckets_count, stop_on_not_set false") {
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    10,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);

            REQUIRE(hashtable->stop_on_not_set == false);

            hashtable_spsc_free(hashtable);
        }

        SECTION("valid buckets_count, free_keys_on_deallocation true") {
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    10,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    true);

            REQUIRE(hashtable->free_keys_on_deallocation == true);

            hashtable_spsc_free(hashtable);
        }

        SECTION("valid buckets_count, max range 1") {
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    10,
                    1,
                    true,
                    false);

            REQUIRE(hashtable->buckets_count_real == 16 + 1);
            REQUIRE(hashtable->max_range == 1);

            hashtable_spsc_free(hashtable);
        }
    }

    SECTION("hashtable_spsc_get_buckets") {
        hashtable_spsc_t *hashtable = hashtable_spsc_new(
                10,
                HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                true,
                false);

        hashtable_spsc_bucket_t *buckets = hashtable_spsc_get_buckets(hashtable);

        REQUIRE(buckets == (void *) (hashtable->hashes + hashtable->buckets_count_real));

        hashtable_spsc_free(hashtable);
    }

    SECTION("hashtable_spsc_find_set_bucket") {
        hashtable_spsc_t *hashtable = hashtable_spsc_new(
                16,
                HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                false,
                false);
        hashtable_spsc_bucket_count_t hashtable_key_bucket_index = key_ci_hash & (hashtable->buckets_count_pow2 - 1);
        hashtable_spsc_bucket_count_t hashtable_key_bucket_index_max =
                hashtable_key_bucket_index + hashtable->max_range;

        SECTION("bucket found") {
            hashtable->hashes[hashtable_key_bucket_index].set = true;

            REQUIRE(hashtable_spsc_find_set_bucket(
                    hashtable,
                    hashtable_key_bucket_index,
                    hashtable_key_bucket_index_max) == hashtable_key_bucket_index);
        }

        SECTION("bucket not found - nothing in range") {
            hashtable->hashes[hashtable_key_bucket_index_max].set = true;

            REQUIRE(hashtable_spsc_find_set_bucket(
                    hashtable,
                    hashtable_key_bucket_index,
                    hashtable_key_bucket_index_max) == -1);
        }

        SECTION("bucket not found - hashtable empty") {
            REQUIRE(hashtable_spsc_find_set_bucket(
                    hashtable,
                    hashtable_key_bucket_index,
                    hashtable_key_bucket_index_max) == -1);
        }

        hashtable_spsc_free(hashtable);
    }

    SECTION("hashtable_spsc_find_empty_bucket") {
        hashtable_spsc_t *hashtable = hashtable_spsc_new(
                16,
                HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                false,
                false);
        hashtable_spsc_bucket_count_t hashtable_key_bucket_index = key_ci_hash & (hashtable->buckets_count_pow2 - 1);
        hashtable_spsc_bucket_count_t hashtable_key_bucket_index_max =
                hashtable_key_bucket_index + hashtable->max_range;

        SECTION("bucket found") {
            REQUIRE(hashtable_spsc_find_empty_bucket(
                    hashtable,
                    hashtable_key_bucket_index,
                    hashtable_key_bucket_index_max) == hashtable_key_bucket_index);
        }

        SECTION("bucket not found - nothing in range") {
            for (int index = hashtable_key_bucket_index; index < hashtable_key_bucket_index_max; index++) {
                hashtable->hashes[index].set = true;
            }

            REQUIRE(hashtable_spsc_find_empty_bucket(
                    hashtable,
                    hashtable_key_bucket_index,
                    hashtable_key_bucket_index_max) == -1);
        }

        SECTION("bucket not found - hashtable full") {
            for (int index = 0; index < hashtable->buckets_count_real; index++) {
                hashtable->hashes[index].set = true;
            }

            REQUIRE(hashtable_spsc_find_empty_bucket(
                    hashtable,
                    hashtable_key_bucket_index,
                    hashtable_key_bucket_index_max) == -1);
        }

        hashtable_spsc_free(hashtable);
    }

    SECTION("hashtable_spsc_bucket_index_from_hash") {
        hashtable_spsc_t *hashtable = hashtable_spsc_new(
                16,
                HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                false,
                false);

        REQUIRE((key_hash & (hashtable->buckets_count_pow2 - 1)) == 15);

        hashtable_spsc_free(hashtable);
    }

    SECTION("Case Insensitive interface") {
        SECTION("hashtable_spsc_find_bucket_index_by_key_ci") {
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);
            hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);

            hashtable_spsc_bucket_count_t hashtable_key_bucket_index =
                    key_ci_hash & (hashtable->buckets_count_pow2 - 1);

            hashtable_spsc_t *hashtable2 = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    true,
                    false);
            hashtable_spsc_bucket_t *hashtable2_buckets = hashtable_spsc_get_buckets(hashtable2);
            hashtable_spsc_bucket_count_t hashtable2_key_bucket_index =
                    key_ci_hash & (hashtable2->buckets_count_pow2 - 1);

            SECTION("bucket found - key exists") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(
                        hashtable, key_ci_hash, key, key_length) == hashtable_key_bucket_index);
            }

            SECTION("bucket found - same hash but different key_length and key") {
                hashtable->hashes[hashtable_key_bucket_index + 1].set = true;
                hashtable->hashes[hashtable_key_bucket_index + 1].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index + 1].key = key;
                hashtable_buckets[hashtable_key_bucket_index + 1].key_length = key_length;

                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash =
                        hashtable->hashes[hashtable_key_bucket_index + 1].cmp_hash;
                hashtable_buckets[hashtable_key_bucket_index].key = key2;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key2_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(
                        hashtable, key_ci_hash, key, key_length) == hashtable_key_bucket_index + 1);
            }

            SECTION("bucket found - same hash and same key_length but different key") {
                hashtable->hashes[hashtable_key_bucket_index + 1].set = true;
                hashtable->hashes[hashtable_key_bucket_index + 1].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index + 1].key = key;
                hashtable_buckets[hashtable_key_bucket_index + 1].key_length = key_length;

                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash =
                        hashtable->hashes[hashtable_key_bucket_index + 1].cmp_hash;
                hashtable_buckets[hashtable_key_bucket_index].key = key2;
                hashtable_buckets[hashtable_key_bucket_index].key_length =
                        hashtable_buckets[hashtable_key_bucket_index + 1].key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(
                        hashtable, key_ci_hash, key, key_length) == hashtable_key_bucket_index + 1);
            }

            SECTION("bucket found - key exists, not first bucket") {
                int extra_slots = 6;

                for (int index = 0; index < extra_slots; index++) {
                    hashtable->hashes[hashtable_key_bucket_index + index].set = true;
                    hashtable->hashes[hashtable_key_bucket_index + index].cmp_hash = (key_ci_hash & 0x7FFF) + 1 + index;
                }

                hashtable->hashes[hashtable_key_bucket_index + extra_slots].set = true;
                hashtable->hashes[hashtable_key_bucket_index + extra_slots].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index + extra_slots].key = key;
                hashtable_buckets[hashtable_key_bucket_index + extra_slots].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(
                        hashtable, key_ci_hash, key, key_length) == hashtable_key_bucket_index + extra_slots);
            }

            SECTION("bucket found - key exists, last bucket within max range") {
                hashtable_key_bucket_index += hashtable->max_range - 1;
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(hashtable, key_ci_hash, key, key_length) ==
                        hashtable_key_bucket_index);
            }

            SECTION("bucket found - key with different case") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(
                        hashtable, key_hash, key_different_case, key_length) == hashtable_key_bucket_index);
            }

            SECTION("bucket not found - key not-existent") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(
                        hashtable, key2_ci_hash, key2, key2_length) == -1);
            }

            SECTION("bucket not found - key outside max range") {
                hashtable_key_bucket_index += hashtable->max_range;
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(hashtable, key_ci_hash, key, key_length) == -1);
            }

            SECTION("bucket not found - key exists but not set buckets on the way") {
                hashtable2_key_bucket_index += hashtable2->max_range;
                hashtable2->hashes[hashtable2_key_bucket_index].set = true;
                hashtable2->hashes[hashtable2_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable2_buckets[hashtable2_key_bucket_index].key = key;
                hashtable2_buckets[hashtable2_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_ci(
                        hashtable2, key_ci_hash, key, key_length) == -1);
            }

            hashtable_spsc_free(hashtable);
            hashtable_spsc_free(hashtable2);
        }

        SECTION("hashtable_spsc_op_try_set_ci") {
            char *value1 = "first value";
            char *value2 = "second value";
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);
            hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);

            hashtable_spsc_bucket_count_t hashtable_key_bucket_index =
                    key_ci_hash & (hashtable->buckets_count_pow2 - 1);

            SECTION("value set - insert") {
                REQUIRE(hashtable_spsc_op_try_set_ci(hashtable, key, key_length, value1));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set);
                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].cmp_hash == (key_ci_hash & 0x7FFF));
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key == key);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key_length == key_length);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].value == value1);
            }

            SECTION("value set - update") {
                REQUIRE(hashtable_spsc_op_try_set_ci(hashtable, key, key_length, value1));
                REQUIRE(hashtable_spsc_op_try_set_ci(hashtable, key, key_length, value2));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set);
                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].cmp_hash == (key_ci_hash & 0x7FFF));
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key == key);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key_length == key_length);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].value == value2);
            }

            SECTION("value set - update - key with different key") {
                REQUIRE(hashtable_spsc_op_try_set_ci(hashtable, key, key_length, value1));
                REQUIRE(hashtable_spsc_op_try_set_ci(hashtable, key_different_case, key_length, value2));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set);
                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].cmp_hash == (key_ci_hash & 0x7FFF));
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key == key_different_case);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key_length == key_length);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].value == value2);
            }

            SECTION("value not set - no free buckets in range") {
                for (int index = 0; index < hashtable->max_range; index++) {
                    hashtable->hashes[hashtable_key_bucket_index + index].set = true;
                }

                REQUIRE(!hashtable_spsc_op_try_set_ci(hashtable, key, key_length, &value1));
            }

            SECTION("value not set - hashtable full") {
                for (int index = 0; index < hashtable->buckets_count_real; index++) {
                    hashtable->hashes[index].set = true;
                }

                REQUIRE(!hashtable_spsc_op_try_set_ci(hashtable, key, key_length, &value1));
            }

            hashtable_spsc_free(hashtable);
        }



        SECTION("hashtable_spsc_op_get_ci") {
            char *value1 = "first value";
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);
            hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);

            hashtable_spsc_bucket_count_t hashtable_key_bucket_index = key_ci_hash & (hashtable->buckets_count_pow2 - 1);

            SECTION("value found - existing key") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;
                hashtable_buckets[hashtable_key_bucket_index].value = value1;

                REQUIRE(hashtable_spsc_op_get_ci(hashtable, key, key_length) == value1);
            }

            SECTION("value found - existing key with different case") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;
                hashtable_buckets[hashtable_key_bucket_index].value = value1;

                REQUIRE(hashtable_spsc_op_get_ci(hashtable, key_different_case, key_length) == value1);
            }

            SECTION("value not found - non-existent key") {
                REQUIRE(hashtable_spsc_op_get_ci(hashtable, key, key_length) == NULL);
            }

            hashtable_spsc_free(hashtable);
        }

        SECTION("hashtable_spsc_op_delete_ci") {
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);
            hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);

            hashtable_spsc_bucket_count_t hashtable_key_bucket_index = key_ci_hash & (hashtable->buckets_count_pow2 - 1);

            SECTION("value deleted - existing key") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_op_delete_ci(hashtable, key, key_length));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set == false);
            }

            SECTION("value deleted - existing key with different case") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_ci_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_op_delete_ci(hashtable, key_different_case, key_length));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set == false);
            }

            SECTION("value not deleted - non-existent key") {
                REQUIRE(!hashtable_spsc_op_delete_ci(hashtable, key, key_length));
            }

            hashtable_spsc_free(hashtable);
        }
    }

    SECTION("Case Sensitive interface") {
        SECTION("hashtable_spsc_find_bucket_index_by_key_cs") {
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);
            hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);

            hashtable_spsc_bucket_count_t hashtable_key_bucket_index = key_hash & (hashtable->buckets_count_pow2 - 1);

            hashtable_spsc_t *hashtable2 = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    true,
                    false);
            hashtable_spsc_bucket_t *hashtable2_buckets = hashtable_spsc_get_buckets(hashtable2);
            hashtable_spsc_bucket_count_t hashtable2_key_bucket_index = key_hash & (hashtable2->buckets_count_pow2 - 1);

            SECTION("bucket found - key exists") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(
                        hashtable, key_hash, key, key_length) == hashtable_key_bucket_index);
            }

            SECTION("bucket found - same hash but different key_length and key") {
                hashtable->hashes[hashtable_key_bucket_index + 1].set = true;
                hashtable->hashes[hashtable_key_bucket_index + 1].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index + 1].key = key;
                hashtable_buckets[hashtable_key_bucket_index + 1].key_length = key_length;

                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash =
                        hashtable->hashes[hashtable_key_bucket_index + 1].cmp_hash;
                hashtable_buckets[hashtable_key_bucket_index].key = key2;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key2_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(
                        hashtable, key_hash, key, key_length) == hashtable_key_bucket_index + 1);
            }

            SECTION("bucket found - same hash and same key_length but different key") {
                hashtable->hashes[hashtable_key_bucket_index + 1].set = true;
                hashtable->hashes[hashtable_key_bucket_index + 1].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index + 1].key = key;
                hashtable_buckets[hashtable_key_bucket_index + 1].key_length = key_length;

                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash =
                        hashtable->hashes[hashtable_key_bucket_index + 1].cmp_hash;
                hashtable_buckets[hashtable_key_bucket_index].key = key2;
                hashtable_buckets[hashtable_key_bucket_index].key_length =
                        hashtable_buckets[hashtable_key_bucket_index + 1].key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(
                        hashtable, key_hash, key, key_length) == hashtable_key_bucket_index + 1);
            }

            SECTION("bucket found - key exists, not first bucket") {
                int extra_slots = 6;

                for (int index = 0; index < extra_slots; index++) {
                    hashtable->hashes[hashtable_key_bucket_index + index].set = true;
                    hashtable->hashes[hashtable_key_bucket_index + index].cmp_hash = (key_hash & 0x7FFF) + 1 + index;
                }

                hashtable->hashes[hashtable_key_bucket_index + extra_slots].set = true;
                hashtable->hashes[hashtable_key_bucket_index + extra_slots].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index + extra_slots].key = key;
                hashtable_buckets[hashtable_key_bucket_index + extra_slots].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(
                        hashtable, key_hash, key, key_length) == hashtable_key_bucket_index + extra_slots);
            }

            SECTION("bucket found - key exists, last bucket within max range") {
                hashtable_key_bucket_index += hashtable->max_range - 1;
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(
                        hashtable, key_hash, key, key_length) == hashtable_key_bucket_index);
            }

            SECTION("bucket not found - key with different case") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(
                        hashtable, key_hash, key_different_case, key_length) == -1);
            }

            SECTION("bucket not found - key not-existent") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(
                        hashtable, key2_ci_hash, key2, key2_length) == -1);
            }

            SECTION("bucket not found - key outside max range") {
                hashtable_key_bucket_index += hashtable->max_range;
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(hashtable, key_hash, key, key_length) == -1);
            }

            SECTION("bucket not found - key exists but not set buckets on the way") {
                hashtable2_key_bucket_index += hashtable2->max_range;
                hashtable2->hashes[hashtable2_key_bucket_index].set = true;
                hashtable2->hashes[hashtable2_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable2_buckets[hashtable2_key_bucket_index].key = key;
                hashtable2_buckets[hashtable2_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_find_bucket_index_by_key_cs(hashtable2, key_hash, key, key_length) == -1);
            }

            hashtable_spsc_free(hashtable);
            hashtable_spsc_free(hashtable2);
        }

        SECTION("hashtable_spsc_op_try_set_cs") {
            char *value1 = "first value";
            char *value2 = "second value";
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);
            hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);

            hashtable_spsc_bucket_count_t hashtable_key_bucket_index = key_hash & (hashtable->buckets_count_pow2 - 1);
            hashtable_spsc_bucket_count_t hashtable_key_different_case_bucket_index =
                    key_different_case_hash & (hashtable->buckets_count_pow2 - 1);

            // The hashtable is small and the hashes end-up colliding, so it's necessary to access the bucket right after,
            // anyway for future reference making it a dynamic check in case the hashtable size is changed
            if (hashtable_key_bucket_index == hashtable_key_different_case_bucket_index) {
                hashtable_key_different_case_bucket_index++;
            }

            SECTION("value set - insert") {
                REQUIRE(hashtable_spsc_op_try_set_cs(hashtable, key, key_length, value1));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set);
                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].cmp_hash == (key_hash & 0x7FFF));
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key == key);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key_length == key_length);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].value == value1);
            }

            SECTION("value set - update") {
                REQUIRE(hashtable_spsc_op_try_set_cs(hashtable, key, key_length, value1));
                REQUIRE(hashtable_spsc_op_try_set_cs(hashtable, key, key_length, value2));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set);
                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].cmp_hash == (key_hash & 0x7FFF));
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key == key);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key_length == key_length);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].value == value2);
            }

            SECTION("value set - insert - key with different key") {
                REQUIRE(hashtable_spsc_op_try_set_cs(hashtable, key, key_length, value1));
                REQUIRE(hashtable_spsc_op_try_set_cs(hashtable, key_different_case, key_length, value2));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set);
                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].cmp_hash == (key_hash & 0x7FFF));
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key == key);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].key_length == key_length);
                REQUIRE(hashtable_buckets[hashtable_key_bucket_index].value == value1);

                REQUIRE(hashtable->hashes[hashtable_key_different_case_bucket_index].set);
                REQUIRE(hashtable->hashes[hashtable_key_different_case_bucket_index].cmp_hash ==
                        (key_different_case_hash & 0x7FFF));
                REQUIRE(hashtable_buckets[hashtable_key_different_case_bucket_index].key == key_different_case);
                REQUIRE(hashtable_buckets[hashtable_key_different_case_bucket_index].key_length == key_length);
                REQUIRE(hashtable_buckets[hashtable_key_different_case_bucket_index].value == value2);
            }

            SECTION("value not set - no free buckets in range") {
                for (int index = 0; index < hashtable->max_range; index++) {
                    hashtable->hashes[hashtable_key_bucket_index + index].set = true;
                }

                REQUIRE(!hashtable_spsc_op_try_set_cs(hashtable, key, key_length, &value1));
            }

            SECTION("value not set - hashtable full") {
                for (int index = 0; index < hashtable->buckets_count_real; index++) {
                    hashtable->hashes[index].set = true;
                }

                REQUIRE(!hashtable_spsc_op_try_set_cs(hashtable, key, key_length, &value1));
            }

            hashtable_spsc_free(hashtable);
        }

        SECTION("hashtable_spsc_op_get_cs") {
            char *value1 = "first value";
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);
            hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);

            hashtable_spsc_bucket_count_t hashtable_key_bucket_index = key_hash & (hashtable->buckets_count_pow2 - 1);

            SECTION("value found - existing key ") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;
                hashtable_buckets[hashtable_key_bucket_index].value = value1;

                REQUIRE(hashtable_spsc_op_get_cs(hashtable, key, key_length) == value1);
            }

            SECTION("value not found - existing key with different case") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;
                hashtable_buckets[hashtable_key_bucket_index].value = value1;

                REQUIRE(hashtable_spsc_op_get_cs(hashtable, key_different_case, key_length) == NULL);
            }

            SECTION("value not found - non-existent key") {
                REQUIRE(hashtable_spsc_op_get_cs(hashtable, key, key_length) == NULL);
            }

            hashtable_spsc_free(hashtable);
        }

        SECTION("hashtable_spsc_op_delete_cs") {
            hashtable_spsc_t *hashtable = hashtable_spsc_new(
                    16,
                    HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                    false,
                    false);
            hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);

            hashtable_spsc_bucket_count_t hashtable_key_bucket_index = key_hash & (hashtable->buckets_count_pow2 - 1);

            SECTION("value deleted - existing key") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(hashtable_spsc_op_delete_cs(hashtable, key, key_length));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set == false);
            }

            SECTION("value not deleted - existing key with different case") {
                hashtable->hashes[hashtable_key_bucket_index].set = true;
                hashtable->hashes[hashtable_key_bucket_index].cmp_hash = key_hash & 0x7FFF;
                hashtable_buckets[hashtable_key_bucket_index].key = key;
                hashtable_buckets[hashtable_key_bucket_index].key_length = key_length;

                REQUIRE(!hashtable_spsc_op_delete_cs(hashtable, key_different_case, key_length));

                REQUIRE(hashtable->hashes[hashtable_key_bucket_index].set == true);
            }

            SECTION("value not deleted - non-existent key") {
                REQUIRE(!hashtable_spsc_op_delete_cs(hashtable, key, key_length));
            }

            hashtable_spsc_free(hashtable);
        }
    }

    SECTION("hashtable_spsc_op_iter") {
        char *value1 = "first value";
        char *value2 = "second value";
        hashtable_spsc_t *hashtable = hashtable_spsc_new(
                16,
                HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                false,
                false);
        hashtable_spsc_bucket_t *hashtable_buckets = hashtable_spsc_get_buckets(hashtable);
        hashtable_spsc_bucket_index_t hashtable_key_bucket_index = 0;

        SECTION("empty hashtable") {
            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_key_bucket_index) == NULL);
            REQUIRE(hashtable_key_bucket_index == -1);
        }

        SECTION("hashtable with 1 bucket set") {
            hashtable->hashes[2].set = true;
            hashtable_buckets[2].value = value1;

            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_key_bucket_index) == value1);
            REQUIRE(hashtable_key_bucket_index == 2);
            hashtable_key_bucket_index++;

            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_key_bucket_index) == NULL);
            REQUIRE(hashtable_key_bucket_index == -1);
        }

        SECTION("hashtable with 2 bucket set") {
            hashtable->hashes[2].set = true;
            hashtable_buckets[2].value = value1;
            hashtable->hashes[6].set = true;
            hashtable_buckets[6].value = value2;

            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_key_bucket_index) == value1);
            REQUIRE(hashtable_key_bucket_index == 2);
            hashtable_key_bucket_index++;

            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_key_bucket_index) == value2);
            REQUIRE(hashtable_key_bucket_index == 6);
            hashtable_key_bucket_index++;

            REQUIRE(hashtable_spsc_op_iter(hashtable, &hashtable_key_bucket_index) == NULL);
            REQUIRE(hashtable_key_bucket_index == -1);
        }

        hashtable_spsc_free(hashtable);
    }
}
