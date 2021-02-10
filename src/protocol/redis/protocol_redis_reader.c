#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "misc.h"
#include "xalloc.h"
#include "protocol_redis.h"

#include "protocol_redis_reader.h"

protocol_redis_reader_context_t* protocol_redis_reader_context_init() {
    protocol_redis_reader_context_t* context =
            (protocol_redis_reader_context_t*)xalloc_alloc_zero(sizeof(protocol_redis_reader_context_t));

    return context;
}

void protocol_redis_reader_context_arguments_free(protocol_redis_reader_context_t* context) {
    // Free the allocated memory for the args array
    if (context->arguments.count > 0) {
        for(int index = 0; index < context->arguments.count; index++) {
            if (context->arguments.list[index].copied_from_buffer) {
                xalloc_free(context->arguments.list[index].value);
            }
        }

        xalloc_free(context->arguments.list);
    }
}

int protocol_redis_reader_context_arguments_clone_current(protocol_redis_reader_context_t* context) {
    if (context->arguments.count == 0) {
        return -1;
    }

    long index = context->arguments.current.index;

    if (context->arguments.list[index].copied_from_buffer) {
        return -1;
    }

    char* value = xalloc_alloc_zero(context->arguments.current.length);

    memcpy(value, context->arguments.list[index].value, context->arguments.current.length);

    context->arguments.list[index].value = value;
    context->arguments.list[index].copied_from_buffer = true;

    return 0;
}

void protocol_redis_reader_context_reset(protocol_redis_reader_context_t* context) {
    protocol_redis_reader_context_arguments_free(context);
    memset(context, 0, sizeof(protocol_redis_reader_context_t));
}

void protocol_redis_reader_context_free(protocol_redis_reader_context_t* context) {
    protocol_redis_reader_context_arguments_free(context);
    xalloc_free(context);
}

