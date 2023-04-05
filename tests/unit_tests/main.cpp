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

#include "misc.h"
#include "exttypes.h"
#include "thread.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "memory_allocator/ffma_region_cache.h"
#include "memory_allocator/ffma_region_cache.h"
#include "memory_allocator/ffma.h"
#include "memory_allocator/ffma_thread_cache.h"

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
