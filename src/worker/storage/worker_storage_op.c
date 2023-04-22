/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"

#include "worker_storage_op.h"

// TODO: need to extend the interface to manage directories (open, iterate, flush, etc.)
// TODO: need to extend the interface to expose disk availability

// Storage operations
worker_op_storage_open_fp_t* worker_op_storage_open;
worker_op_storage_open_fd_fp_t* worker_op_storage_open_fd;
worker_op_storage_read_fp_t* worker_op_storage_read;
worker_op_storage_write_fp_t* worker_op_storage_write;
worker_op_storage_flush_fp_t* worker_op_storage_flush;
worker_op_storage_fallocate_fp_t* worker_op_storage_fallocate;
worker_op_storage_close_fp_t* worker_op_storage_close;