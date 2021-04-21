#include <catch2/catch.hpp>
#include <numa.h>

#include "exttypes.h"
#include "spinlock.h"

#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/mcmp/hashtable_data.h"
#include "data_structures/hashtable/mcmp/hashtable_support_op.h"

#include "fixtures-hashtable.h"

TEST_CASE("hashtable/hashtable_support_op.c", "[hashtable][hashtable_support_op]") {
    SECTION("hashtable_mcmp_support_op_search_key") {
        CHECK_NOFAIL(false);
    }

    SECTION("hashtable_mcmp_support_op_search_key_or_create_new") {
        CHECK_NOFAIL(false);
    }
}
