/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "misc.h"
#include "log/log.h"
#include "memory_fences.h"
#include "xalloc.h"

#include "signal_handler_thread.h"

void* test_signal_handler_thread_main_loop(
        void *user_data) {
    struct timespec timeout = { 0 };
    sigset_t *waitset = (sigset_t*)user_data;

    signal_handler_thread_setup_timeout(&timeout);
    signal_handler_thread_main_loop(waitset, &timeout);

    return NULL;
}

TEST_CASE("signal_handler_thread.c", "[signal_handler_thread]") {
    // The testing relies on having the signal_thread_handler using these signals (signals testing
    // is always very tricky) therefore it's important to have a failure to force the tests to get
    // reviewed and updated as needed.
    SECTION("signal_handler_thread_managed_signals") {
        REQUIRE(signal_handler_thread_managed_signals_count == 5);
        REQUIRE(signal_handler_thread_managed_signals[0] == SIGUSR1);
        REQUIRE(signal_handler_thread_managed_signals[1] == SIGINT);
        REQUIRE(signal_handler_thread_managed_signals[2] == SIGHUP);
        REQUIRE(signal_handler_thread_managed_signals[3] == SIGTERM);
        REQUIRE(signal_handler_thread_managed_signals[4] == SIGQUIT);
    }

    SECTION("signal_handler_thread_should_terminate") {
        SECTION("should not terminate") {
            bool terminate_event_loop = false;
            signal_handler_thread_context_t context = {
                    .terminate_event_loop = &terminate_event_loop
            };

            REQUIRE(signal_handler_thread_should_terminate(&context) == terminate_event_loop);
        }

        SECTION("should terminate") {
            bool terminate_event_loop = true;
            signal_handler_thread_context_t context = {
                    .terminate_event_loop = &terminate_event_loop
            };

            REQUIRE(signal_handler_thread_should_terminate(&context) == terminate_event_loop);
        }
    }

    SECTION("signal_handler_thread_log_producer_set_early_prefix_thread") {
        char prefix_cmp[] = SIGNAL_HANDLER_THREAD_LOG_PRODUCER_PREFIX_FORMAT_STRING;
        char *prefix = signal_handler_thread_log_producer_set_early_prefix_thread();

        REQUIRE(prefix != NULL);
        REQUIRE(strncmp(prefix, prefix_cmp, sizeof(prefix_cmp)) == 0);
        REQUIRE(log_get_early_prefix_thread() == prefix);

        log_unset_early_prefix_thread();
    }

    SECTION("signal_handler_thread_setup_timeout") {
        struct timespec timeout = { 0 };
        signal_handler_thread_setup_timeout(&timeout);

        REQUIRE(timeout.tv_sec == 0);
        REQUIRE(timeout.tv_nsec == SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS * 1000000);
    }

    SECTION("signal_handler_thread_set_thread_name") {
        char thread_name_cmp[] = SIGNAL_HANDLER_THREAD_NAME;
        char thread_name_out[100] = { 0 };

        signal_handler_thread_set_thread_name();

        REQUIRE(pthread_getname_np(pthread_self(), thread_name_out, sizeof(thread_name_out)) == 0);
        REQUIRE(strncmp(thread_name_cmp, thread_name_out, sizeof(thread_name_cmp)) == 0);
    }

    SECTION("signal_handler_thread_handle_signal") {
        bool terminate_event_loop = false;
        signal_handler_thread_context_t context = {
                .terminate_event_loop = &terminate_event_loop
        };
        signal_handler_thread_internal_context = &context;

        SECTION("test managed signals") {
            for(uint8_t i = 0; i < signal_handler_thread_managed_signals_count; i++) {
                signal_handler_thread_handle_signal(signal_handler_thread_managed_signals[i]);
                REQUIRE(terminate_event_loop == true);
            }
        }

        SECTION("test unmanaged signal") {
            signal_handler_thread_handle_signal(SIGCHLD);
            REQUIRE(terminate_event_loop == false);
        }

        signal_handler_thread_internal_context = NULL;
    }

    SECTION("signal_handler_thread_register_signal_handlers") {
        sigset_t waitset = { 0 };

        REQUIRE(signal_handler_thread_managed_signals_count == 5);

        signal_handler_thread_register_signal_handlers(&waitset);

        // Fetch the current actions to restore them later
        struct sigaction *previous_actions = (struct sigaction *)malloc(
                sizeof(struct sigaction) * signal_handler_thread_managed_signals_count);
        for (uint8_t i = 0; i < signal_handler_thread_managed_signals_count; i++) {
            sigaction(signal_handler_thread_managed_signals[i], NULL, &previous_actions[i]);
        }

        for (uint8_t i = 0; i < signal_handler_thread_managed_signals_count; i++) {
            struct sigaction current_action = { 0 };
            int signal = signal_handler_thread_managed_signals[i];
            sigaction(signal, NULL, &current_action);

            REQUIRE(current_action.sa_handler == signal_handler_thread_handle_signal);
            REQUIRE(sigismember(&waitset, signal) == 1);
        }

        // Restore the actions and cleanup the memory
        for (uint8_t i = 0; i < signal_handler_thread_managed_signals_count; i++) {
            sigaction(signal_handler_thread_managed_signals[i], &previous_actions[i], NULL);
        }
        free(previous_actions);
    }

    SECTION("signal_handler_thread_mask_signals") {
        sigset_t new_waitset = { 0 };
        sigset_t current_waitset = { 0 };

        REQUIRE(sigaddset(&new_waitset, SIGUSR1) == 0);

        signal_handler_thread_mask_signals(&new_waitset);

        REQUIRE(sigprocmask(SIG_BLOCK, NULL, &current_waitset) == 0);
        REQUIRE(sigismember(&current_waitset, SIGUSR1) == 1);
        REQUIRE(sigprocmask(SIG_UNBLOCK, &new_waitset, NULL) == 0);
    }

    SECTION("signal_handler_thread_teardown") {
        int previous_signal_handler_thread_managed_signals_0 =
                signal_handler_thread_managed_signals[0];
        int previous_signal_handler_thread_managed_signals_count =
                signal_handler_thread_managed_signals_count;
        sigset_t new_waitset = { 0 };
        sigset_t current_waitset = { 0 };
        REQUIRE(sigaddset(&new_waitset, SIGCHLD) == 0);
        char *test_log_prefix = (char*)xalloc_alloc(8);
        struct sigaction current_action, set_action_ignore;

        set_action_ignore.sa_handler = SIG_IGN;

        REQUIRE(sigaction(SIGCHLD, &set_action_ignore, NULL) == 0);
        REQUIRE(sigprocmask(SIG_BLOCK, &new_waitset, NULL) == 0);

        SECTION("signals restored") {
            signal_handler_thread_managed_signals[0] = SIGCHLD;
            signal_handler_thread_managed_signals_count = 1;

            REQUIRE(signal_handler_thread_teardown(
                    &new_waitset,
                    test_log_prefix) == true);

            REQUIRE(sigaction(SIGCHLD, NULL, &current_action) == 0);
            REQUIRE(sigprocmask(SIG_BLOCK, NULL, &current_waitset) == 0);

            REQUIRE(current_action.sa_handler == SIG_DFL);
            REQUIRE(sigismember(&current_waitset, SIGCHLD) == 0);
        }

        signal_handler_thread_managed_signals[0] =
                previous_signal_handler_thread_managed_signals_0;
        signal_handler_thread_managed_signals_count =
                previous_signal_handler_thread_managed_signals_count;
    }

    SECTION("signal_handler_thread_main_loop") {
        sigset_t waitset = { 0 };
        bool terminate_event_loop = false;
        signal_handler_thread_context_t context = {
                .terminate_event_loop = &terminate_event_loop,
        };
        signal_handler_thread_internal_context = &context;

        REQUIRE(sigaddset(&waitset, SIGCHLD) == 0);

        REQUIRE(pthread_create(
                &context.pthread,
                NULL,
                test_signal_handler_thread_main_loop,
                &waitset) == 0);

        usleep(25000);
        sched_yield();

        SECTION("teardown with terminate_event_loop") {
            terminate_event_loop = true;
            MEMORY_FENCE_STORE();

            usleep((SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
            sched_yield();

            REQUIRE(pthread_join(context.pthread, NULL) == 0);

            usleep((SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
            sched_yield();
        }

        signal_handler_thread_internal_context = NULL;
    }

    SECTION("signal_handler_thread_func") {
        int previous_signal_handler_thread_managed_signals_0 =
                signal_handler_thread_managed_signals[0];
        int previous_signal_handler_thread_managed_signals_count =
                signal_handler_thread_managed_signals_count;

        bool terminate_event_loop = false;
        signal_handler_thread_context_t context = {
                .terminate_event_loop = &terminate_event_loop,
        };

        signal_handler_thread_managed_signals[0] = SIGCHLD;
        signal_handler_thread_managed_signals_count = 1;

        REQUIRE(pthread_create(
                &context.pthread,
                NULL,
                signal_handler_thread_func,
                &context) == 0);

        usleep(25000);
        sched_yield();

        SECTION("teardown with terminate_event_loop") {
            terminate_event_loop = true;
            MEMORY_FENCE_STORE();

            usleep((SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
            sched_yield();

            REQUIRE(pthread_join(context.pthread, NULL) == 0);
        }

        SECTION("teardown with signal") {
            REQUIRE(kill(0, SIGCHLD) == 0);

            usleep((SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS + 100) * 1000);
            sched_yield();

            REQUIRE(pthread_join(context.pthread, NULL) == 0);
        }

        signal_handler_thread_managed_signals[0] =
                previous_signal_handler_thread_managed_signals_0;
        signal_handler_thread_managed_signals_count =
                previous_signal_handler_thread_managed_signals_count;
    }
}
