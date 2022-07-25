/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "spinlock.h"
#include "clock.h"
#include "config.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "slab_allocator.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"

#include "module_redis_command.h"

bool module_redis_command_is_key_too_long(
        network_channel_t *channel,
        size_t key_length) {
    if (unlikely(key_length > channel->module_config->redis->max_key_length)) {
        return true;
    }

    return false;
}

module_redis_command_context_t* module_redis_command_alloc_context(
        module_redis_command_info_t *command_info) {
    return slab_allocator_mem_alloc_zero(command_info->context_size);
}

bool module_redis_command_free_context_free_argument_value_needs_free(
        module_redis_command_argument_type_t argument_type) {
    return argument_type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY ||
           argument_type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_STRING ||
           argument_type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN;
}

void module_redis_command_free_context_free_argument_value(
        module_redis_command_argument_type_t argument_type,
        void *argument_context) {
    if (argument_type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY) {
        // The key is basically a struct which contains a string and a length, so it's necessary to identify
        // the correct field containing the pointer to free (although should always be the first, using
        // offsetof guarantees that if things change and this code doesn't get up to date we will catch it)
        char **key = argument_context + offsetof(module_redis_key_t, key);
        if (*key != NULL) {
            slab_allocator_mem_free(*key);
        }
    } else if (argument_type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN) {
        // The pattern is a string, as the key, but the struct containing it is different so a different
        // if is necessary
        char **pattern = argument_context + offsetof(module_redis_pattern_t, pattern);
        if (*pattern != NULL) {
            slab_allocator_mem_free(*pattern);
        }
    } else if (argument_type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_STRING) {
        // The argument type string is actually a sequence of chunks and
        // TODO
        assert(0);
    }
}

void module_redis_command_free_context_free_argument(
        module_redis_command_argument_t *argument,
        void *argument_context_base_addr) {
    bool argument_is_list = false;

    if (argument->has_multiple_occurrences) {
        argument_is_list = true;
    }

    if (argument_is_list) {
        // If the argument is a list, the struct contains always first the list (which is the allocated memory) and
        // then the count
        void **list = argument_context_base_addr + argument->argument_context_offset;
        int *count = argument_context_base_addr + argument->argument_context_offset + sizeof(void*);

        if (*count == 0) {
            return;
        }

        // If count is not zero list should always be allocated
        assert(*list != NULL);

        if (module_redis_command_free_context_free_argument_value_needs_free(argument->type)) {
            for (int index = 0; index < *count; index++) {
                void *list_item = list[index];
                module_redis_command_free_context_free_argument_value(
                        argument->type,
                        list_item);
            }
        }

        slab_allocator_mem_free(*list);
        *list = NULL;
        *count = 0;
    } else {
        if (module_redis_command_free_context_free_argument_value_needs_free(argument->type)) {
            module_redis_command_free_context_free_argument_value(
                    argument->type,
                    argument_context_base_addr + argument->argument_context_offset);
        }
    }
}

void module_redis_command_free_context_free_arguments(
        module_redis_command_argument_t *arguments,
        int arguments_count,
        void *argument_context_base_addr) {
    for(int argument_index = 0; argument_index < arguments_count; argument_index++) {
        module_redis_command_free_context_free_argument(
                &arguments[argument_index],
                argument_context_base_addr);
    }
}

void module_redis_command_free_context(
        module_redis_command_info_t *command_info,
        module_redis_command_context_t *command_context) {
    module_redis_command_free_context_free_arguments(
            command_info->arguments,
            command_info->arguments_count,
            command_context);

    slab_allocator_mem_free(command_context);
}
