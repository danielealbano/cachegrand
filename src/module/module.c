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
#include <string.h>

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
        module_config_prepare_cb_t *config_prepare,
        module_config_validate_after_load_cb_t *config_validate_after_load,
        module_program_ctor_cb_t *program_ctor,
        module_program_dtor_cb_t *program_dtor,
        module_worker_ctor_cb_t *worker_ctor,
        module_worker_dtor_cb_t *worker_dtor,
        module_connection_accept_cb_t *connection_accept) {
    assert(name != NULL);
    assert(connection_accept != NULL);

    modules_registered_list_size++;
    modules_registered_list = xalloc_realloc(
            modules_registered_list,
            sizeof(module_t) * modules_registered_list_size);

    modules_registered_list[modules_registered_list_size - 1] = (module_t) {
            .id = modules_registered_list_size - 1,
            .name = name,
            .config_prepare = config_prepare,
            .config_validate_after_load = config_validate_after_load,
            .program_ctor = program_ctor,
            .program_dtor = program_dtor,
            .worker_ctor = worker_ctor,
            .worker_dtor = worker_dtor,
            .connection_accept = connection_accept,
    };

    return modules_registered_list_size - 1;
}

module_t* module_get_by_name(
        const char *name) {
    module_id_t module_id = 0;
    module_t *module = NULL;

    while((module = module_get_by_id(module_id++)) != NULL) {
        if (strcmp(module->name, name) != 0) {
            continue;
        }

        return module;
    }

    return NULL;
}
