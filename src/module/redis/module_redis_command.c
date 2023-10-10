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
#include <errno.h>
#include <error.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <assert.h>
#include <math.h>

#include "misc.h"
#include "exttypes.h"
#include "xalloc.h"
#include "log/log.h"
#include "spinlock.h"
#include "transaction.h"
#include "transaction_rwspinlock.h"
#include "clock.h"
#include "config.h"
#include "data_structures/ring_bounded_queue_spsc/ring_bounded_queue_spsc_voidptr.h"
#include "data_structures/double_linked_list/double_linked_list.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "data_structures/hashtable/spsc/hashtable_spsc.h"
#include "data_structures/queue_mpmc/queue_mpmc.h"
#include "data_structures/slots_bitmap_mpmc/slots_bitmap_mpmc.h"
#include "memory_allocator/ffma.h"
#include "protocol/redis/protocol_redis.h"
#include "protocol/redis/protocol_redis_reader.h"
#include "protocol/redis/protocol_redis_writer.h"
#include "module/module.h"
#include "network/io/network_io_common.h"
#include "network/channel/network_channel.h"
#include "network/network.h"
#include "storage/io/storage_io_common.h"
#include "storage/channel/storage_channel.h"
#include "storage/db/storage_db.h"
#include "module/redis/module_redis.h"
#include "module_redis_connection.h"

#include "module_redis_command.h"

#define TAG "module_redis_command"

bool module_redis_command_process_begin(
        module_redis_connection_context_t *connection_context) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;

    if (connection_context->command.info->context_size > 0) {
        if ((connection_context->command.context = ffma_mem_alloc_zero(
                connection_context->command.info->context_size)) == NULL) {
            LOG_D(TAG, "Unable to allocate the command context, terminating connection");
            return false;
        }
    }

    // Figures out if there is a positional argument that has to be handled
    if (connection_context->command.info->arguments_count > 0 &&
            connection_context->command.info->arguments[0].is_positional) {
        module_redis_command_argument_t *expected_argument = &connection_context->command.info->arguments[0];
        command_parser_context->current_argument.expected_argument = expected_argument;
        command_parser_context->current_argument.block_argument_index = 0;
    } else {
        command_parser_context->current_argument.expected_argument = NULL;
    }

    return true;
}

void *module_redis_command_get_base_context_from_argument(
        module_redis_command_argument_t *argument,
        module_redis_command_argument_t *stop_at_parent_argument,
        module_redis_command_context_t *command_context,
        bool *stopped_at_list,
        uint16_t *stopped_at_list_count,
        module_redis_command_argument_t **stopped_at_list_argument,
        module_redis_command_argument_t **stopped_at_list_resume_from_argument) {
    uintptr_t base_addr = (uintptr_t)command_context;
    int arguments_queue_count = 0, arguments_queue_index;
    module_redis_command_argument_t *arguments_queue[4];

    // Uses a for to let gcc unroll the loop easily and have only 1 branch based on the parent_argument check but a
    // queue with just 4 slots might not be enough for all the cases and commands so there is an assert right after
    // to ensure the loop ended because it found the expected root or not
    for(
            ;
            // should argument != null if stop_at_parent_argument == null and argument->parent != stop_at_parent_argument if not
            argument != stop_at_parent_argument && arguments_queue_count < ARRAY_SIZE(arguments_queue);
            argument = argument->parent_argument) {
        arguments_queue[arguments_queue_count] = argument;
        arguments_queue_count++;
    }

    assert(argument == stop_at_parent_argument);

    for(
            arguments_queue_index = arguments_queue_count - 1;
            arguments_queue_index >= 0;
            arguments_queue_index--) {
        argument = arguments_queue[arguments_queue_index];

        base_addr = base_addr + argument->argument_context_member_offset;

        // If argument is a token, the first element in the context is the boolean value
        if (argument->token != NULL) {
            base_addr += module_redis_command_get_context_has_token_padding_size();
        }

        // If it's a list we need to stop and return the pointer to the list because the right index has to be
        // identified before the member of the context within the list can be fetched.
        if (argument->has_multiple_occurrences) {
            *stopped_at_list = true;
            *stopped_at_list_count = *(int *)(base_addr + sizeof(void *));
            *stopped_at_list_argument = argument;
            *stopped_at_list_resume_from_argument = arguments_queue[arguments_queue_index];
        }

        if (*stopped_at_list) {
            break;
        }
    }

    if (argument->token) {
        base_addr -= module_redis_command_get_context_has_token_padding_size();
    }

    return (void*)base_addr;
}

