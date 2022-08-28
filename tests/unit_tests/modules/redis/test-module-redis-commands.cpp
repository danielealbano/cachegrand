/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch.hpp>
#include <cstring>
#include <arpa/inet.h>

#include "../../enum-flags-operators.hpp"

#include "misc.h"
#include "exttypes.h"
#include "config.h"
#include "clock.h"
#include "spinlock.h"
#include "xalloc.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/small_circular_queue/small_circular_queue.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "module/redis/module_redis.h"

#include "module/redis/module_redis_commands.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

extern module_redis_command_argument_t module_redis_command_sort_arguments[];
extern module_redis_command_argument_t module_redis_command_get_arguments[];

TEST_CASE("module/redis/module_redis_commands.c", "[module][redis][module_redis_commands]") {
    module_redis_command_info_t command_infos_map[] = {
            {
                    .string = { 'S', 'O', 'R', 'T', 0 },
                    .string_len = (uint8_t)strlen("SORT"),
                    .command = MODULE_REDIS_COMMAND_SORT,
                    .context_size = sizeof(module_redis_command_sort_context_t),
                    .arguments_count = 7,
                    .required_arguments_count = 3,
                    .arguments = module_redis_command_sort_arguments,
                    .tokens_hashtable = nullptr,
            },
            {
                    .string = { 'G', 'E', 'T', 0 },
                    .string_len = (uint8_t)strlen("GET"),
                    .command = MODULE_REDIS_COMMAND_GET,
                    .context_size = sizeof(module_redis_command_get_context_t),
                    .arguments_count = 1,
                    .required_arguments_count = 1,
                    .arguments = module_redis_command_get_arguments,
                    .tokens_hashtable = nullptr,
            },
    };

    SECTION("Ensure that auto generated struct and data haven't changed") {
        SECTION("sizeof(module_redis_command_sort_context_t)") {
            REQUIRE(sizeof(module_redis_command_sort_context_t) == 120);
        }

        SECTION("sizeof(module_redis_command_sort_context_subargument_order_t)") {
            REQUIRE(sizeof(module_redis_command_sort_context_subargument_order_t) == 2);
        }

        SECTION("sizeof(module_redis_command_sort_context_subargument_limit_offset_count_t)") {
            REQUIRE(sizeof(module_redis_command_sort_context_subargument_limit_offset_count_t) == 16);
        }

        SECTION("command_info ") {
            REQUIRE(sizeof(module_redis_command_sort_context_subargument_limit_offset_count_t) == 16);
        }
    }

    SECTION("module_redis_commands_build_commands_hashtables") {
        hashtable_spsc_t *hashtable = module_redis_commands_build_commands_hashtables(
                command_infos_map, ARRAY_SIZE(command_infos_map));

        REQUIRE(hashtable != nullptr);

        REQUIRE(hashtable_spsc_op_get_ci(
                hashtable,
                command_infos_map[0].string,
                command_infos_map[0].string_len) == &command_infos_map[0]);

        REQUIRE(hashtable_spsc_op_get_ci(
                hashtable,
                command_infos_map[1].string,
                command_infos_map[1].string_len) == &command_infos_map[1]);

        hashtable_spsc_free(hashtable);
    }

    SECTION("module_redis_commands_count_tokens_in_command_arguments") {
        SECTION("command with tokens") {
            uint16_t tokens_count = module_redis_commands_count_tokens_in_command_arguments(
                    command_infos_map[0].arguments,
                    command_infos_map[0].arguments_count);

            REQUIRE(tokens_count == 7);
        }

        SECTION("command without tokens") {
            uint16_t tokens_count = module_redis_commands_count_tokens_in_command_arguments(
                    command_infos_map[1].arguments,
                    command_infos_map[1].arguments_count);

            REQUIRE(tokens_count == 0);
        }
    }

    SECTION("module_redis_commands_build_command_argument_token_entry_oneof_tokens") {
        module_redis_command_parser_context_argument_token_entry_t token_entry = { nullptr };

        SECTION("argument with oneof tokens") {
            module_redis_commands_build_command_argument_token_entry_oneof_tokens(
                    &command_infos_map[0].arguments[4].sub_arguments[0],
                    &token_entry);

            REQUIRE(strcmp(token_entry.one_of_tokens[0], "DESC") == 0);
            REQUIRE(token_entry.one_of_token_count == 1);
        }
    }

    SECTION("module_redis_commands_build_command_argument_token_entry") {
        module_redis_command_parser_context_argument_token_entry_t *token_entry;

        SECTION("argument with token") {
            token_entry = module_redis_commands_build_command_argument_token_entry(
                    &command_infos_map[0].arguments[5]);

            REQUIRE(token_entry != NULL);
            REQUIRE(token_entry->token == command_infos_map[0].arguments[5].token);
            REQUIRE(token_entry->token_length == strlen(command_infos_map[0].arguments[5].token));
            REQUIRE(token_entry->argument == &command_infos_map[0].arguments[5]);
            REQUIRE(token_entry->one_of_tokens[0] == nullptr);
            REQUIRE(token_entry->one_of_token_count == 0);
        }

        SECTION("subargument with oneof tokens") {
            token_entry = module_redis_commands_build_command_argument_token_entry(
                    &command_infos_map[0].arguments[4].sub_arguments[0]);

            REQUIRE(token_entry != NULL);
            REQUIRE(token_entry->token == command_infos_map[0].arguments[4].sub_arguments[0].token);
            REQUIRE(token_entry->token_length == strlen(command_infos_map[0].arguments[4].sub_arguments[0].token));
            REQUIRE(token_entry->argument == &command_infos_map[0].arguments[4].sub_arguments[0]);
            REQUIRE(strcmp(token_entry->one_of_tokens[0], "DESC") == 0);
            REQUIRE(token_entry->one_of_token_count == 1);
        }

        free(token_entry);
    }

    SECTION("module_redis_commands_build_command_arguments_token_entries_hashtable") {
        hashtable_spsc_bucket_index_t bucket_index;
        hashtable_spsc_t *hashtable = hashtable_spsc_new(
                32,
                HASHTABLE_SPSC_DEFAULT_MAX_RANGE,
                true,
                false);

        SECTION("command with tokens") {
            module_redis_command_parser_context_argument_token_entry_t *token_entry;

            REQUIRE(module_redis_commands_build_command_arguments_token_entries_hashtable(
                    command_infos_map[0].arguments,
                    command_infos_map[0].arguments_count,
                    hashtable) == true);

            token_entry = (module_redis_command_parser_context_argument_token_entry_t*) hashtable_spsc_op_get_ci(
                    hashtable,
                    "LIMIT",
                    strlen("LIMIT"));

            REQUIRE(token_entry != nullptr);
            REQUIRE(strcmp(token_entry->token, "LIMIT") == 0);
            REQUIRE(token_entry->token_length == strlen("LIMIT"));
            REQUIRE(token_entry->argument == &command_infos_map[0].arguments[2]);
            REQUIRE(token_entry->one_of_tokens[0] == nullptr);
            REQUIRE(token_entry->one_of_token_count == 0);

            token_entry = (module_redis_command_parser_context_argument_token_entry_t*) hashtable_spsc_op_get_ci(
                    hashtable,
                    "ASC",
                    strlen("ASC"));

            REQUIRE(token_entry != nullptr);
            REQUIRE(strcmp(token_entry->token, "ASC") == 0);
            REQUIRE(token_entry->token_length == strlen("ASC"));
            REQUIRE(token_entry->argument == &command_infos_map[0].arguments[4].sub_arguments[0]);
            REQUIRE(strcmp(token_entry->one_of_tokens[0], "DESC") == 0);
            REQUIRE(token_entry->one_of_token_count == 1);
        }

        SECTION("command without tokens") {
            REQUIRE(module_redis_commands_build_command_arguments_token_entries_hashtable(
                    command_infos_map[1].arguments,
                    command_infos_map[1].arguments_count,
                    hashtable) == true);

            bucket_index = 0;
            REQUIRE(hashtable_spsc_op_iter(hashtable, &bucket_index) == nullptr);
            REQUIRE(bucket_index == -1);
        }

        void *value;
        bucket_index = 0;
        while((value = hashtable_spsc_op_iter(hashtable, &bucket_index)) != nullptr) {
            xalloc_free(value);
            bucket_index++;
        }

        hashtable_spsc_free(hashtable);
    }

    SECTION("module_redis_commands_build_commands_arguments_token_entries_hashtable") {
        hashtable_spsc_bucket_index_t bucket_index;
        module_redis_command_parser_context_argument_token_entry_t *token_entry;
        module_redis_commands_build_commands_arguments_token_entries_hashtable(
                command_infos_map,
                ARRAY_SIZE(command_infos_map));

        REQUIRE(command_infos_map[0].tokens_hashtable != nullptr);
        REQUIRE(command_infos_map[1].tokens_hashtable == nullptr);

        token_entry = (module_redis_command_parser_context_argument_token_entry_t*) hashtable_spsc_op_get_ci(
                command_infos_map[0].tokens_hashtable,
                "LIMIT",
                strlen("LIMIT"));

        REQUIRE(token_entry != nullptr);
        REQUIRE(strcmp(token_entry->token, "LIMIT") == 0);
        REQUIRE(token_entry->token_length == strlen("LIMIT"));
        REQUIRE(token_entry->argument == &command_infos_map[0].arguments[2]);
        REQUIRE(token_entry->one_of_tokens[0] == nullptr);
        REQUIRE(token_entry->one_of_token_count == 0);

        void *value;
        bucket_index = 0;
        while((value = hashtable_spsc_op_iter(
                command_infos_map[0].tokens_hashtable,
                &bucket_index)) != nullptr) {
            xalloc_free(value);
            bucket_index++;
        }

        hashtable_spsc_free(command_infos_map[0].tokens_hashtable);
        command_infos_map[0].tokens_hashtable = nullptr;
    }

    SECTION("module_redis_commands_free_command_arguments_token_entries_hashtable") {
        command_infos_map[0].tokens_hashtable = hashtable_spsc_new(
                8,
                8,
                true,
                false);

        module_redis_commands_free_command_arguments_token_entries_hashtable(&command_infos_map[0]);

        REQUIRE(command_infos_map[0].tokens_hashtable == nullptr);
    }

    SECTION("module_redis_commands_free_commands_arguments_token_entries_hashtable") {
        module_redis_commands_build_commands_arguments_token_entries_hashtable(
                command_infos_map,
                ARRAY_SIZE(command_infos_map));

        REQUIRE(command_infos_map[0].tokens_hashtable != nullptr);
        REQUIRE(command_infos_map[1].tokens_hashtable == nullptr);

        module_redis_commands_free_commands_arguments_token_entries_hashtable(
                command_infos_map,
                ARRAY_SIZE(command_infos_map));

        REQUIRE(command_infos_map[0].tokens_hashtable == nullptr);
        REQUIRE(command_infos_map[1].tokens_hashtable == nullptr);
    }
}