long protocol_redis_reader_read(
        char* buffer,
        size_t length,
        protocol_redis_reader_context_t* context) {
    size_t read_offset = 0;
    char* buffer_end_ptr = buffer + length;

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
    if (context->state == PROTOCOL_REDIS_READER_STATE_BEGIN) {
        char first_byte = *(char*)buffer;
        bool is_inline = first_byte != PROTOCOL_REDIS_TYPE_ARRAY;

        if (is_inline) {
            context->state = PROTOCOL_REDIS_READER_STATE_INLINE_WAITING_ARGUMENT;
            context->arguments.count = 0;
        } else {
            char *new_line_ptr = memchr(buffer, '\n', length);
            if (new_line_ptr == NULL) {
                // If the new line can't be find, it means that we received partial data and need to wait for more
                // before trying to parse again.
                // The state is not changed on purpose
                return 0;
            }

            // No need to error check if new_line_ptr - 1 < buffer, there is always at least 1 byte before
            if (*(new_line_ptr - 1) == '\r') {
                new_line_ptr--;
            }

            char *args_count_end_ptr = NULL;
            long args_count = strtol(buffer + 1, &args_count_end_ptr, 10);

            // Ensure that the tail ptr matches the new line, otherwise means there are invalid chars
            if (new_line_ptr != args_count_end_ptr) {
                context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH;
                return -1;
            }

            if (args_count <= 0) {
                context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH;
                return -1;
            }

            // TODO: should limit the number of allowed elements otherwise it can be used as DDOS attack

            context->arguments.count = args_count;

            // TODO: use a buffer pool
            context->arguments.list = xalloc_alloc_zero(
                    sizeof(protocol_redis_reader_context_argument_t) * context->arguments.count);

            unsigned long move_offset = new_line_ptr - buffer + 2;
            read_offset += move_offset;
            buffer += move_offset;
            length -= move_offset;

            context->state = PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_LENGTH;
        }

        context->arguments.current.index = -1;
        context->arguments.current.beginning = true;
        context->arguments.current.length = 0;
    }

    // PROTOCOL_REDIS_READER_STATE_INLINE_WAITING_ARGUMENT is inline protocol only
    if (context->state == PROTOCOL_REDIS_READER_STATE_INLINE_WAITING_ARGUMENT) {
        bool is_arg_end_newline = false;
        bool arg_end_char_found = false;
        char* arg_start_char_ptr = buffer;
        char* arg_end_char_ptr;
        unsigned long data_length;

        // Check if the string starts with a single or double quote.
        // If double quotes, need to parse chars in hex format \xHH
        // Every time a new argument is found, the array needs to be realloc and the counter incremented
        // TODO: improve it, it's crap

        // Check if it's the beginning of the command parsing
        if (context->arguments.current.beginning) {
            // Skip any additional space character
            do {
                if (*arg_start_char_ptr != ' ') {
                    break;
                }
            } while (arg_start_char_ptr++ && arg_start_char_ptr < buffer_end_ptr);

            // TODO: the parser has to be refactored to do not make assumption, the loop above may get on buffer_end_ptr
            //       and arg_start_char_ptr be not initialized!
            char first_byte = *arg_start_char_ptr;
            if (first_byte == '\'' || first_byte == '"') {
                context->arguments.inline_protocol.current.quote_char = first_byte;
                context->arguments.inline_protocol.current.decode_escaped_chars = first_byte == '"';

                arg_start_char_ptr++;
            } else {
                context->arguments.inline_protocol.current.quote_char = 0;
                context->arguments.inline_protocol.current.decode_escaped_chars = false;
            }
        }

        arg_end_char_ptr = arg_start_char_ptr;
        char arg_search_char = (context->arguments.inline_protocol.current.quote_char != 0)
                ? context->arguments.inline_protocol.current.quote_char
                : ' ';
        do {
            // Would be much better to use memchr but it's not possible to search for multiple characters. The do/while
            // loop below is slower than memchr but the inline_protocol (inline) protocol should be used only for debugging
            // or the simple HELLO/PING commands so doesn't matter
            do {
                // Check if it has found the end of the argument
                if (*arg_end_char_ptr == '\n' || *arg_end_char_ptr == arg_search_char) {
                    arg_end_char_found = true;
                    break;
                }
            } while (arg_end_char_ptr++ && arg_end_char_ptr < buffer_end_ptr);

            // Check if it has found the end char
            if (arg_end_char_found) {
                // Ensure that if a quote is expected the argument end char is not a new line, multiline commands are
                // not allowed in inline_protocol (inline) mode in redis
                if (context->arguments.inline_protocol.current.quote_char != 0 && *arg_end_char_ptr == '\n') {
                    context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES;
                    return -1;
                }

                // If the argument is wrapped in double quotes ensure that the quote found is not escaped
                if (context->arguments.inline_protocol.current.quote_char != 0 &&
                    context->arguments.inline_protocol.current.decode_escaped_chars) {
                    // Count the number of backslashes, if any
                    char* backslash_char_ptr = arg_end_char_ptr;
                    uint64_t backslash_count = 0;
                    while(*--backslash_char_ptr == '\\') {
                        backslash_count++;
                    }

                    // If found
                    if (backslash_count > 0) {
                        if ((backslash_count % 2) == 1) {
                            // The quote char is escaped, it's not the end of the string, has to keep searching
                            arg_end_char_found = false;
                            arg_end_char_ptr++;
                        }
                    }
                }
            }
        } while (arg_end_char_found == false && arg_end_char_ptr < buffer_end_ptr);

        // Calculate the read offset, includes any space and new line if present to avoid useless operations afterwards
        char *read_offset_arg_end_char_ptr = arg_end_char_ptr;

        if (arg_end_char_found) {
            if (context->arguments.inline_protocol.current.quote_char != 0) {
                read_offset_arg_end_char_ptr++;
            }

            // Count in the read offset any empty space after the arg end char
            do {
                if (*read_offset_arg_end_char_ptr != ' ') {
                    break;
                }
            } while (read_offset_arg_end_char_ptr++ && read_offset_arg_end_char_ptr < buffer_end_ptr);

            is_arg_end_newline = *read_offset_arg_end_char_ptr == '\n';

            // If the argument doesn't end with a new line, check if it's followed by one to avoid useless operations
            if (!is_arg_end_newline) {
                // Check both \n and \r\n after the end of the
                for (int newlinechars = 2; newlinechars > 0; newlinechars--) {
                    if (read_offset_arg_end_char_ptr + newlinechars < buffer_end_ptr &&
                        *(read_offset_arg_end_char_ptr + newlinechars) == '\n') {
                        read_offset_arg_end_char_ptr += newlinechars;
                        is_arg_end_newline = true;
                        break;
                    }
                }
            }

            // If a new line has been found, move over the new line
            if (is_arg_end_newline) {
                read_offset_arg_end_char_ptr++;
            }
        }

        // Update the read offset, move forward the initial point of the buffer and update the length
        read_offset += read_offset_arg_end_char_ptr - buffer;
        buffer += read_offset;
        length -= read_offset;

        // Calculate the data length but first remove any carriage feed if present
        if (arg_end_char_found && *(arg_end_char_ptr - 1) == '\r') {
            arg_end_char_ptr--;
        }

        data_length = arg_end_char_ptr - arg_start_char_ptr;

        // If the data length is 0, it's just a new line and can be skipped
        if (data_length > 0) {
            // Update the context, if the flag beginning is set to true the system has also take care of allocating
            // the necessary memory
            if (context->arguments.current.beginning) {
                // Increments the counter of the arguments and reallocate the memory. Because of this late initialization
                // count starts from 0 and current.index starts from -1
                context->arguments.count++;
                context->arguments.current.index++;

                // Set the data length
                context->arguments.current.length = data_length;

                // Increase the size of the list and update the new element
                context->arguments.list = xalloc_realloc(
                        context->arguments.list,
                        sizeof(protocol_redis_reader_context_argument_t) * context->arguments.count);
                context->arguments.list[context->arguments.current.index].value = arg_start_char_ptr;
                context->arguments.list[context->arguments.current.index].length = context->arguments.current.length;
                context->arguments.list[context->arguments.current.index].copied_from_buffer = false;
                context->arguments.list[context->arguments.current.index].all_read = false;

                // Mark that it's not the beginning of the parsing of the current argument anymore
                context->arguments.current.beginning = false;
            } else {
                // If the data are not from the buffer, it needs to resize the allocated memory and copy the new data
                if (context->arguments.list[context->arguments.current.index].copied_from_buffer) {
                    context->arguments.list[context->arguments.current.index].value = xalloc_realloc(
                            context->arguments.list[context->arguments.current.index].value,
                            context->arguments.current.length + data_length);

                    char* value_dest =
                            context->arguments.list[context->arguments.current.index].value +
                                    context->arguments.current.length;
                    memcpy(value_dest, arg_start_char_ptr, data_length);
                }

                // Updates the length
                context->arguments.current.length += data_length;
                context->arguments.list[context->arguments.current.index].length = context->arguments.current.length;
            }
        }

        if (arg_end_char_found) {
            if (context->arguments.current.index > -1) {
                context->arguments.list[context->arguments.current.index].all_read = true;
            }

            context->arguments.current.length = 0;
            context->arguments.current.beginning = true;

            if (is_arg_end_newline) {
                context->state = PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED;
            }
        }
    }

    if (length > 0 && context->state == PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_LENGTH) {
        char first_byte = *(char*)buffer;
        bool is_blob_string = first_byte == PROTOCOL_REDIS_TYPE_BLOB_STRING;

        if (!is_blob_string) {
            context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_EXPECTED;
            return -1;
        }

        char *new_line_ptr = memchr(buffer, '\n', length);
        if (new_line_ptr == NULL) {
            // If the new line can't be find, it means that we received partial data and need to wait for more
            // before trying to parse again.
            // The state is not changed on purpose
            return read_offset;
        }

        // No need to error check if new_line_ptr - 1 < buffer, there is always at least 1 byte before
        if (*(new_line_ptr - 1) == '\r') {
            new_line_ptr--;
        }

        char *data_length_end_ptr = NULL;
        long data_length = strtol(buffer + 1, &data_length_end_ptr, 10);

        // Ensure that the tail ptr matches the new line, otherwise means there are invalid chars
        if (new_line_ptr != data_length_end_ptr) {
            context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH;
            return -1;
        }

        if (data_length < 0) {
            context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH;
            return -1;
        }

        // Increase read_offset
        unsigned long move_offset = new_line_ptr - buffer + 2;
        read_offset += move_offset;

        // Move forward the initial point of the buffer and update the length
        buffer += move_offset;
        length -= move_offset;

        // Update the status of the current argument
        context->arguments.current.index++;
        context->arguments.current.length = data_length;
        context->arguments.resp_protocol.current.received_length = 0;

        context->arguments.list[context->arguments.current.index].value = buffer;
        context->arguments.list[context->arguments.current.index].length = data_length;
        context->arguments.list[context->arguments.current.index].copied_from_buffer = false;
        context->arguments.list[context->arguments.current.index].all_read = false;

        // Change the state to PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA only if there are data
        if (data_length > 0) {
            context->state = PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA;
        } else {
            if (context->arguments.current.index == context->arguments.count - 1) {
                context->state = PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED;
            }
        }
    }

    if (length > 0 && context->state == PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_DATA) {
        // Check if the current buffer may contain the entire / remaining response, take into account that every data
        // blob has to be followed by a \r\n
        size_t waiting_data_length =
                context->arguments.current.length -
                context->arguments.resp_protocol.current.received_length +
                2;

        if (length < waiting_data_length) {
            read_offset += length;
            context->arguments.resp_protocol.current.received_length += length;
        } else {
            // Check if the end of the data has the proper signature (\r\n)
            char* signature_ptr = buffer + waiting_data_length - 2;
            if (*signature_ptr != '\r' || *(signature_ptr + 1) != '\n') {
                context->error = PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_MISSING_END_SIGNATURE;
                return -1;
            }

            read_offset += waiting_data_length;
            buffer += waiting_data_length;
            length -= waiting_data_length;

            // Update the current status
            context->arguments.current.length = 0;
            context->arguments.current.beginning = true;

            context->arguments.list[context->arguments.current.index].all_read = true;

            // Check if this is the last argument of the array
            if (context->arguments.current.index == context->arguments.count - 1) {
                context->state = PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED;
            } else {
                context->state = PROTOCOL_REDIS_READER_STATE_RESP_WAITING_ARGUMENT_LENGTH;
            }
        }
    }

    return read_offset;
}
