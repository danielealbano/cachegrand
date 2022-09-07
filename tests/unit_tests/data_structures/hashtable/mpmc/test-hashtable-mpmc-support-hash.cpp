/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "fiber.h"
#include "fiber_scheduler.h"
#include "clock.h"
#include "config.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "worker/worker_stats.h"
#include "worker/worker_context.h"
#include "worker/worker.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_support_hash.h"

#include "fixtures-hashtable-mpmc.h"

TEST_CASE("hashtable/hashtable_support_hash.c", "[hashtable][hashtable_support][hashtable_support_hash]") {
    worker_context_t worker_context = { 0 };
    worker_context.worker_index = UINT16_MAX;
    worker_context_set(&worker_context);
    transaction_set_worker_index(worker_context.worker_index);

    SECTION("hashtable_mcmp_support_hash_calculate") {
        SECTION("hash calculation") {
            REQUIRE(hashtable_mcmp_support_hash_calculate(
                    test_key_1,
                    test_key_1_len) == test_key_1_hash);
        }
    }

    SECTION("hashtable_mcmp_support_hash_half") {
        SECTION("hash half calculation") {
            REQUIRE(hashtable_mcmp_support_hash_half(test_key_1_hash) == test_key_1_hash_half);
        }

        SECTION("ensure the upper bit is always set to 1") {
            REQUIRE((hashtable_mcmp_support_hash_half(test_key_1_hash) & 0x80000000u) == 0x80000000u);
        }
    }
}
