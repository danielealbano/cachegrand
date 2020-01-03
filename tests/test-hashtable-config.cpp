#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support.h"
#include "hashtable/hashtable_gc.h"
#include "hashtable/hashtable_op_get.h"
#include "hashtable/hashtable_op_set.h"

TEST_CASE("hashtable_config.c", "[hashtable][hashtable_config]") {
    SECTION("hashtable_config_init") {
        hashtable_config_t* hashtable_config = hashtable_config_init();

        REQUIRE(hashtable_config != NULL);

        hashtable_config_free(hashtable_config);
    }
}
