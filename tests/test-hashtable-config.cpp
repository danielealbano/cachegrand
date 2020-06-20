#include "catch.hpp"

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"

TEST_CASE("hashtable/hashtable_config.c", "[hashtable][hashtable_config]") {
    SECTION("hashtable_config_init") {
        hashtable_config_t* hashtable_config = hashtable_config_init();

        REQUIRE(hashtable_config != NULL);

        hashtable_config_free(hashtable_config);
    }
}
