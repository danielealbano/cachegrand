#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <sys/types.h>
#include <unistd.h>

#pragma GCC diagnostic ignored "-Wwrite-strings"

// It's necessary to setup the sigsegv fatal handler and to setup our own process group so it's not possible to use
// the default main function provided by Catch2
int main(int argc, char* argv[]) {
    Catch::Session session;

    // Switch to it's own process process group to avoid propagating the signals to the parent
    setpgid(getpid(), getpid());

    int returnCode = session.applyCommandLine(argc, argv);
    if( returnCode != 0 ) {
        return returnCode;
    }

    return session.run();
}
