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

#define TAG "module_redis_command"

bool module_redis_command_is_key_too_long(
        network_channel_t *channel,
        size_t key_length) {
    if (unlikely(key_length > channel->module_config->redis->max_key_length)) {
        return true;
    }

    return false;
}

void build_token_argument_map(
        module_redis_command_argument_t *arguments,
        uint16_t arguments_count,
        module_redis_command_parser_context_token_map_entry_t token_map[],
        uint16_t *token_count) {
    for(uint16_t index = 0; index < arguments_count; index++) {
        module_redis_command_argument_t *argument = &arguments[index];
        if (argument->token != NULL) {
            if (token_map != NULL) {
                module_redis_command_parser_context_token_map_entry_t *token_map_entry = &token_map[*token_count];
                token_map_entry->token = argument->token;
                token_map_entry->argument = argument;
                if (argument->parent_argument &&
                    argument->parent_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_ONEOF) {
                    for(
                            uint16_t parent_oneof_token_index = 0;
                            parent_oneof_token_index < argument->parent_argument->sub_arguments_count;
                            parent_oneof_token_index++) {
                        char *one_of_token = argument->parent_argument->sub_arguments[parent_oneof_token_index].token;
                        if (one_of_token && one_of_token != argument->token) {
                            assert(token_map_entry->one_of_token_count <=
                                sizeof(token_map_entry->one_of_tokens) / sizeof(char*));
                            token_map_entry->one_of_tokens[token_map_entry->one_of_token_count] = one_of_token;
                            token_map_entry->one_of_token_count++;
                        }
                    }
                }

            }

            (*token_count)++;
        }

        if (argument->has_sub_arguments) {
            build_token_argument_map(
                    argument->sub_arguments,
                    argument->sub_arguments_count,
                    token_map,
                    token_count);
        }
    }
}

bool module_redis_command_process_begin(
        module_redis_connection_context_t *connection_context) {
    if (connection_context->command.info->context_size > 0) {
        if ((connection_context->command.context = slab_allocator_mem_alloc_zero(
                connection_context->command.info->context_size)) == NULL) {
            LOG_D(TAG, "Unable to allocate the command context, terminating connection");
            return false;
        }
    }

    // The parser for the arguments works in pretty straightforward way:
    // - it expects all the positional non-optional arguments
    // - it then expects the positional optional arguments
    // - stop searching for the positional optional arguments once it finds the first token
    //

    // Count the amount of positional arguments
    uint16_t positional_arguments_total_count = 0, positional_arguments_required_count = 0;
    for(uint16_t index = 0; index < connection_context->command.info->arguments_count; index++) {
        module_redis_command_argument_t *argument = &connection_context->command.info->arguments[index];

        // First token found, stop the search
        if (argument->token) {
            break;
        }

        positional_arguments_total_count++;
        positional_arguments_required_count++;
    }

    // Builds the token map
    uint16_t token_count = 0;
    module_redis_command_parser_context_token_map_entry_t *token_map = NULL;

    build_token_argument_map(
            connection_context->command.info->arguments,
            connection_context->command.info->arguments_count,
            NULL,
            &token_count);

    token_map = slab_allocator_mem_alloc_zero(
            sizeof(module_redis_command_parser_context_token_map_entry_t) * token_count);
    if (token_map == NULL) {
        return false;
    }

    token_count = 0;
    build_token_argument_map(
            connection_context->command.info->arguments,
            connection_context->command.info->arguments_count,
            token_map,
            &token_count);

    connection_context->command.parser_context.token_count = token_count;
    connection_context->command.parser_context.token_map = token_map;
    connection_context->command.parser_context.positional_arguments_total_count = positional_arguments_total_count;
    connection_context->command.parser_context.positional_arguments_required_count = positional_arguments_required_count;

    return true;
}

bool module_redis_command_process_argument_stream_begin(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index,
        uint32_t arguments_count) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;
    module_redis_command_argument_t *arguments = connection_context->command.info->arguments;



    return true;
}

bool module_redis_command_process_argument_require_stream(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;

    return false;
}

bool module_redis_command_process_argument_stream_data(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index,
        char *chunk_data,
        size_t chunk_length) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;

    return true;
}

bool module_redis_command_process_argument_stream_end(
        module_redis_connection_context_t *connection_context) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;

    return true;
}

bool module_redis_command_process_argument_full(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_index,
        char *chunk_data,
        size_t chunk_length) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;

    return true;
}

bool module_redis_command_process_end(
        module_redis_connection_context_t *connection_context) {
    return connection_context->command.info->command_end_funcptr(
            connection_context);
}

