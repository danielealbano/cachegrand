#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include "signals_support.h"

int main(int argc, char* argv[]) {
    Catch::Session session;

    signals_support_register_sigsegv_fatal_handler();

    int returnCode = session.applyCommandLine( argc, argv );
    if( returnCode != 0 ) {
        return returnCode;
    }

    return session.run();
}