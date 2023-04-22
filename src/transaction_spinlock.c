/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>

#include "misc.h"
#include "exttypes.h"

#include "transaction.h"
#include "transaction_spinlock.h"

#define TAG "transaction_spinlock"

void transaction_spinlock_init(
        transaction_spinlock_lock_volatile_t* spinlock) {
    spinlock->transaction_id = TRANSACTION_SPINLOCK_UNLOCKED;
}
