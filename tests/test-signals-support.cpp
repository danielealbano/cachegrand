#include <catch2/catch.hpp>

#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include "signals_support.h"

//void signals_support_register_sigsegv_fatal_handler();

sigjmp_buf jump_fp_signals_support;
void test_signals_support_signal_sigabrt_handler_longjmp(int signal_number) {
    siglongjmp(jump_fp_signals_support, 1);
}

void test_signals_support_setup_sigabrt_signal_handler() {
    signals_support_register_signal_handler(
            SIGABRT,
            test_signals_support_signal_sigabrt_handler_longjmp,
            NULL);
}

int test_signals_support_signal_handler_signal_number_received = -1;
void test_signals_support_signal_handler(int signal_number) {
    test_signals_support_signal_handler_signal_number_received = signal_number;
}

TEST_CASE("signals_support.c", "[signals_support]") {
    SECTION("signals_support_name") {
        SECTION("valid signal") {
            char *signal_name;
            char buffer[SIGNALS_SUPPORT_NAME_BUFFER_SIZE];

            signal_name = signals_support_name(SIGABRT, buffer, sizeof(buffer));

            REQUIRE(signal_name == buffer);
            REQUIRE(signal_name != unknown_signal_name);
            REQUIRE(strncmp(buffer, "Aborted", sizeof(buffer) - 1) == 0);
        }

        SECTION("not enough space") {
            char *signal_name;
            char buffer[5];

            signal_name = signals_support_name(SIGABRT, buffer, sizeof(buffer));

            REQUIRE(signal_name == buffer);
            REQUIRE(signal_name != unknown_signal_name);
            REQUIRE(strncmp(buffer, "Abor", sizeof(buffer) - 1) == 0);
        }

        SECTION("all signals") {
            char *signal_name;
            char buffer[SIGNALS_SUPPORT_NAME_BUFFER_SIZE];

            for(uint8_t signal_number = 0; signal_number < NSIG; signal_number++) {
                signal_name = signals_support_name(SIGABRT, buffer, sizeof(buffer));

                REQUIRE(signal_name == buffer);
                REQUIRE(signal_name != unknown_signal_name);
            }
        }

        SECTION("invalid signal") {
            char *signal_name;
            char buffer[SIGNALS_SUPPORT_NAME_BUFFER_SIZE];

            signal_name = signals_support_name(NSIG, buffer, sizeof(buffer));

            REQUIRE(signal_name == NULL);
        }
    }

    SECTION("signals_support_register_signal_handler") {
        SECTION("register signal handler") {
            struct sigaction previous_action = {0};
            test_signals_support_signal_handler_signal_number_received = -1;
            bool res = signals_support_register_signal_handler(
                    SIGCHLD,
                    test_signals_support_signal_handler,
                    &previous_action);

            REQUIRE(kill(0, SIGCHLD) == 0);

            // Restore original signal handler
            REQUIRE(sigaction(SIGCHLD, &previous_action, NULL) == 0);
            REQUIRE(res);
            REQUIRE(test_signals_support_signal_handler_signal_number_received == SIGCHLD);
        }

        SECTION("register signal handler and restore") {
            struct sigaction previous_action = {0};
            test_signals_support_signal_handler_signal_number_received = -1;
            bool res = signals_support_register_signal_handler(
                    SIGALRM,
                    test_signals_support_signal_handler,
                    &previous_action);

            REQUIRE(kill(0, SIGALRM) == 0);

            // Restore original signal handler
            REQUIRE(sigaction(SIGCHLD, &previous_action, NULL) == 0);
            REQUIRE(res);
            REQUIRE(test_signals_support_signal_handler_signal_number_received == SIGALRM);

            // Test restore original signal handler via function
            REQUIRE(signals_support_register_signal_handler(
                    SIGALRM,
                    test_signals_support_signal_handler,
                    &previous_action));
        }

        SECTION("invalid signal number") {
            struct sigaction previous_action = {0};
            bool res = signals_support_register_signal_handler(
                    NSIG,
                    test_signals_support_signal_handler,
                    &previous_action);

            REQUIRE(!res);
        }
    }

    SECTION("signals_support_handler_sigsegv_fatal") {
        bool fatal_catched = false;

        if (sigsetjmp(jump_fp_signals_support, 1) == 0) {
            test_signals_support_setup_sigabrt_signal_handler();
            signals_support_handler_sigsegv_fatal(SIGINT);
        } else {
            fatal_catched = true;
        }

        REQUIRE(fatal_catched);
    }

    SECTION("signals_support_register_sigsegv_fatal_handler") {
        struct sigaction action = {0};
        struct sigaction original_action = {0};

        // Fetch the original action for the SIGSEGV signal
        REQUIRE(sigaction(SIGSEGV, NULL, &original_action) == 0);

        // Update the signal handler
        signals_support_register_sigsegv_fatal_handler();

        // Fetch the action
        REQUIRE(sigaction(SIGSEGV, NULL, &action) == 0);
        REQUIRE((action.sa_handler == signals_support_handler_sigsegv_fatal));

        // Restore the original action
        REQUIRE(sigaction(SIGSEGV, &original_action, NULL) == 0);
    }
}