void *module_redis_command_context_list_expand_and_get_new_entry(
        module_redis_command_argument_t *argument,
        void *base_addr) {
    void *list = NULL, *list_new = NULL, *new_list_entry;

    int list_count = module_redis_command_context_list_get_count(argument, base_addr);
    int list_count_new = list_count + 1;

    // If the list_count is zero it's not necessary to get the current pointer
    if (list_count == 0) {
        list = ffma_mem_alloc_zero(list_count_new * argument->argument_context_member_size);
    } else {
        list = module_redis_command_context_list_get_list(argument, base_addr);
        size_t current_size = list_count * argument->argument_context_member_size;
        size_t new_size = list_count_new * argument->argument_context_member_size;

        list_new = ffma_mem_realloc(
                list,
                new_size);

        // Zero the new memory
        memset(list_new + current_size, 0, new_size - current_size);

        list = list_new;

        if (!list) {
            return NULL;
        }
    }

    module_redis_command_context_list_set_list(argument, base_addr, list);
    module_redis_command_context_list_set_count(argument, base_addr, list_count_new);

    // Calculate the offset of the new entry and return it
    new_list_entry = list + ((list_count_new - 1) * argument->argument_context_member_size);
    return new_list_entry;
}

void *module_redis_command_context_get_argument_member_context_addr(
        module_redis_command_argument_t *argument,
        bool is_in_block,
        int block_argument_index,
        void *context) {
    bool stopped_at_list = false;
    uint16_t stopped_at_list_count = 0;
    void *argument_member_context_addr;
    module_redis_command_argument_t *stopped_at_list_argument, *stopped_at_list_resume_from_argument;

    argument_member_context_addr = module_redis_command_get_base_context_from_argument(
            argument,
            NULL,
            context,
            &stopped_at_list,
            &stopped_at_list_count,
            &stopped_at_list_argument,
            &stopped_at_list_resume_from_argument);
    if (stopped_at_list) {
        // If the argument processing stopped at a list it means that:
        // - if the parser is not parsing a block it can always resize
        // - if the parser is parsing a block it can resize only if it's the first argument of the block (the index is
        //   set back to zero when the argument block is fully parsed)
        // - if the parser is parsing a block and it's not the first argument, it must only retrieve the last entry of
        //   the list without expanding it
        if (is_in_block == false || block_argument_index == 0) {
            argument_member_context_addr = module_redis_command_context_list_expand_and_get_new_entry(
                    stopped_at_list_argument,
                    argument_member_context_addr);
        } else if (block_argument_index > 0) {
            argument_member_context_addr = module_redis_command_context_list_get_entry(
                    stopped_at_list_argument,
                    argument_member_context_addr,
                    stopped_at_list_count - 1);
        }

        // If the resume from argument is not null it means that the list just found is a block and therefore it's
        // necessary to keep digging
        if (stopped_at_list_resume_from_argument != NULL) {
            // Search again but this time as parent use the element right before the one on which we stopped
            module_redis_command_argument_t *stop_at_parent_argument = stopped_at_list_resume_from_argument;
            stopped_at_list = false;
            stopped_at_list_resume_from_argument = NULL;
            argument_member_context_addr = module_redis_command_get_base_context_from_argument(
                    argument,
                    stop_at_parent_argument,
                    argument_member_context_addr,
                    &stopped_at_list,
                    &stopped_at_list_count,
                    &stopped_at_list_argument,
                    &stopped_at_list_resume_from_argument);

            // Nested lists aren't allowed!
            assert(!stopped_at_list);
        }
    }

    return argument_member_context_addr;
}

