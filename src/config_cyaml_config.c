/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <cyaml/cyaml.h>

#include "config.h"
#include "config_cyaml_config.h"

cyaml_config_t cyaml_config_global = {
        .log_level = CYAML_LOG_ERROR,
        .log_fn = config_internal_cyaml_log,
        .mem_fn = cyaml_mem,
        .flags = CYAML_CFG_DEFAULT | CYAML_CFG_CASE_INSENSITIVE,
};

cyaml_config_t* config_cyaml_config_get_global() {
    return &cyaml_config_global;
}
