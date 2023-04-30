/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "misc.h"
#include "config.h"
#include "xalloc.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/network.h"

#include "module.h"

module_t *modules_registered_list = NULL;
uint32_t modules_registered_list_size = 0;

module_id_t module_register(
        const char *name,
        const char *config_section_name,
        module_config_validate_after_load_t *config_validate_after_load,
        module_connection_accept_t *connection_accept) {
    modules_registered_list_size++;
    modules_registered_list = xalloc_realloc(
            modules_registered_list,
            sizeof(module_t) * modules_registered_list_size);

    modules_registered_list[modules_registered_list_size - 1] = (module_t) {
            .id = modules_registered_list_size - 1,
            .name = name,
            .config_type_name = config_section_name,
            .config_validate_after_load = config_validate_after_load,
            .connection_accept = connection_accept,
    };

    return modules_registered_list_size - 1;
}
