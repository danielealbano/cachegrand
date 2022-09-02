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

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"

TEST_CASE("hashtable/hashtable_config.c", "[hashtable][hashtable_config]") {
    SECTION("hashtable_mcmp_config_init") {
        hashtable_config_t* hashtable_config = hashtable_mcmp_config_init();

        REQUIRE(hashtable_config != NULL);

        hashtable_mcmp_config_free(hashtable_config);
    }
}
