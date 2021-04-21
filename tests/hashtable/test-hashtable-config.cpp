#include <catch2/catch.hpp>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_config.h"

TEST_CASE("hashtable/hashtable_config.c", "[hashtable][hashtable_config]") {
    SECTION("hashtable_mcmp_config_init") {
        hashtable_config_t* hashtable_config = hashtable_mcmp_config_init();

        REQUIRE(hashtable_config != NULL);

        hashtable_mcmp_config_free(hashtable_config);
    }
}
