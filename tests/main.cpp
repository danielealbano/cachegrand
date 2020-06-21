#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "signal_handler_sigsegv.h"

int main(int argc, char* argv[]) {
    Catch::Session session;

    signal_handler_sigsegv_init();

    int returnCode = session.applyCommandLine( argc, argv );
    if( returnCode != 0 ) {
        return returnCode;
    }

    return session.run();
}