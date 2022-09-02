/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>

#include "spinlock.h"

#define TAG "spinlock"

void spinlock_init(
        spinlock_lock_volatile_t* spinlock) {
    spinlock->lock = SPINLOCK_UNLOCKED;
}
