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

#include "misc.h"
#include "exttypes.h"
#include "config.h"
#include "module/module.h"
#include "log/log.h"

#include "module_redis_config.h"

#define TAG "module_redis_config"

bool module_redis_config_validate_after_load(
        config_module_t *module) {
    bool return_result = true;

    if (module->redis->max_key_length > UINT16_MAX - 1) {
        LOG_E(
                TAG,
                "In module <%s>, the allowed maximum value of max_key_length is <%u>",
                module->type,
                UINT16_MAX - 1);
        return_result = false;
    }

    return return_result;
}
