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
#include <string.h>
#include <assert.h>

#include "misc.h"
#include "protocol_redis.h"

#include "protocol_redis_reader.h"

void protocol_redis_reader_context_reset(
        protocol_redis_reader_context_t* context) {
    memset(context, 0, sizeof(protocol_redis_reader_context_t));
}

int32_t protocol_redis_reader_read(
        char* buffer,
        size_t length,
        protocol_redis_reader_context_t* context,
        protocol_redis_reader_op_t* ops,
        uint8_t ops_size) {
    size_t read_offset = 0;
    uint8_t op_index = 0;

    // TODO: the function can be optimized to process the entire buffer till ops is filled but this requires (1) the
    //       ability to terminate the parsing once ops is full and (2) the ability to loop over the entire buffer that
    //       means basically wrapping all the code into this function in a giant while loop that terminates if (2.1) ops
    //       is full, (2.2) if the entire buffer has been processed (read_offset == length or buffer == buffer_end_ptr)
    //       or (2.3) if there is a parser error (i.e. all the various return -1 represent a non recoverable error)

    // Ensure there is going to be enough space to hold the maximum amount of data that can be processed
    assert(ops_size >= 6);

    // Ensure that there no errors reported in the context and there are data to parse
    if (unlikely(context->error != 0)) {
        return -1;
    } else if (unlikely(length == 0)) {
        context->error = PROTOCOL_REDIS_READER_ERROR_NO_DATA;
        return -1;
    } if (unlikely(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED)) {
        context->error = PROTOCOL_REDIS_READER_ERROR_COMMAND_ALREADY_PARSED;
        return -1;
    }

    // The reader has first to check if the state is BEGIN, it needs to identify if the command is resp (RESP3)
    // or if it is inlined checking the first byte.
    if (unlikely(context->state == PROTOCOL_REDIS_READER_STATE_BEGIN)) {
        char first_byte = *(char*)buffer;
        bool is_inline = first_byte != PROTOCOL_REDIS_TYPE_ARRAY;

        if (unlikely(is_inline)) {
            // The inline protocol support has still to be implemented by to support redis-benchmark there is an ad-hoc
            // check for the PING command, as redis-benchmark ignores the protocol type and sends the PING command
            // as inline.
            // This check will also work only if the PING command is sent as PING\r\n altogether, if the command will
            // be split among multiple packets it will not work.
            if (length >= 6 && strncmp(buffer, "PING\r\n", 6) == 0) {
                size_t command_length = 4;
                size_t data_read_len = 6;

                // Update the context status
                context->arguments.count = 1;
                context->state = PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED;

                // Update the various offsets (and pointers)
                read_offset += data_read_len;
                buffer += data_read_len;
                context->arguments.current.received_length += data_read_len;

                // Update the ops list
                ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN;
                ops[op_index].data_read_len = 0;
                ops[op_index].data.command.arguments_count = 1;
                op_index++;

                // Update the OPs list
                ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN;
                ops[op_index].data_read_len = 0;
                ops[op_index].data.argument.index = 0;
                ops[op_index].data.argument.length = command_length;
                op_index++;

                // Update the OPs list
                ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA;
                ops[op_index].data_read_len = (off_t)command_length;
                ops[op_index].data.argument.index = 0;
                ops[op_index].data.argument.length = command_length;
                ops[op_index].data.argument.offset = 0;
                ops[op_index].data.argument.data_length = command_length;
                op_index++;

                // Update the OPs list
                ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END;
                ops[op_index].data_read_len = 0;
                ops[op_index].data.argument.index = 0;
                ops[op_index].data.argument.length = command_length;
                ops[op_index].data.argument.offset = command_length;
                op_index++;

                // Update the OPs list
                ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END;
                ops[op_index].data_read_len = 2;
                ops[op_index].data.command.arguments_count = 1;
                op_index++;

                return op_index;
            }

            context->error = PROTOCOL_REDIS_READER_ERROR_INLINE_PROTOCOL_NOT_SUPPORTED;
            return -1;

//            context->protocol_type = PROTOCOL_REDIS_READER_PROTOCOL_TYPE_INLINE;
//            context->state = PROTOCOL_REDIS_READER_STATE_INLINE_WAITING_ARGUMENT;
//            context->arguments.count = 0;
        }

        char *new_line_ptr = memchr(buffer, '\n', length);
        if (unlikely(new_line_ptr == NULL)) {
            // If the new line can't be found, it means that we received partial data and need to wait for more
            // before trying to parse again, the state is not changed on purpose.
            return op_index;
        }

        // Ensure that there is at least 1 character and the \r before the found \n
        if (unlikely(new_line_ptr - buffer < 2 || *(new_line_ptr - 1) != '\r')) {
            context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH;
            return -1;
        }

        // Convert the argument count to a number
        char *args_count_end_ptr = NULL;
        long args_count = strtol(buffer + 1, &args_count_end_ptr, 10);

        if (new_line_ptr - 1 != args_count_end_ptr ||
            args_count <= 0) {
            context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH;
            return -1;
        }

        // Update context arguments count and allocates the memory for the arguments
        context->arguments.count = args_count;

        // Update the amount of processed data
        unsigned long move_offset = new_line_ptr - buffer + 1;
        read_offset += move_offset;
        buffer += move_offset;
        length -= move_offset;

        // Update the internal state
        context->protocol_type = PROTOCOL_REDIS_READER_PROTOCOL_TYPE_RESP;
        context->state = PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_LENGTH;
        context->arguments.current.index = -1;
        context->arguments.current.beginning = true;
        context->arguments.current.length = 0;

        // Update the ops list
        ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN;
        ops[op_index].data_read_len = (off_t)move_offset;
        ops[op_index].data.command.arguments_count = context->arguments.count;
        op_index++;
    }

    // The INLINE protocol parser has been disabled, supporting this variant of the protocol is not a priority as it is
    // unused in production systems and it's barely used for manual development / testing purposes (as it's just easier
    // to use redis-cli which properly supports RESP3)
//    // PROTOCOL_REDIS_READER_STATE_INLINE_WAITING_ARGUMENT is inline protocol only
//    // Set this as unlikely to give priority to the other code paths
//    if (unlikely(context->state == PROTOCOL_REDIS_READER_STATE_INLINE_WAITING_ARGUMENT)) {
//        bool is_arg_end_newline = false;
//        bool arg_end_char_found = false;
//        char* arg_start_char_ptr = buffer;
//        char* arg_end_char_ptr;
//        unsigned long data_length;
//
//        // Check if the string starts with a single or double quote.
//        // If double quotes, need to parse chars in hex format \xHH
//        // Every time a new argument is found, the array needs to be realloc and the counter incremented
//        // TODO: improve it, it's crap
//
//        // Check if it's the beginning of the command parsing
//        if (context->arguments.current.beginning) {
//            // Skip any additional space character
//            do {
//                if (*arg_start_char_ptr != ' ') {
//                    break;
//                }
//            } while (arg_start_char_ptr++ && arg_start_char_ptr < buffer_end_ptr);
//
//            // TODO: the parser has to be refactored to do not make assumption, the loop above may get on buffer_end_ptr
//            //       and arg_start_char_ptr be not initialized!
//            char first_byte = *arg_start_char_ptr;
//            if (first_byte == '\'' || first_byte == '"') {
//                context->arguments.inline_protocol.current.quote_char = first_byte;
//                context->arguments.inline_protocol.current.decode_escaped_chars = first_byte == '"';
//
//                arg_start_char_ptr++;
//            } else {
//                context->arguments.inline_protocol.current.quote_char = 0;
//                context->arguments.inline_protocol.current.decode_escaped_chars = false;
//            }
//        }
//
//        arg_end_char_ptr = arg_start_char_ptr;
//        char arg_search_char = (context->arguments.inline_protocol.current.quote_char != 0)
//                ? context->arguments.inline_protocol.current.quote_char
//                : ' ';
//        do {
//            // Would be much better to use memchr but it's not possible to search for multiple characters. The do/while
//            // loop below is slower than memchr but the inline_protocol (inline) protocol should be used only for debugging
//            // or the simple HELLO/PING commands so doesn't matter
//            do {
//                // Check if it has found the end of the argument
//                if (*arg_end_char_ptr == '\n' || *arg_end_char_ptr == arg_search_char) {
//                    arg_end_char_found = true;
//                    break;
//                }
//            } while (arg_end_char_ptr++ && arg_end_char_ptr < buffer_end_ptr);
//
//            // Check if it has found the end char
//            if (arg_end_char_found) {
//                // Ensure that if a quote is expected the argument end char is not a new line, multiline commands are
//                // not allowed in inline_protocol (inline) mode in redis
//                if (context->arguments.inline_protocol.current.quote_char != 0 && *arg_end_char_ptr == '\n') {
//                    context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES;
//                    return -1;
//                }
//
//                // If the argument is wrapped in double quotes ensure that the quote found is not escaped
//                if (context->arguments.inline_protocol.current.quote_char != 0 &&
//                    context->arguments.inline_protocol.current.decode_escaped_chars) {
//                    // Count the number of backslashes, if any
//                    char* backslash_char_ptr = arg_end_char_ptr;
//                    uint64_t backslash_count = 0;
//                    while(*--backslash_char_ptr == '\\') {
//                        backslash_count++;
//                    }
//
//                    // If found
//                    if (backslash_count > 0) {
//                        if ((backslash_count % 2) == 1) {
//                            // The quote char is escaped, it's not the end of the string, has to keep searching
//                            arg_end_char_found = false;
//                            arg_end_char_ptr++;
//                        }
//                    }
//                }
//            }
//        } while (arg_end_char_found == false && arg_end_char_ptr < buffer_end_ptr);
//
//        // Calculate the read offset, includes any space and new line if present to avoid useless operations afterwards
//        char *read_offset_arg_end_char_ptr = arg_end_char_ptr;
//
//        if (arg_end_char_found) {
//            if (context->arguments.inline_protocol.current.quote_char != 0) {
//                read_offset_arg_end_char_ptr++;
//            }
//
//            // Count in the read offset any empty space after the arg end char
//            do {
//                if (*read_offset_arg_end_char_ptr != ' ') {
//                    break;
//                }
//            } while (read_offset_arg_end_char_ptr++ && read_offset_arg_end_char_ptr < buffer_end_ptr);
//
//            is_arg_end_newline = *read_offset_arg_end_char_ptr == '\n';
//
//            // If the argument doesn't end with a new line, check if it's followed by one to avoid useless operations
//            if (!is_arg_end_newline) {
//                // Check both \n and \r\n after the end of the
//                for (int newlinechars = 2; newlinechars > 0; newlinechars--) {
//                    if (read_offset_arg_end_char_ptr + newlinechars < buffer_end_ptr &&
//                        *(read_offset_arg_end_char_ptr + newlinechars) == '\n') {
//                        read_offset_arg_end_char_ptr += newlinechars;
//                        is_arg_end_newline = true;
//                        break;
//                    }
//                }
//            }
//
//            // If a new line has been found, move over the new line
//            if (is_arg_end_newline) {
//                read_offset_arg_end_char_ptr++;
//            }
//        }
//
//        // Update the read offset, move forward the initial point of the buffer and update the length
//        read_offset += read_offset_arg_end_char_ptr - buffer;
//        buffer += read_offset;
//        length -= read_offset;
//
//        // Calculate the data length but first remove any carriage feed if present
//        if (arg_end_char_found && *(arg_end_char_ptr - 1) == '\r') {
//            arg_end_char_ptr--;
//        }
//
//        data_length = arg_end_char_ptr - arg_start_char_ptr;
//
//        // If the data length is 0, it's just a new line and can be skipped
//        if (data_length > 0) {
//            // Update the context, if the flag beginning is set to true the system has also take care of allocating
//            // the necessary memory
//            if (context->arguments.current.beginning) {
//                // Increments the counter of the arguments and reallocate the memory. Because of this late initialization
//                // count starts from 0 and current.index starts from -1
//                context->arguments.count++;
//                context->arguments.current.index++;
//
//                // Set the data length
//                context->arguments.current.length = data_length;
//
//                // Increase the size of the list and update the new element
//                context->arguments.list = ffma_mem_realloc(
//                        context->arguments.list,
//                        sizeof(protocol_redis_reader_context_argument_t) * (context->arguments.count - 1),
//                        sizeof(protocol_redis_reader_context_argument_t) * context->arguments.count,
//                        true);
//                context->arguments.list[context->arguments.current.index].value = arg_start_char_ptr;
//                context->arguments.list[context->arguments.current.index].length = context->arguments.current.length;
//                context->arguments.list[context->arguments.current.index].copied_from_buffer = false;
//                context->arguments.list[context->arguments.current.index].all_read = false;
//
//                // Mark that it's not the beginning of the parsing of the current argument anymore
//                context->arguments.current.beginning = false;
//            } else {
//                // If the data are not from the buffer, it needs to resize the allocated memory and copy the new data
//                if (context->arguments.list[context->arguments.current.index].copied_from_buffer) {
//                    context->arguments.list[context->arguments.current.index].value = ffma_mem_realloc(
//                            context->arguments.list[context->arguments.current.index].value,
//                            context->arguments.current.length,
//                            context->arguments.current.length + data_length,
//                            true);
//
//                    char* value_dest =
//                            context->arguments.list[context->arguments.current.index].value +
//                                    context->arguments.current.length;
//                    memcpy(value_dest, arg_start_char_ptr, data_length);
//                }
//
//                // Updates the length
//                context->arguments.current.length += data_length;
//                context->arguments.list[context->arguments.current.index].length = context->arguments.current.length;
//            }
//        }
//
//        if (arg_end_char_found) {
//            if (context->arguments.current.index > -1) {
//                context->arguments.list[context->arguments.current.index].all_read = true;
//            }
//
//            context->arguments.current.length = 0;
//            context->arguments.current.beginning = true;
//
//            if (is_arg_end_newline) {
//                context->state = PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED;
//            }
//        }
//    }

    if (length > 0 && context->state == PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_LENGTH) {
        // Only blob strings are allowed when making a request
        if (unlikely(*buffer != PROTOCOL_REDIS_TYPE_BLOB_STRING)) {
            context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_EXPECTED;
            return -1;
        }

        char *new_line_ptr = memchr(buffer, '\n', length);
        if (unlikely(new_line_ptr == NULL)) {
            // If the new line can't be found, it means that we received partial data and need to wait for more
            // before trying to parse again, the state is not changed on purpose.
            return op_index;
        }

        // Ensure that there is at least 1 charater and the \r before the found \n
        if (unlikely(new_line_ptr - buffer < 2 || *(new_line_ptr - 1) != '\r')) {
            context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH;
            return -1;
        }

        // Convert the data length to a number
        char *data_length_end_ptr = NULL;
        long data_length = strtol(buffer + 1, &data_length_end_ptr, 10);

        if (unlikely(new_line_ptr - 1 != data_length_end_ptr || data_length < 0)) {
            context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH;
            return -1;
        }

        // Update the amount of processed data
        unsigned long move_offset = new_line_ptr - buffer + 1;
        read_offset += move_offset;
        buffer += move_offset;
        length -= move_offset;

        // Update the status of the current argument
        context->arguments.current.index++;
        context->arguments.current.length = data_length;
        context->arguments.current.received_length = 0;

        // Update the OPs list
        ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN;
        ops[op_index].data_read_len = (off_t)move_offset;
        ops[op_index].data.argument.index = context->arguments.current.index;
        ops[op_index].data.argument.length = data_length;
        op_index++;

        // Change the state to PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA
        context->state = PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA;
    }

    if (length > 0 && context->state == PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA) {
        size_t data_length;
        bool end_found = false;
        size_t argument_waiting_data_length =
                context->arguments.current.length -
                context->arguments.current.received_length;

        // Determine the amount of data found
        if (length < argument_waiting_data_length) {
            data_length = length;
        } else {
            data_length = argument_waiting_data_length;
            end_found = true;
        }

        // Update the various offsets (and pointers)
        context->arguments.current.received_length += data_length;
        read_offset += data_length;
        buffer += data_length;
        length -= data_length;

        // Update the OPs list
        ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA;
        ops[op_index].data_read_len = (off_t)data_length;
        ops[op_index].data.argument.index = context->arguments.current.index;
        ops[op_index].data.argument.length = context->arguments.current.length;
        ops[op_index].data.argument.offset = read_offset - data_length;
        ops[op_index].data.argument.data_length = data_length;
        op_index++;

        if (end_found) {
            // Update the status
            context->state = PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA_END;
        }
    }

    if (length > 0 && context->state == PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA_END) {
        size_t waiting_data_length = 2;

        // Check if there are enough data to contain the argument data end signature
        if (likely(length >= waiting_data_length)) {
            // Check if the end of the data has the proper signature (\r\n)
            if (*buffer != '\r' || *(buffer + 1) != '\n') {
                context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_MISSING_END_SIGNATURE;
                return -1;
            }

            // Update the various offsets (and pointers)
            read_offset += waiting_data_length;
            buffer += waiting_data_length;
            context->arguments.current.received_length += waiting_data_length;

            // Update the OPs list
            ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END;
            ops[op_index].data_read_len = (off_t)waiting_data_length;
            ops[op_index].data.argument.index = context->arguments.current.index;
            ops[op_index].data.argument.length = context->arguments.current.length;
            ops[op_index].data.argument.offset = read_offset - waiting_data_length;
            op_index++;

            // Check if this is the last argument of the array
            if (context->arguments.current.index == context->arguments.count - 1) {
                context->state = PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED;

                // Update the OPs list
                ops[op_index].type = PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END;
                ops[op_index].data_read_len = 0;
                ops[op_index].data.command.arguments_count = context->arguments.count;
                op_index++;
            } else {
                context->state = PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_LENGTH;
            }
        }
    }

    return op_index;
}
