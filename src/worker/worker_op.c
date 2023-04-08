/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdbool.h>
#include <stdint.h>

#include "worker/worker_op.h"

#define TAG "worker_op"

worker_op_wait_fp_t* worker_op_wait;
worker_op_wait_ms_fp_t* worker_op_wait_ms;
