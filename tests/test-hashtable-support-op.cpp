#include "catch.hpp"

#include "exttypes.h"
#include "spinlock.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_data.h"
#include "hashtable/hashtable_support_op.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable_support_op.c", "[hashtable][hashtable_support_op]") {
    SECTION("hashtable_support_op_search_key") {
        CHECK_NOFAIL(false);
    }

    SECTION("hashtable_support_op_search_key_or_create_new") {
        CHECK_NOFAIL(false);
    }
}