#if DEBUG == 1
void module_redis_command_dump_argument(
        storage_db_t *db,
        uint32_t argument_index,
        module_redis_command_argument_t *argument,
        uintptr_t argument_context_base_addr,
        int depth) {
    int fake_count_1 = 1;
    bool argument_is_list = false;
    bool has_token = false;
    char depth_prefix[128] = { 0 };
    int *count;
    uintptr_t list;
    size_t list_item_size;

    char *argument_type_map[] = {
            "UNSUPPORTED",
            "KEY",
            "STRING",
            "INTEGER",
            "DOUBLE",
            "UNIXTIME",
            "BOOL",
            "PATTERN",
            "BLOCK",
            "ONEOF",
    };

    if (depth > 0) {
        sprintf(depth_prefix, "%.*s", depth*4, "                                                                ");
    }

    fprintf(
            stdout,
            "%s[%d] %s <%s%s>: ",
            depth_prefix,
            argument_index,
            argument->name,
            argument_type_map[argument->type],
            argument->has_multiple_occurrences ? "[]" : "");

    if (argument->has_multiple_occurrences) {
        argument_is_list = true;
    }

    list = argument_context_base_addr + argument->argument_context_member_offset;

    if (argument->token != NULL) {
        has_token = *(bool*)list;

        // This is VERY flaky, although only 1 byte is occupied for the bool, for performance reasons the compiler pad
        // the structure with 7 additional bytes to have the pointer or the struct afterwards 8-byte aligned.
        // This code most likely will not work on different arch and will cause a crash but it's not a massive problem
        // as this function, together with its caller, are used only for debugging so it's acceptable.
        list += sizeof(void*);
    }

    if (argument_is_list) {
        count = (int *)(list + sizeof(void *));

        fprintf(stdout, "(%d items)\n", *count);

        if (*count == 0) {
            return;
        }

        // Get the pointer to the list
        void **list_ptr = (void*)list;

        // Because count > 0, the pointer to the list can't be null
        assert(*list_ptr != NULL);

        // Cast back the list_ptr to uniptr_t to assign it back to list
        list = (uintptr_t)*list_ptr;
    } else {
        count = &fake_count_1;
    }

    list_item_size = argument->argument_context_member_size;

    // If it's not a list, list[0] will be the base address, the loop will not move forward as the count is artificially
    // set to 1 via the fake_count_1 variable
    for (int index = 0; index < *count; index++) {
        storage_db_chunk_sequence_t *chunk_sequence;
        uintptr_t base_addr = (uintptr_t)(list + (list_item_size * index));

        if (argument_is_list) {
            fprintf(stdout,"%s    %d: ", depth_prefix, index);
        }

        switch (argument->type) {
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY:
                fprintf(
                        stdout, "%s (%lu)\n",
                        *(char**)(base_addr + offsetof(module_redis_key_t, key)),
                        *(size_t*)(base_addr + offsetof(module_redis_key_t, length)));
                break;
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_STRING:
                chunk_sequence = *(storage_db_chunk_sequence_t**)(base_addr);

                if (chunk_sequence == NULL) {
                    fprintf(stdout, "NOT SET\n");
                } else {
                    bool is_short_value = chunk_sequence->size <= 64;

                    if (is_short_value) {
                        char buffer[64] = {0};
                        storage_db_chunk_read(
                                db,
                                storage_db_chunk_sequence_get(chunk_sequence, 0),
                                buffer);

                        fprintf(stdout, "%*s\n", (int) chunk_sequence->size, buffer);
                    } else {
                        fprintf(stdout, "<TOO LONG - %lu bytes>\n", chunk_sequence->size);
                    }
                }
                break;
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_INTEGER:
                fprintf(stdout, "%ld\n", *(int64_t*)(base_addr));
                break;
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_DOUBLE:
                fprintf(stdout, "%lf\n", *(double*)(base_addr));
                break;
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_UNIXTIME:
                fprintf(stdout, "%ld\n", *(int64_t*)(base_addr));
                break;
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BOOL:
                fprintf(stdout, "%s\n", *(bool*)(base_addr) ? "true" : "false");
                break;
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN:
                fprintf(
                        stdout, "%s (%lu)\n",
                        *(char**)(base_addr + offsetof(module_redis_pattern_t, pattern)),
                        *(size_t*)(base_addr + offsetof(module_redis_pattern_t, length)));
                break;
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BLOCK:
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_ONEOF:
                fprintf(stdout, "\n");
                module_redis_command_dump_arguments(
                        db,
                        argument->sub_arguments,
                        argument->sub_arguments_count,
                        base_addr,
                        depth + 1);
                break;
            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_UNSUPPORTED:
                fprintf(stdout, "UNSUPPORTED!\n");
                break;
        }
    }

    if (argument->token != NULL) {
        fprintf(
                stdout,
                "%s    token <%s> is %s\n",
                depth_prefix,
                argument->token,
                has_token ? "true" : "false");
    }
}

void module_redis_command_dump_arguments(
        storage_db_t *db,
        module_redis_command_argument_t arguments[],
        int arguments_count,
        uintptr_t argument_context_base_addr,
        int depth) {

    for(int argument_index = 0; argument_index < arguments_count; argument_index++) {
        module_redis_command_dump_argument(
                db,
                argument_index,
                &arguments[argument_index],
                argument_context_base_addr,
                depth);
    }
}
#endif
