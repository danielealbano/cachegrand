#include <signal.h>

#include "fatal.h"
#include "signal_handler_sigsegv.h"

static const char* TAG = "signal_handler/sigsegv";

void signal_handler_sigsegv_handler(int sig) {
    fatal(TAG, "Error: signal %d:\n", sig);
}

void signal_handler_sigsegv_init() {
#ifndef DEBUG
    signal(SIGSEGV, signal_handler_sigsegv_handler);
#endif
}