bool module_redis_command_is_key_pattern_allowed_length(
        module_redis_connection_context_t *connection_context,
        module_redis_command_argument_t *argument,
        size_t argument_length) {
    // Checks if the key / pattern length is allowed before doing anything else
    if (
            argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY ||
            argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN) {
        if (module_redis_command_is_key_too_long(
                connection_context->network_channel,
                argument_length)) {
            return false;
        }
    }

    return true;
}

bool module_redis_command_process_argument_begin(
        module_redis_connection_context_t *connection_context,
        uint32_t argument_length) {
    bool is_in_block = false;
    module_redis_command_argument_t *expected_argument;
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;

    expected_argument = command_parser_context->current_argument.expected_argument;

    // If there isn't an expected argument to handle there is nothing that can be done in advance, the only arguments
    // which will be allowed are the ones with tokens, they can't be streamed
    if (expected_argument == NULL) {
        command_parser_context->current_argument.require_stream = false;
        return true;
    }

#if DEBUG == 1
    // Ensure that required arguments don't have tokens as they are positional
    if (expected_argument->is_optional == false) {
        assert(expected_argument->token == NULL);
    }
#endif

    if (expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BLOCK) {
        is_in_block = true;
        expected_argument = &expected_argument->sub_arguments[
                command_parser_context->current_argument.block_argument_index];
#if DEBUG == 1
        // Ensure that required arguments don't have tokens as they are positional
        if (expected_argument->is_optional == false) {
            assert(expected_argument->token == NULL);
        }
#endif
    }

    if (!module_redis_command_is_key_pattern_allowed_length(
            connection_context,
            expected_argument,
            argument_length)) {
        return module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR The %s length has exceeded the allowed size of '%u'",
                expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY ? "key" : "pattern",
                connection_context->network_channel->module_config->redis->max_key_length);
    }

    if (expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_LONG_STRING) {
        if (!storage_db_chunk_sequence_is_size_allowed(argument_length)) {
            return module_redis_connection_error_message_printf_noncritical(
                    connection_context,
                    "ERR The argument length has exceeded the allowed size of '%lu'",
                    storage_db_chunk_sequence_allowed_max_size());
        }

        command_parser_context->current_argument.member_context_addr =
                module_redis_command_context_get_argument_member_context_addr(
                        expected_argument,
                        is_in_block,
                        command_parser_context->current_argument.block_argument_index,
                        connection_context->command.context);

        module_redis_long_string_t *string = command_parser_context->current_argument.member_context_addr;

        if (unlikely(!storage_db_chunk_sequence_allocate(
                connection_context->db,
                &string->chunk_sequence,
                argument_length))) {
            LOG_E(TAG, "Failed to allocate chunks for the incoming data");
            return false;
        }

        command_parser_context->current_argument.require_stream = true;
    } else {
        command_parser_context->current_argument.require_stream = false;
    }

    return true;
}

bool module_redis_command_process_argument_stream_data(
        module_redis_connection_context_t *connection_context,
        char *chunk_data,
        size_t chunk_length) {
    module_redis_long_string_t *string;
    storage_db_chunk_sequence_t *chunk_sequence;
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;
    module_redis_command_argument_t *expected_argument = command_parser_context->current_argument.expected_argument;
    void *argument_member_context_addr = command_parser_context->current_argument.member_context_addr;

    // Streaming is only allowed for positional arguments that are strings, tokens will never have to handle strings
    assert(expected_argument != NULL);

    if (expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BLOCK) {
        expected_argument = &expected_argument->sub_arguments[
                command_parser_context->current_argument.block_argument_index];
#if DEBUG == 1
        // Ensure that required arguments don't have tokens as they are positional
        if (expected_argument->is_optional == false) {
            assert(expected_argument->token == NULL);
        }
#endif
    }

    assert(expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_LONG_STRING);

    if (unlikely(chunk_length == 0)) {
        return true;
    }

    string = argument_member_context_addr;
    chunk_sequence = &string->chunk_sequence;

    size_t written_data = 0;
    do {
        // When all the data have been written current_chunk.index will always match chunk_sequence_count, this assert
        // catch the cases when this function is invoked to write data even if all the chunks have been written.
        assert(string->current_chunk.index < chunk_sequence->count);

        storage_db_chunk_info_t *chunk_info = storage_db_chunk_sequence_get(
                chunk_sequence,
                string->current_chunk.index);

        size_t chunk_length_to_write = chunk_length - written_data;
        size_t chunk_available_size = chunk_info->chunk_length - string->current_chunk.offset;
        size_t chunk_data_to_write_length =
                chunk_length_to_write > chunk_available_size ? chunk_available_size : chunk_length_to_write;

        // There should always be something to write
        assert(chunk_length_to_write > 0);
        assert(chunk_data_to_write_length > 0);

        bool res = storage_db_chunk_write(
                connection_context->db,
                chunk_info,
                string->current_chunk.offset,
                chunk_data + written_data,
                chunk_data_to_write_length);

        if (!res) {
            LOG_E(
                    TAG,
                    "Unable to write value chunk <%u> long <%u> bytes",
                    string->current_chunk.index,
                    chunk_info->chunk_length);
            return false;
        }

        written_data += chunk_data_to_write_length;
        string->current_chunk.offset += (off_t)chunk_data_to_write_length;

        if (string->current_chunk.offset == chunk_info->chunk_length) {
            string->current_chunk.index++;
            string->current_chunk.offset = 0;
        }
    } while(written_data < chunk_length);

    return true;
}

