#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ptrace.h>

#include "log/log.h"
#include "memory_fences.h"
#include "xalloc.h"
#include "signals_support.h"
#include "thread.h"
#include "utils_numa.h"

#include "signal_handler_thread.h"
#include "fatal.h"

#define TAG "signal_handler_thread"

static signal_handler_thread_context_t *int_context;

int program_signals[] = { SIGUSR1, SIGINT, SIGHUP, SIGTERM, SIGQUIT };
uint8_t program_signals_count = sizeof(program_signals) / sizeof(int);

bool signal_handler_thread_running_under_debugger() {
    bool debugger_attached = false;
    if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
        debugger_attached = true;
        ptrace(PTRACE_DETACH, 0, 1, 0);
    }

    return debugger_attached;
}

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

    bool debugger_attached = signal_handler_thread_running_under_debugger();

    signal(SIGCHLD, SIG_IGN);

    for(uint8_t i = 0; i < program_signals_count; i++) {
//        // SIGINT is used by gdb to be notified when new breakpoints are added
//        if (debugger_attached && program_signals[i] == SIGINT) {
//            continue;
//        }

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

void* signal_handler_thread_func(
        void* user_data) {
    sigset_t waitset = { 0 };
    siginfo_t info = { 0 };
    struct timespec timeout;

    int_context = (signal_handler_thread_context_t*)user_data;

    char* log_producer_early_prefix_thread =
            signal_handler_thread_log_producer_set_early_prefix_thread(int_context);

    signal_handler_thread_register_signal_handlers(&waitset);
    sigprocmask(SIG_BLOCK, &waitset, NULL);

    // Use a timeout to ensure that the thread exit if it should terminate
    timeout.tv_sec = 0;
    timeout.tv_nsec = SIGNAL_HANDLER_THREAD_LOOP_MAX_WAIT_TIME_MS * 1000000;

    LOG_V(TAG, "Starting events loop");

    do {
        int res = sigtimedwait(&waitset, &info, &timeout);

        if (res == 0 && errno != EAGAIN) {
            FATAL(TAG, "Error while waiting for a signal");
        }
    } while(!signal_handler_thread_should_terminate(int_context));

    LOG_V(TAG, "Tearing signal handler thread");

    xalloc_free(log_producer_early_prefix_thread);

    return NULL;
}