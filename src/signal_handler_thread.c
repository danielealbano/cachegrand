/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <pthread.h>

#include "misc.h"
#include "log/log.h"
#include "fatal.h"
#include "memory_fences.h"
#include "xalloc.h"
#include "signals_support.h"

#include "signal_handler_thread.h"

#define TAG "signal_handler_thread"

static signal_handler_thread_context_t *int_context;

int program_signals[] = { SIGUSR1, SIGINT, SIGHUP, SIGTERM, SIGQUIT };
uint8_t program_signals_count = sizeof(program_signals) / sizeof(int);

void signal_handler_thread_handle_signal(
        int signal_number) {
    char *signal_name = SIGNALS_SUPPORT_NAME_WRAPPER(signal_number);

    int found_sig_index = -1;
    for(uint8_t i = 0; i < program_signals_count; i++) {
        if (program_signals[i] == signal_number) {
            found_sig_index = i;
            break;
        }
    }

    if (found_sig_index == -1) {
        LOG_V(
                TAG,
                "Received un-managed signal <%s (%d)>, ignoring",
                signal_name,
                signal_number);
        return;
    }

    LOG_I(
            TAG,
            "Received signal <%s (%d)>, requesting loop termination",
            signal_name,
            signal_number);

    *int_context->terminate_event_loop = true;
    MEMORY_FENCE_STORE();
}

bool signal_handler_thread_should_terminate(
        signal_handler_thread_context_t *context) {
    MEMORY_FENCE_LOAD();
    return *context->terminate_event_loop;
}

void signal_handler_thread_register_signal_handlers(
        sigset_t *waitset) {
    struct sigaction action;
    action.sa_handler = signal_handler_thread_handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    signal(SIGCHLD, SIG_IGN);

    for(uint8_t i = 0; i < program_signals_count; i++) {
        sigaddset(waitset, program_signals[i]);
        signals_support_register_signal_handler(
                program_signals[i],
                signal_handler_thread_handle_signal,
                NULL);
    }
}

char* signal_handler_thread_log_producer_set_early_prefix_thread(
        signal_handler_thread_context_t *context) {
    size_t prefix_size = snprintf(
            NULL,
            0,
            SIGNAL_HANDLER_THREAD_LOG_PRODUCER_PREFIX_FORMAT_STRING) + 1;
    char *prefix = xalloc_alloc_zero(prefix_size);

    snprintf(
            prefix,
            prefix_size,
            SIGNAL_HANDLER_THREAD_LOG_PRODUCER_PREFIX_FORMAT_STRING);
    log_set_early_prefix_thread(prefix);

    return prefix;
}

void signal_handler_thread_main_loop(
        sigset_t *waitset,
        struct timespec *timeout) {
    siginfo_t info = { 0 };

    LOG_V(TAG, "Starting events loop");

    do {
        int res = sigtimedwait(waitset, &info, timeout);

        if (res == 0 && errno != EAGAIN) {
            FATAL(TAG, "Error while waiting for a signal");
        }
    } while(!signal_handler_thread_should_terminate(int_context));

    LOG_V(TAG, "Events loop ended");
}

void signal_handler_thread_setup_timeout(
        struct timespec *timeout) {

    // Use a timeout to ensure that the thread exit if it should terminate
    timeout->tv_sec = 0;
    timeout->tv_nsec = SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS * 1000000;
}

void signal_handler_thread_set_thread_name() {
    if (pthread_setname_np(
            pthread_self(),
            SIGNAL_HANDLER_THREAD_NAME) != 0) {
        LOG_E(TAG, "Unable to set name of the signal handler thread");
        LOG_E_OS_ERROR(TAG);
    }
}

void signal_handler_thread_mask_signals(
        sigset_t *waitset) {
    sigprocmask(SIG_BLOCK, waitset, NULL);
}

bool signal_handler_thread_teardown(
        sigset_t *waitset,
        char *log_producer_early_prefix_thread) {
    bool res = true;

    LOG_V(TAG, "Tearing down signal handler thread");

    xalloc_free(log_producer_early_prefix_thread);
    sigprocmask(SIG_UNBLOCK, waitset, NULL);

    for(uint8_t i = 0; i < signal_handler_thread_managed_signals_count; i++) {
        if (!signals_support_register_signal_handler(
                signal_handler_thread_managed_signals[i],
                SIG_DFL,
                NULL)) {
            res = false;
            LOG_E(TAG, "Unable to set signal back to default handler");
            LOG_E_OS_ERROR(TAG);
        }
    }

    return res;
}

void* signal_handler_thread_func(
        void* user_data) {
    struct timespec timeout;
    sigset_t waitset = { 0 };

    int_context = (signal_handler_thread_context_t*)user_data;

    char* log_producer_early_prefix_thread =
            signal_handler_thread_log_producer_set_early_prefix_thread(int_context);

    signal_handler_thread_set_thread_name();
    signal_handler_thread_register_signal_handlers(&waitset);
    signal_handler_thread_mask_signals(&waitset);
    signal_handler_thread_setup_timeout(&timeout);
    signal_handler_thread_main_loop(&waitset, &timeout);
    signal_handler_thread_teardown(log_producer_early_prefix_thread);

    return NULL;
}