bool module_redis_command_process_argument_full(
        module_redis_connection_context_t *connection_context,
        char *chunk_data,
        size_t chunk_length) {
    char *string_value, *integer_value_end_ptr, *double_value_end_ptr;
    uint64_t *integer_value;
    long double *double_value;
    bool check_tokens = false, token_found = false, is_in_block = false;
    uint16_t block_argument_index = 0;
    module_redis_command_argument_t *guessed_argument = NULL;
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;
    module_redis_command_argument_t *expected_argument = command_parser_context->current_argument.expected_argument;

    // The guessing argument logic is pretty straight forward:
    // - if the expected positional argument is not NULL and it's marked as required, that's what we are looking for, no
    //   need to further guess
    // - if the expected positional argument is not NULL and it's marked as optional, it's necessary to check if the
    //   received argument is a token or not

    if (expected_argument != NULL) {
        // If the current expect argument is a block, it has to be processed till the end
        if (expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BLOCK) {
            is_in_block = true;
            block_argument_index = command_parser_context->current_argument.block_argument_index;
            guessed_argument = expected_argument = &expected_argument->sub_arguments[
                    command_parser_context->current_argument.block_argument_index];

            // TODO: the current implementation isn't capable of dealing properly with optional elements in blocks
        } else {
            // If the expected argument is required or it's an already known token there is nothing to guess
            if (expected_argument->is_optional == false || expected_argument->token) {
                guessed_argument = expected_argument;
            } else {
                check_tokens = true;
            }
        }
    } else {
        check_tokens = true;
    }

    if (check_tokens && connection_context->command.info->tokens_hashtable) {
        module_redis_command_parser_context_argument_token_entry_t *token_entry = hashtable_spsc_op_get_ci(
                connection_context->command.info->tokens_hashtable,
                chunk_data,
                chunk_length);

        if (token_entry) {
            if (connection_context->network_channel->module_config->redis->strict_parsing) {
                bool stopped_at_list = false, has_token = false;
                uint16_t stopped_at_list_count = 0;
                module_redis_command_argument_t *stopped_at_list_argument, *stopped_at_list_resume_from_argument;

                for(int index = 0; index < token_entry->one_of_token_count; index++) {
                    module_redis_command_parser_context_argument_token_entry_t *oneof_token_entry = hashtable_spsc_op_get_ci(
                            connection_context->command.info->tokens_hashtable,
                            token_entry->one_of_tokens[index],
                            strlen(token_entry->one_of_tokens[index]));

                    assert(oneof_token_entry);

                    void *base_addr = module_redis_command_get_base_context_from_argument(
                            oneof_token_entry->argument,
                            NULL,
                            connection_context->command.context,
                            &stopped_at_list,
                            &stopped_at_list_count,
                            &stopped_at_list_argument,
                            &stopped_at_list_resume_from_argument);

                    has_token = module_redis_command_context_has_token_get(
                            oneof_token_entry->argument,
                            base_addr);

                    if (has_token) {
                        return module_redis_connection_error_message_printf_noncritical(
                                connection_context,
                                "ERR the command '%s' doesn't support both the parameters '%s' and '%s' set at the same time",
                                connection_context->command.info->string,
                                oneof_token_entry->token,
                                token_entry->token);
                    }
                }

                void *base_addr = module_redis_command_get_base_context_from_argument(
                        token_entry->argument,
                        NULL,
                        connection_context->command.context,
                        &stopped_at_list,
                        &stopped_at_list_count,
                        &stopped_at_list_argument,
                        &stopped_at_list_resume_from_argument);

                has_token = module_redis_command_context_has_token_get(
                        token_entry->argument,
                        base_addr);

                if (has_token && !token_entry->argument->has_multiple_token) {
                    return module_redis_connection_error_message_printf_noncritical(
                            connection_context,
                            "ERR the parameter '%s' has already been specified for the command '%s'",
                            token_entry->token,
                            connection_context->command.info->string);
                }
            }

            token_found = true;
            guessed_argument = token_entry->argument;
            command_parser_context->current_argument.expected_argument = NULL;

            // If the token is found, the next expected argument has to be the argument found to properly set the value
            // unless it's a boolean argument, in that case has_token is set directly in this iteration as there isn't
            // another argument
            if (guessed_argument->type != MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BOOL) {
                command_parser_context->current_argument.next_expected_argument = guessed_argument;
            }
        }
    }

    // If a token can't be found then we can treat the current argument as a positional one and rely on the
    // expected_argument argument identified in advance. If that is not available it means that probably an unknown
    // token has been received or a positional argument has been received after a token, which isn't a supported
    // scenario.
    if (!token_found) {
        if (expected_argument) {
            guessed_argument = expected_argument;
        }
    }

    // If guessed argument is null it means it wasn't possible to guess one, report the error and move on
    if (guessed_argument == NULL) {
        return module_redis_connection_error_message_printf_noncritical(
                connection_context,
                "ERR wrong number of arguments for '%s' command",
                connection_context->command.info->string);
    }

    // If the tokens have been checked and the guessed argument has a token, it can be set straight to true and the
    // operation can be stopped here, if there is an argument associated with the token, it will be processed in the
    // next iteration as next_expected_argument has been set to guessed_argument.
    if (check_tokens && guessed_argument->token) {
        // To properly handle lists, the argument has to be set true bypassing the lists automated expansion so the
        // code here invokes module_redis_command_get_base_context_from_argument directly.
        bool stopped_at_list = false;
        uint16_t stopped_at_list_count = 0;
        module_redis_command_argument_t *stopped_at_list_argument, *stopped_at_list_resume_from_argument;

        void *base_addr = module_redis_command_get_base_context_from_argument(
                guessed_argument,
                NULL,
                connection_context->command.context,
                &stopped_at_list,
                &stopped_at_list_count,
                &stopped_at_list_argument,
                &stopped_at_list_resume_from_argument);

        module_redis_command_context_has_token_set(
                guessed_argument,
                base_addr,
                true);

        return true;
    }

    command_parser_context->current_argument.member_context_addr =
            module_redis_command_context_get_argument_member_context_addr(
                    guessed_argument,
                    is_in_block,
                    block_argument_index,
                    connection_context->command.context);

    void *base_addr = module_redis_command_context_base_addr_skip_has_token(
            guessed_argument,
            command_parser_context->current_argument.member_context_addr);

    switch (guessed_argument->type) {
        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY:
        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN:
        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_SHORT_STRING:
            if (!module_redis_command_is_key_pattern_allowed_length(
                    connection_context,
                    guessed_argument,
                    chunk_length)) {
                return module_redis_connection_error_message_printf_noncritical(
                        connection_context,
                        "ERR The %s length has exceeded the allowed size of '%u'",
                        expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY
                        ? "key"
                        : (expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN
                            ? "pattern"
                            : "short string"),
                        connection_context->network_channel->module_config->redis->max_key_length);
            }

            if (unlikely(chunk_length == 0)) {
                return module_redis_connection_error_message_printf_noncritical(
                        connection_context,
                        "ERR the %s '%s' has length '0', not allowed",
                        expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY
                        ? "key"
                        : (expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN
                           ? "pattern"
                           : "short string"),
                       guessed_argument->name);
            }

            string_value = xalloc_alloc(chunk_length);

            if (!string_value) {
                LOG_E(TAG, "Failed to allocate memory for the incoming data");
                return false;
            }

            memcpy(string_value, chunk_data, chunk_length);

            if (guessed_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_KEY) {
                module_redis_key_t *key = base_addr;
                key->key = string_value;
                key->length = chunk_length;
            } else if (guessed_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN) {
                module_redis_pattern_t *pattern = base_addr;
                pattern->pattern = string_value;
                pattern->length = chunk_length;
            } else {
                module_redis_short_string_t *short_string = base_addr;
                short_string->short_string = string_value;
                short_string->length = chunk_length;
            }

            break;

        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_INTEGER:
        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_UNIXTIME:
            integer_value = base_addr;
            *integer_value = strtoll(chunk_data, &integer_value_end_ptr, 10);

            if (errno == ERANGE || integer_value_end_ptr != chunk_data + chunk_length) {
                return module_redis_connection_error_message_printf_noncritical(
                        connection_context,
                        "ERR value for argument '%s' is not an integer or out of range",
                        guessed_argument->name);
            }
            break;

        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_DOUBLE:
            double_value = base_addr;
            *double_value = strtold(chunk_data, &double_value_end_ptr);

            if (errno == ERANGE || double_value_end_ptr != chunk_data + chunk_length) {
                return module_redis_connection_error_message_printf_noncritical(
                        connection_context,
                        "ERR value for argument '%s' is not a double or out of range",
                        guessed_argument->name);
            }
            break;

        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BLOCK:
        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BOOL:
            // Nothing has to be done, the has_token flag is automatically set when a token is found
            break;

        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_LONG_STRING:
        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_ONEOF:
        case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_UNSUPPORTED:
            // This should never really happen
            assert(false);
    }

    return true;
}

