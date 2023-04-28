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
#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "misc.h"
#include "exttypes.h"
#include "log/log.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_spinlock.h"
#include "clock.h"
#include "config.h"
#include "xalloc.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"

#include "module_redis_commands.h"

#define TAG "module_redis_commands"

hashtable_spsc_t *module_redis_commands_build_commands_hashtables(
        module_redis_command_info_t *command_infos,
        uint32_t command_infos_count) {
    hashtable_spsc_t *commands_hashtable = hashtable_spsc_new(
            command_infos_count,
            32,
            false);
    for(
            uint32_t command_info_index = 0;
            command_info_index < command_infos_count;
            command_info_index++) {
        module_redis_command_info_t *command_info = &command_infos[command_info_index];

        if (!hashtable_spsc_op_try_set_ci(
                commands_hashtable,
                command_info->string,
                command_info->string_len,
                command_info)) {
            hashtable_spsc_free(commands_hashtable);
            return NULL;
        }
    }

    return commands_hashtable;
}

uint16_t module_redis_commands_count_tokens_in_command_arguments(
        module_redis_command_argument_t *arguments,
        uint16_t arguments_count) {
    uint16_t count = 0;

    for(uint16_t index = 0; index < arguments_count; index++) {
        module_redis_command_argument_t *argument = &arguments[index];
        if (argument->has_sub_arguments) {
            count += module_redis_commands_count_tokens_in_command_arguments(
                    argument->sub_arguments,
                    argument->sub_arguments_count);
        }

        if (!argument->token) {
            continue;
        }

        count++;
    }

    return count;
}

void module_redis_commands_build_command_argument_token_entry_oneof_tokens(
        module_redis_command_argument_t *argument,
        module_redis_command_parser_context_argument_token_entry_t *token_entry) {
    for(
            uint16_t parent_oneof_token_index = 0;
            parent_oneof_token_index < argument->parent_argument->sub_arguments_count;
            parent_oneof_token_index++) {
        char *one_of_token = argument->parent_argument->sub_arguments[parent_oneof_token_index].token;
        if (one_of_token && one_of_token != argument->token) {
            assert(token_entry->one_of_token_count <=
                   sizeof(token_entry->one_of_tokens) / sizeof(char*));
            token_entry->one_of_tokens[token_entry->one_of_token_count] = one_of_token;
            token_entry->one_of_token_count++;
        }
    }
}

module_redis_command_parser_context_argument_token_entry_t *module_redis_commands_build_command_argument_token_entry(
        module_redis_command_argument_t *argument) {
    module_redis_command_parser_context_argument_token_entry_t *token_entry =
            xalloc_alloc_zero(sizeof(module_redis_command_parser_context_argument_token_entry_t));

    if (!token_entry) {
        return NULL;
    }

    token_entry->token = argument->token;
    token_entry->token_length = strlen(argument->token);
    token_entry->argument = argument;

    if (argument->parent_argument &&
        argument->parent_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_ONEOF) {
        module_redis_commands_build_command_argument_token_entry_oneof_tokens(argument, token_entry);
    }

    return token_entry;
}

bool module_redis_commands_build_command_arguments_token_entries_hashtable(
        module_redis_command_argument_t *arguments,
        uint16_t arguments_count,
        hashtable_spsc_t *hashtable) {
    for(uint16_t index = 0; index < arguments_count; index++) {
        module_redis_command_parser_context_argument_token_entry_t *token_entry;
        module_redis_command_argument_t *argument = &arguments[index];

        if (argument->has_sub_arguments) {
            if (!module_redis_commands_build_command_arguments_token_entries_hashtable(
                    argument->sub_arguments,
                    argument->sub_arguments_count,
                    hashtable)) {
                return false;
            }
        }

        if (!argument->token) {
            continue;
        }

        if ((token_entry = module_redis_commands_build_command_argument_token_entry(argument)) == NULL) {
            LOG_E(TAG, "Failed to create the token entry for the token <%s>", argument->token);
            return false;
        }

        // These are fixed hashtable generated at the start, if the set fails the software must stop
        if (!hashtable_spsc_op_try_set_ci(
                hashtable,
                argument->token,
                strlen(argument->token),
                token_entry)) {
            LOG_E(TAG, "Failed to insert the token <%s> in the tokens hashtable", argument->token);
            return false;
        }
    }

    return true;
}

void module_redis_commands_free_command_arguments_token_entries_hashtable(
        module_redis_command_info_t *command_info) {
    void *value;
    hashtable_spsc_bucket_index_t bucket_index = 0;
    while((value = hashtable_spsc_op_iter(
            command_info->tokens_hashtable, &bucket_index)) != NULL) {
        xalloc_free(value);
        bucket_index++;
    }

    hashtable_spsc_free(command_info->tokens_hashtable);
    command_info->tokens_hashtable = NULL;
}

void module_redis_commands_free_commands_arguments_token_entries_hashtable(
        module_redis_command_info_t *command_infos,
        uint32_t command_infos_count) {
    for(
            uint32_t command_info_index = 0;
            command_info_index < command_infos_count;
            command_info_index++) {
        module_redis_command_info_t *command_info = &command_infos[command_info_index];

        if (!command_info->tokens_hashtable) {
            continue;
        }

        module_redis_commands_free_command_arguments_token_entries_hashtable(command_info);
    }
}

bool module_redis_commands_build_commands_arguments_token_entries_hashtable(
        module_redis_command_info_t *command_infos,
        uint32_t command_infos_count) {
    bool return_res = false;

    for(uint32_t command_info_index = 0; command_info_index < command_infos_count; command_info_index++) {
        hashtable_spsc_t *tokens_hashtable = NULL;
        module_redis_command_info_t *command_info = &command_infos[command_info_index];

        // Count the tokens
        uint16_t token_count = module_redis_commands_count_tokens_in_command_arguments(
                command_info->arguments,
                command_info->arguments_count);

        if (token_count == 0) {
            continue;
        }

        if ((tokens_hashtable = hashtable_spsc_new(
                token_count,
                8,
                false)) == NULL) {
            goto end;
        }

        if (!module_redis_commands_build_command_arguments_token_entries_hashtable(
                command_info->arguments,
                command_info->arguments_count,
                tokens_hashtable)) {
            goto end;
        }

        command_info->tokens_hashtable = tokens_hashtable;
    }

    return_res = true;

end:

    return return_res;
}
