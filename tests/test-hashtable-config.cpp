#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_primenumbers.h"

TEST_CASE("hashtable_config.c", "[hashtable][hashtable_config]") {
    SECTION("hashtable_config_init") {
        hashtable_config_t* hashtable_config = hashtable_config_init();

        REQUIRE(hashtable_config != NULL);

        hashtable_config_free(hashtable_config);
    }

    SECTION("hashtable_config_prefill_cachelines_to_probe_with_defaults") {
        hashtable_config_t* hashtable_config = hashtable_config_init();

        hashtable_config_prefill_cachelines_to_probe_with_defaults(hashtable_config);
        hashtable_config_cachelines_to_probe_t* cachelines_to_probe_list = hashtable_config->cachelines_to_probe;

        HASHTABLE_PRIMENUMBERS_FOREACH(primenumbers, primenumbers_index, primenumber, {
            uint16_t cachelines_to_probe = 0;

            HASHTABLE_CONFIG_CACHELINES_PRIMENUMBERS_MAP_FOREACH(map, map_index, map_value, {
                cachelines_to_probe = map_value.cachelines_to_probe;
                if (map_value.hashtable_size >= primenumber) {
                    break;
                }
            })

            REQUIRE(cachelines_to_probe_list[primenumbers_index].hashtable_size == primenumber);
            REQUIRE(cachelines_to_probe_list[primenumbers_index].cachelines_to_probe > 0);
            REQUIRE(cachelines_to_probe_list[primenumbers_index].cachelines_to_probe == cachelines_to_probe);
        })

        hashtable_config_free(hashtable_config);
    }
}
