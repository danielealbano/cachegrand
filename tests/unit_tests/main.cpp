/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include <sys/types.h>
#include <unistd.h>

#include "thread.h"

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

    // Ensure that the current thread is pinned to the core 0 otherwise some tests can fail if the kernel shift around
    // the main thread of the process
    thread_current_set_affinity(0);

    return session.run();
}