bool module_redis_command_process_argument_end(
        module_redis_connection_context_t *connection_context) {
    module_redis_command_parser_context_t *command_parser_context = &connection_context->command.parser_context;
    module_redis_command_argument_t *expected_argument = command_parser_context->current_argument.expected_argument;
    module_redis_command_argument_t *next_expected_argument =
            command_parser_context->current_argument.next_expected_argument;

    if (next_expected_argument) {
        command_parser_context->current_argument.next_expected_argument = NULL;
        command_parser_context->current_argument.expected_argument = next_expected_argument;
        command_parser_context->current_argument.block_argument_index = 0;
        return true;
    }

    if (!expected_argument) {
        return true;
    }

    if (expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_BLOCK) {
        // TODO: It is necessary to handle cases where block arguments are optional, as a token in the block can be
        //       received or a token from another entirely block can be received, currently it's not handled properly
        command_parser_context->current_argument.block_argument_index++;

        if (command_parser_context->current_argument.block_argument_index < expected_argument->sub_arguments_count) {
            return true;
        }

        // If it gets here it means that all the arguments in the block have been processed so is_in_block can be
        // marked as false
        command_parser_context->current_argument.block_argument_index = 0;
    }

    // Check if the there is an expected positional argument set, if it does no tokens have been found yet and there
    // might be further positional arguments to be processed
    // Also, if the current argument (expected positional argument is automatically the current argument if it's not
    // null at this point) is positional and is multi, will never move to the next argument, unless it's a token in
    // which case the code does simply nothing
    if (!expected_argument->is_positional) {
        expected_argument = NULL;
    } else if(!expected_argument->has_multiple_occurrences) {
        expected_argument = NULL;

        // Check if there are more arguments
        if (command_parser_context->positional_arguments_parsed_count <
            connection_context->command.info->arguments_count) {
            command_parser_context->positional_arguments_parsed_count++;

            if (unlikely(command_parser_context->positional_arguments_parsed_count >=
                connection_context->command.info->arguments_count)) {
                expected_argument = NULL;
            } else {
                expected_argument = &connection_context->command.info->arguments[
                        command_parser_context->positional_arguments_parsed_count];

                // If the argument after the current is not positional (has a token) or it's of type ONEOF they have all
                // been processed and there are only tokens to process so set the expected argument to null
                if (!expected_argument->is_positional ||
                    expected_argument->type == MODULE_REDIS_COMMAND_ARGUMENT_TYPE_ONEOF) {
                    expected_argument = NULL;
                }
            }
        }
    }

    command_parser_context->current_argument.expected_argument = expected_argument;

    return true;
}

