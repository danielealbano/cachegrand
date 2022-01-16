/**
 * Copyright (C) 2020-2021 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>

#include "intrinsics.h"

uint64_t intrinsic_rdtscp(uint32_t *aux) {
    uint64_t rax, rdx;
    asm volatile (
            "rdtscp\n"
            : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32) + rax;
}
