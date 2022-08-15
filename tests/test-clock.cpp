/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>

#include "clock.h"

TEST_CASE("clock.c", "[clock]") {
    SECTION("clock_monotonic") {
        timespec_t a, b;
        clock_monotonic(&a);

        REQUIRE(clock_gettime(CLOCK_MONOTONIC, &b) == 0);
        REQUIRE(b.tv_sec >= a.tv_sec);
    }

    SECTION("clock_monotonic_coarse") {
        timespec_t a, b;
        clock_monotonic_coarse(&a);

        REQUIRE(clock_gettime(CLOCK_MONOTONIC_COARSE, &b) == 0);
        REQUIRE(b.tv_sec >= a.tv_sec);
    }

    SECTION("clock_realtime") {
        timespec_t a, b;
        clock_realtime(&a);

        REQUIRE(clock_gettime(CLOCK_REALTIME, &b) == 0);
        REQUIRE(b.tv_sec >= a.tv_sec);
    }

    SECTION("clock_realtime_coarse") {
        timespec_t a, b;
        clock_realtime_coarse(&a);

        REQUIRE(clock_gettime(CLOCK_REALTIME_COARSE, &b) == 0);
        REQUIRE(b.tv_sec >= a.tv_sec);
    }


    int64_t clock_timespec_to_int64_ms(
            timespec_t *timespec);

    SECTION("clock_timespec_to_int64_ms") {
        SECTION("ns less than 1 ms") {
            timespec_t timespec = {
                    .tv_sec = 1234,
                    .tv_nsec = 4321,
            };

            REQUIRE(clock_timespec_to_int64_ms(&timespec) == 1234000);
        }

        SECTION("ns more than 1 ms") {
            timespec_t timespec = {
                    .tv_sec = 1234,
                    .tv_nsec = 7654321,
            };

            REQUIRE(clock_timespec_to_int64_ms(&timespec) == 1234007);
        }

        SECTION("0 ns") {
            timespec_t timespec = {
                    .tv_sec = 1234,
                    .tv_nsec = 0,
            };

            REQUIRE(clock_timespec_to_int64_ms(&timespec) == 1234000);
        }

        SECTION("0 s, ns less than 1 ms") {
            timespec_t timespec = {
                    .tv_sec = 0,
                    .tv_nsec = 4321,
            };

            REQUIRE(clock_timespec_to_int64_ms(&timespec) == 0);

        }

        SECTION("0 s, ns more than 1 ms") {
            timespec_t timespec = {
                    .tv_sec = 0,
                    .tv_nsec = 7654321,
            };

            REQUIRE(clock_timespec_to_int64_ms(&timespec) == 7);

        }

        SECTION("0 s and 0 ns") {
            timespec_t timespec = {
                    .tv_sec = 0,
                    .tv_nsec = 0,
            };

            REQUIRE(clock_timespec_to_int64_ms(&timespec) == 0);
        }
    }

    SECTION("clock_monotonic_int64_ms") {
        timespec_t a;
        clock_monotonic(&a);
        int64_t b = clock_monotonic_int64_ms();

        // Allow up to 1ms of difference although these 2 clock reads are executed right after so there shouldn't be any
        int64_t diff = b - clock_timespec_to_int64_ms(&a);
        REQUIRE(((diff == 0) || (diff == 1)));
    }

    SECTION("clock_realtime_int64_ms") {
        timespec_t a;
        clock_realtime(&a);
        int64_t b = clock_realtime_int64_ms();

        // Allow up to 1ms of difference although these 2 clock reads are executed right after so there shouldn't be any
        int64_t diff = b - clock_timespec_to_int64_ms(&a);
        REQUIRE(((diff == 0) || (diff == 1)));
    }

    SECTION("clock_monotonic_coarse_int64_ms") {
        timespec_t a, res;
        clock_monotonic_coarse(&a);
        int64_t b = clock_monotonic_coarse_int64_ms();

        clock_getres(CLOCK_MONOTONIC_COARSE, &res);
        int64_t res_ms = clock_timespec_to_int64_ms(&res);

        // Allow up to res in ms of difference
        int64_t diff = (b - clock_timespec_to_int64_ms(&a)) + res_ms;
        REQUIRE((diff >= 0 && diff <= res_ms * 2));
    }

    SECTION("clock_realtime_coarse_int64_ms") {
        timespec_t a, res;
        clock_realtime_coarse(&a);
        int64_t b = clock_realtime_coarse_int64_ms();

        clock_getres(CLOCK_REALTIME_COARSE, &res);
        int64_t res_ms = clock_timespec_to_int64_ms(&res);

        // Allow up to res in ms of difference
        int64_t diff = (b - clock_timespec_to_int64_ms(&a)) + res_ms;
        REQUIRE((diff >= 0 && diff <= res_ms * 2));
    }

    SECTION("clock_diff") {
        timespec_t diff;
        timespec_t a = {
                .tv_sec = 1234,
                .tv_nsec = 4321,
        };

        timespec_t b = {
                .tv_sec = 4321,
                .tv_nsec = 9876,
        };

        SECTION("a > b") {
            clock_diff(&a, &b, &diff);

            REQUIRE(diff.tv_sec == -3088);
            REQUIRE(diff.tv_nsec == 999994445);
        }
        SECTION("a == b") {
            clock_diff(&a, &a, &diff);

            REQUIRE(diff.tv_sec == 0);
            REQUIRE(diff.tv_nsec == 0);
        }
        SECTION("a < b") {
            clock_diff(&b, &a, &diff);

            REQUIRE(diff.tv_sec == 3087);
            REQUIRE(diff.tv_nsec == 5555);
        }
    }
}