bool module_redis_command_acquire_slice_and_write_blob_start(
        network_channel_t *network_channel,
        size_t slice_length,
        size_t value_length,
        network_channel_buffer_data_t **send_buffer,
        network_channel_buffer_data_t **send_buffer_start,
        network_channel_buffer_data_t **send_buffer_end) {
    *send_buffer = *send_buffer_start = network_send_buffer_acquire_slice(
            network_channel,
            slice_length);
    if (*send_buffer_start == NULL) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        return false;
    }

    *send_buffer_end = *send_buffer_start + slice_length;

    *send_buffer_start = protocol_redis_writer_write_argument_blob_start(
            *send_buffer_start,
            slice_length,
            false,
            (int)value_length);

    if (*send_buffer_start == NULL) {
        network_send_buffer_release_slice(
                network_channel,
                0);
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        return false;
    }

    return true;
}

bool module_redis_command_stream_entry_range_with_one_chunk(
        network_channel_t *network_channel,
        storage_db_t *db,
        storage_db_entry_index_t *entry_index,
        off_t offset,
        size_t length) {
    bool result_res = false;
    network_channel_buffer_data_t *send_buffer = NULL, *send_buffer_start = NULL, *send_buffer_end = NULL;
    storage_db_chunk_info_t *chunk_info;

    assert(entry_index->value.count == 1 && length + 32 <= NETWORK_CHANNEL_MAX_PACKET_SIZE);

    // Acquires a slice long enough to stream the data and the protocol bits
    if (unlikely(!module_redis_command_acquire_slice_and_write_blob_start(
            network_channel,
            length + 32,
            length,
            &send_buffer,
            &send_buffer_start,
            &send_buffer_end))) {
        goto end;
    }

    chunk_info = storage_db_chunk_sequence_get(
            &entry_index->value,
            0);

    if (unlikely(!storage_db_chunk_read(
            db,
            chunk_info,
            send_buffer_start,
            offset,
            length))) {
        goto end;
    }

    send_buffer_start += length;

    send_buffer_start = protocol_redis_writer_write_argument_blob_end(
            send_buffer_start,
            send_buffer_end - send_buffer_start);

    if (unlikely(send_buffer_start == NULL)) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        goto end;
    }

    network_send_buffer_release_slice(
            network_channel,
            send_buffer_start - send_buffer);

    send_buffer = NULL;
    result_res = true;

