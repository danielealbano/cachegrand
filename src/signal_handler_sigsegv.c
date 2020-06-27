#include <stdlib.h>
#include <signal.h>

#include "misc.h"
#include "log.h"
#include "fatal.h"
#include "signal_handler_sigsegv.h"

LOG_PRODUCER_CREATE_LOCAL_DEFAULT("signal_handler/sigsegv", signal_handler_sigsegv)

void signal_handler_sigsegv_handler(int sig) {
    fatal(LOG_PRODUCER_DEFAULT, "Error: signal %d:\n", sig);
}

void signal_handler_sigsegv_init() {
#ifndef DEBUG
    signal(SIGSEGV, signal_handler_sigsegv_handler);
#endif
}
