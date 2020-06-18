#include "catch.hpp"

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"

#include "fixtures-hashtable.h"

TEST_CASE("exttypes.h", "[exttypes]") {
    SECTION("sizeof(uint8_atomic_t) == 1") {
        REQUIRE(sizeof(uint8_volatile_t) == 1);
    }
}