end:

    if (unlikely(send_buffer != NULL && !result_res)) {
        network_send_buffer_release_slice(
                network_channel,
                0);
    }

    return result_res;
}

bool module_redis_command_stream_entry_range_with_multiple_chunks(
        network_channel_t *network_channel,
        storage_db_t *db,
        storage_db_entry_index_t *entry_index,
        off_t offset,
        size_t length) {
    network_channel_buffer_data_t *send_buffer = NULL, *send_buffer_start = NULL, *send_buffer_end = NULL;
    storage_db_chunk_info_t *chunk_info = NULL;
    size_t slice_length = 32;

    assert(entry_index->value.count > 1 || length + 32 > NETWORK_CHANNEL_MAX_PACKET_SIZE);

    if (unlikely(!module_redis_command_acquire_slice_and_write_blob_start(
            network_channel,
            32,
            length,
            &send_buffer,
            &send_buffer_start,
            &send_buffer_end))) {
        return false;
    }

    network_send_buffer_release_slice(
            network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    size_t sent_data;
    storage_db_chunk_index_t chunk_index = 0;

    // Skip the chunks until it reaches one containing range_start
    for (; chunk_index < entry_index->value.count; chunk_index++) {
        chunk_info = storage_db_chunk_sequence_get(&entry_index->value, chunk_index);

        if (offset < chunk_info->chunk_length) {
            break;
        }

        offset -= chunk_info->chunk_length;
    }

    // Set sent_data to the value of range_start to skip the initial part of the first chunk selected to be sent
    sent_data = offset;

    // Build the chunks for the value
    for (; chunk_index < entry_index->value.count && length > 0; chunk_index++) {
        char *buffer_to_send;
        bool allocated_new_buffer = false;
        chunk_info = storage_db_chunk_sequence_get(&entry_index->value, chunk_index);

        if (unlikely((buffer_to_send = storage_db_get_chunk_data(
                db,
                chunk_info,
                &allocated_new_buffer)) == NULL)) {
            return false;
        }

        size_t chunk_length_to_send = length > chunk_info->chunk_length
                ? chunk_info->chunk_length
                : length + sent_data;
        do {
            size_t data_available_to_send_length = chunk_length_to_send - sent_data;
            size_t data_to_send_length =
                    data_available_to_send_length > NETWORK_CHANNEL_MAX_PACKET_SIZE
                    ? NETWORK_CHANNEL_MAX_PACKET_SIZE
                    : data_available_to_send_length;

            // TODO: check if it's the last chunk and, if yes, if it would fit in the send buffer with the protocol
            //       bits that have to be sent later without doing an implicit flush
            if (network_send_direct(
                    network_channel,
                    buffer_to_send + sent_data,
                    data_to_send_length) != NETWORK_OP_RESULT_OK) {
                if (allocated_new_buffer) {
                    ffma_mem_free(buffer_to_send);
                }

                return false;
            }

            sent_data += data_to_send_length;
            length -= data_to_send_length;
        } while (sent_data < chunk_length_to_send);

        assert(sent_data == chunk_length_to_send);

        if (allocated_new_buffer) {
            ffma_mem_free(buffer_to_send);
        }

        // Resets sent data at the end of the loop
        sent_data = 0;
    }

    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(
            network_channel,
            slice_length);
    if (unlikely(send_buffer_start == NULL)) {
        LOG_E(TAG, "Unable to acquire send buffer slice!");
        return false;
    }

    send_buffer_start = protocol_redis_writer_write_argument_blob_end(
            send_buffer_start,
            slice_length);
    network_send_buffer_release_slice(
            network_channel,
            send_buffer_start ? send_buffer_start - send_buffer : 0);

    if (unlikely(send_buffer_start == NULL)) {
        LOG_E(TAG, "buffer length incorrectly calculated, not enough space!");
        return false;
    }

    return true;
}

#if CACHEGRAND_MODULE_REDIS_COMMAND_DUMP_CONTEXT == 1
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
            "SHORT_STRING",
            "LONG_STRING",
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
        list += field_has_token_extra_padding;
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

        // Cast back the list_ptr to uintptr_t to assign it back to list
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
                        stdout, "%.*s (%lu)\n",
                        (int)*(size_t*)(base_addr + offsetof(module_redis_key_t, length)),
                        *(char**)(base_addr + offsetof(module_redis_key_t, key)),
                        *(size_t*)(base_addr + offsetof(module_redis_key_t, length)));
                break;

            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_PATTERN:
                fprintf(
                        stdout, "%.*s (%lu)\n",
                        (int)*(size_t*)(base_addr + offsetof(module_redis_pattern_t, length)),
                        *(char**)(base_addr + offsetof(module_redis_pattern_t, pattern)),
                        *(size_t*)(base_addr + offsetof(module_redis_pattern_t, length)));
                break;

            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_SHORT_STRING:
                fprintf(
                        stdout, "%.*s (%lu)\n",
                        (int)*(size_t*)(base_addr + offsetof(module_redis_short_string_t, length)),
                        *(char**)(base_addr + offsetof(module_redis_short_string_t, short_string)),
                        *(size_t*)(base_addr + offsetof(module_redis_short_string_t, length)));
                break;

            case MODULE_REDIS_COMMAND_ARGUMENT_TYPE_LONG_STRING:
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

                        fprintf(stdout, "%.*s\n", (int) chunk_sequence->size, buffer);
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
                // Print nothing, the value is contained in has_token
                fprintf(stdout, "\n");
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
