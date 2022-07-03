/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>

#include "intrinsics.h"

// TODO: this should be refactored as intrinsics are different per platform, instead of invoking directly
//       intrinsic_rdtscp a performance counters component should be implemented to fetch these information to be
//       able to provide a platform-agnostic set of them.

uint64_t intrinsic_rdtscp(uint32_t *aux) {
    uint64_t rax, rdx;
    asm volatile (
            "rdtscp\n"
            : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32) + rax;
}
