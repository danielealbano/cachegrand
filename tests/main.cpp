#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <sys/types.h>
#include <unistd.h>

#include "misc.h"
#include "log/log.h"
#include "signals_support.h"
#include <log/sink/log_sink.h>
#include <log/sink/log_sink_console.h>

#pragma GCC diagnostic ignored "-Wwrite-strings"

#define TAG "tests_main"

void tests_init_logging() {
    // Enable logging
    log_level_t level = LOG_LEVEL_ALL;
    log_sink_settings_t settings = { 0 };
    settings.console.use_stdout_for_errors = false;

#if NDEBUG == 1
    level -= LOG_LEVEL_DEBUG;
#endif

    log_sink_register(log_sink_console_init(level, &settings));
}

// It's necessary to setup the sigsegv fatal handler and to setup our own process group so it's not possible to use
// the default main function provided by Catch2
int main(int argc, char* argv[]) {
    Catch::Session session;

    // Switch to it's own process process group to avoid propagating the signals to the parent
    if (setpgid(getpid(),getpid()) != 0) {
        LOG_E(TAG, "Failed to change process group");
        LOG_E_OS_ERROR(TAG);
    }

    int returnCode = session.applyCommandLine( argc, argv );
    if( returnCode != 0 ) {
        return returnCode;
    }

    return session.run();
}