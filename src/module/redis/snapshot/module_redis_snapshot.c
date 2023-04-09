/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>

#include "module_redis_snapshot.h"

bool module_redis_snapshot_is_value_type_valid(
        module_redis_snapshot_value_type_t value_type) {
    // There is no value type 8
    return value_type <= MODULE_REDIS_SNAPSHOT_VALUE_TYPE_MAX && value_type != 8;
}

bool module_redis_snapshot_is_value_type_supported(
        module_redis_snapshot_value_type_t value_type) {
#pragma unroll(MODULE_REDIS_SNAPSHOT_VALUES_TYPES_SUPPORTED_COUNT)
    for(int index = 0; index < MODULE_REDIS_SNAPSHOT_VALUES_TYPES_SUPPORTED_COUNT; index++) {
        if (module_redis_snapshot_rdb_values_types_supported[index] == value_type) {
            return true;
        }
    }

    return false;
}
