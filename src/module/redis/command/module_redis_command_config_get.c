/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include "misc.h"
#include "exttypes.h"
#include "clock.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_rwspinlock.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "memory_allocator/ffma_region_cache.h"
#include "memory_allocator/ffma.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "config.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module/redis/module_redis_connection.h"
#include "helpers/module_redis_command_helper_config.h"

#define TAG "module_redis_command_config_get"

MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END(config_get) {
    bool found_wildcard = false;
    uint64_t parameters_to_send_size = 0;
    uint64_t parameters_to_send_count = 0;
    module_redis_command_helper_config_parameter_key_value_t *parameters_to_send = NULL;

    module_redis_command_config_get_context_t *context = connection_context->command.context;

    // Loop over the parameters until it finishes or find a wildcard
    for(int index = 0; index < context->parameter.count; index++) {
        // If the parameter is a wildcard, we need to send all the parameters so break the current loop as the list has
        // to be rebuilt from scratch
        if (context->parameter.list[index].length == 1 && context->parameter.list[index].short_string[0] == '*') {
            found_wildcard = true;
            break;
        }

        // Try to get the parameter from the list of known parameters, if it doesn't exist, skip it
        module_redis_command_helper_config_parameter_key_value_t *parameter =
                module_redis_command_helper_config_parameter_get(
                        context->parameter.list[index].short_string,
                        context->parameter.list[index].length);

        if (parameter == NULL) {
            continue;
        }

        // Check if it's necessary to upsize the list of parameters to send
        if (parameters_to_send_count >= parameters_to_send_size) {
            if (parameters_to_send_size == 0) {
                parameters_to_send_size = 1;
            } else {
                parameters_to_send_size *= 2;
            }

            parameters_to_send = ffma_mem_realloc(
                    parameters_to_send,
                    parameters_to_send_size * sizeof(module_redis_command_helper_config_parameter_key_value_t));
        }

        // Add the parameter to the list of parameters to send
        parameters_to_send[parameters_to_send_count].name = parameter->name;
        parameters_to_send[parameters_to_send_count].to_handle = parameter->to_handle;

        // If the parameter has to be handle, call the appropriate function to get the value
        if (parameter->to_handle) {
            parameters_to_send[parameters_to_send_count].value =
                    module_redis_command_config_get_handle_parameter_value(
                            connection_context->config,
                            connection_context->network_channel->module_config,
                            connection_context->db->config,
                            parameter->name);
        } else {
            parameters_to_send[parameters_to_send_count].value = parameter->value;
        }

        parameters_to_send_count++;
    }

    // If wildcard, send all the parameters
    if (found_wildcard) {
        // All the parameters have to be sent so get the size of the array of the list of parameters
        parameters_to_send_count = ARRAY_SIZE(module_redis_command_helper_config_parameters);

        // Resize the list of parameters to send
        parameters_to_send = ffma_mem_realloc(
                parameters_to_send,
                parameters_to_send_count * sizeof(module_redis_command_helper_config_parameter_key_value_t));

        // Loop over the list of parameters and add them to the list of parameters to send
        for(
                uint64_t parameter_index = 0;
                parameter_index < parameters_to_send_count;
                parameter_index++) {
            module_redis_command_helper_config_parameter_key_value_t *parameter =
                    &module_redis_command_helper_config_parameters[parameter_index];

            // Add the parameter to the list of parameters to send
            parameters_to_send[parameter_index].name = parameter->name;
            parameters_to_send[parameter_index].to_handle = parameter->to_handle;

            // If the parameter has to be handle, call the appropriate function to get the value
            if (parameter->to_handle) {
                parameters_to_send[parameter_index].value =
                        module_redis_command_config_get_handle_parameter_value(
                                connection_context->config,
                                connection_context->network_channel->module_config,
                                connection_context->db->config,
                                parameter->name);
            } else {
                parameters_to_send[parameter_index].value = parameter->value;
            }
        }
    }

    // Send the response
    bool result = module_redis_command_helper_config_send_response(
            connection_context,
            parameters_to_send,
            parameters_to_send_count);

    // Free the list of parameters to send if it was allocated
    if (parameters_to_send != NULL) {
        // If the parameter was handled, free up the allocated memory for the value
        for(
                uint64_t parameter_index = 0;
                parameter_index < parameters_to_send_count;
                parameter_index++) {
            module_redis_command_helper_config_parameter_key_value_t *parameter = &parameters_to_send[parameter_index];

            if (parameter->to_handle) {
                ffma_mem_free(parameter->value);
            }
        }

        // Free the list of parameters to send
        ffma_mem_free(parameters_to_send);
    }

    return result;
}
