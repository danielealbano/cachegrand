#include "catch.hpp"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_support.h"

TEST_CASE("hashtable_support.c", "[hashtable][hashtable_support") {
    // Shared Test data
    char test_key[] = "cachegrand v2"; // hash(test_key) == 4804135818922944713
    uint64_t test_hash = 4804135818922944713;
    hashtable_bucket_index_t test_index_53 = 51;
    hashtable_bucket_index_t test_index_101 = 77;
    hashtable_bucket_index_t test_index_307 = 20;

    uint64_t buckets_initial_count_5 = 5;
    uint64_t buckets_initial_count_100 = 100;
    uint64_t buckets_initial_count_305 = 305;

    uint64_t buckets_count_53 = 53;
    uint64_t buckets_count_101 = 101;
    uint64_t buckets_count_307 = 307;

    uint64_t buckets_count_real_64 = 64;
    uint64_t buckets_count_real_112 = 112;
    uint64_t buckets_count_real_320 = 320;

    SECTION("hashtable_primenumbers_next") {
        SECTION("allowed values") {
            REQUIRE(hashtable_primenumbers_next(53 - 1) == 53);
            REQUIRE(hashtable_primenumbers_next(101 - 1) == 101);
            REQUIRE(hashtable_primenumbers_next(307 - 1) == 307);
            REQUIRE(hashtable_primenumbers_next(677 - 1) == 677);
            REQUIRE(hashtable_primenumbers_next(1523 - 1) == 1523);
            REQUIRE(hashtable_primenumbers_next(3389 - 1) == 3389);
            REQUIRE(hashtable_primenumbers_next(7639 - 1) == 7639);
            REQUIRE(hashtable_primenumbers_next(17203 - 1) == 17203);
            REQUIRE(hashtable_primenumbers_next(26813 - 1) == 26813);
            REQUIRE(hashtable_primenumbers_next(40231 - 1) == 40231);
            REQUIRE(hashtable_primenumbers_next(60353 - 1) == 60353);
            REQUIRE(hashtable_primenumbers_next(90529 - 1) == 90529);
            REQUIRE(hashtable_primenumbers_next(135799 - 1) == 135799);
            REQUIRE(hashtable_primenumbers_next(203713 - 1) == 203713);
            REQUIRE(hashtable_primenumbers_next(305581 - 1) == 305581);
            REQUIRE(hashtable_primenumbers_next(458377 - 1) == 458377);
            REQUIRE(hashtable_primenumbers_next(687581 - 1) == 687581);
            REQUIRE(hashtable_primenumbers_next(1031399 - 1) == 1031399);
            REQUIRE(hashtable_primenumbers_next(1547101 - 1) == 1547101);
            REQUIRE(hashtable_primenumbers_next(2320651 - 1) == 2320651);
            REQUIRE(hashtable_primenumbers_next(5221501 - 1) == 5221501);
            REQUIRE(hashtable_primenumbers_next(7832261 - 1) == 7832261);
            REQUIRE(hashtable_primenumbers_next(11748391 - 1) == 11748391);
            REQUIRE(hashtable_primenumbers_next(17622589 - 1) == 17622589);
            REQUIRE(hashtable_primenumbers_next(26433887 - 1) == 26433887);
            REQUIRE(hashtable_primenumbers_next(39650833 - 1) == 39650833);
            REQUIRE(hashtable_primenumbers_next(59476253 - 1) == 59476253);
            REQUIRE(hashtable_primenumbers_next(89214403 - 1) == 89214403);
            REQUIRE(hashtable_primenumbers_next(133821673 - 1) == 133821673);
            REQUIRE(hashtable_primenumbers_next(200732527 - 1) == 200732527);
            REQUIRE(hashtable_primenumbers_next(301098823 - 1) == 301098823);
            REQUIRE(hashtable_primenumbers_next(451648247 - 1) == 451648247);
            REQUIRE(hashtable_primenumbers_next(677472371 - 1) == 677472371);
            REQUIRE(hashtable_primenumbers_next(1016208581 - 1) == 1016208581);
            REQUIRE(hashtable_primenumbers_next(1524312899 - 1) == 1524312899);
            REQUIRE(hashtable_primenumbers_next(2286469357 - 1) == 2286469357);
            REQUIRE(hashtable_primenumbers_next(3429704039 - 1) == 3429704039);
            REQUIRE(hashtable_primenumbers_next(5144556059 - 1) == 5144556059);
        }

        SECTION("unsupported value") {
            REQUIRE(hashtable_primenumbers_next(5144556060) == 0);
        }
    }

    SECTION("hashtable_primenumbers_mod") {
        SECTION("allowed values") {
            uint64_t number = 5144556065;

            REQUIRE(hashtable_primenumbers_mod(number, 53) == 30);
            REQUIRE(hashtable_primenumbers_mod(number, 101) == 67);
            REQUIRE(hashtable_primenumbers_mod(number, 307) == 188);
            REQUIRE(hashtable_primenumbers_mod(number, 677) == 569);
            REQUIRE(hashtable_primenumbers_mod(number, 1523) == 658);
            REQUIRE(hashtable_primenumbers_mod(number, 3389) == 3230);
            REQUIRE(hashtable_primenumbers_mod(number, 7639) == 2764);
            REQUIRE(hashtable_primenumbers_mod(number, 17203) == 16118);
            REQUIRE(hashtable_primenumbers_mod(number, 26813) == 26194);
            REQUIRE(hashtable_primenumbers_mod(number, 40231) == 16940);
            REQUIRE(hashtable_primenumbers_mod(number, 60353) == 5992);
            REQUIRE(hashtable_primenumbers_mod(number, 90529) == 64582);
            REQUIRE(hashtable_primenumbers_mod(number, 135799) == 82548);
            REQUIRE(hashtable_primenumbers_mod(number, 203713) == 191676);
            REQUIRE(hashtable_primenumbers_mod(number, 305581) == 99930);
            REQUIRE(hashtable_primenumbers_mod(number, 458377) == 190994);
            REQUIRE(hashtable_primenumbers_mod(number, 687581) == 75023);
            REQUIRE(hashtable_primenumbers_mod(number, 1031399) == 969252);
            REQUIRE(hashtable_primenumbers_mod(number, 1547101) == 445240);
            REQUIRE(hashtable_primenumbers_mod(number, 2320651) == 1993449);
            REQUIRE(hashtable_primenumbers_mod(number, 5221501) == 1377580);
            REQUIRE(hashtable_primenumbers_mod(number, 7832261) == 6592849);
            REQUIRE(hashtable_primenumbers_mod(number, 11748391) == 10509198);
            REQUIRE(hashtable_primenumbers_mod(number, 17622589) == 16382666);
            REQUIRE(hashtable_primenumbers_mod(number, 26433887) == 16381987);
            REQUIRE(hashtable_primenumbers_mod(number, 39650833) == 29598608);
            REQUIRE(hashtable_primenumbers_mod(number, 59476253) == 29598307);
            REQUIRE(hashtable_primenumbers_mod(number, 89214403) == 59335094);
            REQUIRE(hashtable_primenumbers_mod(number, 133821673) == 59332491);
            REQUIRE(hashtable_primenumbers_mod(number, 200732527) == 126242890);
            REQUIRE(hashtable_primenumbers_mod(number, 301098823) == 25876074);
            REQUIRE(hashtable_primenumbers_mod(number, 451648247) == 176425348);
            REQUIRE(hashtable_primenumbers_mod(number, 677472371) == 402249468);
            REQUIRE(hashtable_primenumbers_mod(number, 1016208581) == 63513160);
            REQUIRE(hashtable_primenumbers_mod(number, 1524312899) == 571617368);
            REQUIRE(hashtable_primenumbers_mod(number, 2286469357) == 571617351);
            REQUIRE(hashtable_primenumbers_mod(number, 3429704039) == 1714852026);
            REQUIRE(hashtable_primenumbers_mod(number, 5144556059) == 6);
        }

        SECTION("unsupported value") {
            uint64_t number = 5144556065;

            REQUIRE(hashtable_primenumbers_mod(number, 5144556060) == 0);
        }
    }

    SECTION("hashtable_calculate_hash") {
        REQUIRE(hashtable_calculate_hash(
                test_key,
                sizeof(test_key)) == test_hash);
    }

    SECTION("hashtable_rounddown_to_cacheline") {
        REQUIRE(hashtable_rounddown_to_cacheline(buckets_initial_count_5) == 0);
        REQUIRE(hashtable_rounddown_to_cacheline(buckets_initial_count_100) == 96);
        REQUIRE(hashtable_rounddown_to_cacheline(buckets_initial_count_305) == 304);
    }

    SECTION("hashtable_roundup_to_cacheline_plus_one") {
        REQUIRE(hashtable_roundup_to_cacheline_plus_one(buckets_initial_count_5) == 16);
        REQUIRE(hashtable_roundup_to_cacheline_plus_one(buckets_initial_count_100) == 112);
        REQUIRE(hashtable_roundup_to_cacheline_plus_one(buckets_initial_count_305) == 320);
    }

    SECTION("hashtable_bucket_index_from_hash") {
        SECTION("buckets_initial_count_5") {
            REQUIRE(hashtable_bucket_index_from_hash(
                    buckets_count_53,
                    test_hash) == 13);
        }

        SECTION("buckets_initial_count_100") {
            REQUIRE(hashtable_bucket_index_from_hash(
                    buckets_count_101,
                    test_hash) == 15);
        }

        SECTION("buckets_initial_count_305") {
            REQUIRE(hashtable_bucket_index_from_hash(
                    buckets_count_307,
                    test_hash) == 292);
        }
    }

    SECTION("hashtable_calculate_neighborhood") {
        hashtable_bucket_index_t start, end;

        SECTION("buckets_count_53") {
            hashtable_calculate_neighborhood(
                    buckets_count_53,
                    test_hash,
                    &start,
                    &end);

            REQUIRE(start == 8);
            REQUIRE(end == 24);
        }

        SECTION("buckets_count_101") {
            hashtable_calculate_neighborhood(
                    buckets_count_101,
                    test_hash,
                    &start,
                    &end);

            REQUIRE(start == 8);
            REQUIRE(end == 24);
        }

        SECTION("buckets_initial_count_305") {
            hashtable_calculate_neighborhood(
                    buckets_count_307,
                    test_hash,
                    &start,
                    &end);

            REQUIRE(start == 288);
            REQUIRE(end == 304);
        }
    }
}
