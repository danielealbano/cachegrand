#include <catch2/catch.hpp>
#include <cstring>

#include "protocol/redis/protocol_redis_reader.h"


TEST_CASE("protocols/redis/protocol_redis_reader.c/resp", "[protocols][redis][protocol_redis_reader][resp]") {
    SECTION("protocol_redis_reader_read") {
        protocol_redis_reader_op_t ops[8] = { };
        int32_t ops_size = 8;

        SECTION("empty array") {
            char buffer[] = "*0\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("invalid length array") {
            char buffer[] = "*0a\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, negative arguments count") {
            char buffer[] = "*-1\r\n$5\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, arguments count malformed length") {
            char buffer[] = "*1a\r\n$5\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument") {
            char buffer[] = "*1\r\n$5\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == 5);
            REQUIRE(context->error == 0);
            REQUIRE(ops[0].type == PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN);
            REQUIRE(ops[1].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN);
            REQUIRE(ops[2].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA);
            REQUIRE(ops[3].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END);
            REQUIRE(ops[4].type == PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END);
            REQUIRE(ops[2].data_read_len == 7);
            REQUIRE(ops[2].data.argument.length == 5);
            REQUIRE(ops[2].data.argument.data_length == 5);
            REQUIRE(ops[2].data.argument.offset == 5);
            REQUIRE(strncmp(buffer + ops[2].data.argument.offset, "HELLO", ops[2].data.argument.length) == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 1);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, zero length") {
            char buffer[] = "*1\r\n$0\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == 5);
            REQUIRE(context->error == 0);
            REQUIRE(ops[0].type == PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN);
            REQUIRE(ops[1].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN);
            REQUIRE(ops[2].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA);
            REQUIRE(ops[3].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END);
            REQUIRE(ops[4].type == PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END);
            REQUIRE(ops[2].data_read_len == 7);
            REQUIRE(ops[2].data.argument.length == 0);
            REQUIRE(ops[2].data.argument.data_length == 0);
            REQUIRE(ops[2].data.argument.offset == 5);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 1);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("two arguments") {
            char buffer[] = "*2\r\n$5\r\nHELLO\r\n$8\r\nNEWWORLD\r\n";
            off_t data_read_len1 = 0, data_read_len2 = 0;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(context->error == 0);
            REQUIRE(ops_found == 4);
            REQUIRE(ops[0].type == PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN);
            REQUIRE(ops[1].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN);
            REQUIRE(ops[2].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA);
            REQUIRE(ops[3].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END);

            for(int32_t op_index = 0; op_index < ops_found; op_index++) {
                data_read_len1 += ops[op_index].data_read_len;
            }

            REQUIRE(data_read_len1 == 15);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(ops[2].data.argument.length == 5);
            REQUIRE(strncmp(buffer + ops[2].data.argument.offset, "HELLO", ops[2].data.argument.length) == 0);

            ops_found = protocol_redis_reader_read(buffer, strlen(buffer) - data_read_len1, context, ops, ops_size);

            REQUIRE(context->error == 0);
            REQUIRE(ops_found == 4);
            REQUIRE(ops[0].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN);
            REQUIRE(ops[1].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA);
            REQUIRE(ops[2].type == PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END);
            REQUIRE(ops[3].type == PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END);

            for(int32_t op_index = 0; op_index < ops_found; op_index++) {
                data_read_len2 += ops[op_index].data_read_len;
            }

            REQUIRE(data_read_len1 + data_read_len2 == strlen(buffer));
            REQUIRE(ops[2].data.argument.length == 8);
            REQUIRE(strncmp(buffer + ops[2].data.argument.offset, "NEWWORLD", ops[2].data.argument.length) == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 1);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, no type") {
            char buffer[] = "*1\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_EXPECTED);

            protocol_redis_reader_context_free(context);
        }


        SECTION("one argument, malformed, argument negative length") {
            char buffer[] = "*1\r\n$-1\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, argument malformed length") {
            char buffer[] = "*1\r\n$5a\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, argument incorrect length wrong signature") {
            char buffer[] = "*1\r\n$3\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            int32_t ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

            REQUIRE(ops_found == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_MISSING_END_SIGNATURE);

            protocol_redis_reader_context_free(context);
        }


        SECTION("multiple commands, multiple arguments") {
            char buffer[] = "*2\r\n$5\r\nFIRST\r\n$8\r\nARGUMENT\r\n*3\r\n$3\r\nFOR\r\n$2\r\nAN\r\n$12\r\nHELLO WORLD!\r\n";
            protocol_redis_reader_op_type_t expected_op_types[] = {
                    // First command
                    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END,

                    // Second command
                    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END,
            };
            char* buffer_read = buffer;
            unsigned long buffer_length = strlen(buffer);
            off_t data_read_len = 0;
            uint32_t expected_op_type_index = 0;
            int32_t ops_found;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            do {
                ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);

                REQUIRE(ops_found != -1);
                REQUIRE(context->error == 0);

                for(int32_t op_index = 0; op_index < ops_found; op_index++) {
                    REQUIRE(expected_op_types[expected_op_type_index] == ops[op_index].type);
                    data_read_len += ops[op_index].data_read_len;

                    // Validate the number of arguments read with the first op index
                    if (expected_op_type_index == 0) {
                        REQUIRE(ops[op_index].data.command.arguments_count == 2);
                    }

                    // First argument
                    if (expected_op_type_index == 2) {
                        REQUIRE(ops[op_index].data.argument.length == 5);
                        REQUIRE(strncmp(buffer + ops[op_index].data.argument.offset, "FIRST", ops[op_index].data.argument.length) == 0);
                    }

                    // Second argument
                    if (expected_op_type_index == 5) {
                        REQUIRE(ops[op_index].data.argument.length == 8);
                        REQUIRE(strncmp(buffer + ops[op_index].data.argument.offset, "ARGUMENT", ops[op_index].data.argument.length) == 0);
                    }

                    expected_op_type_index++;
                }

                buffer_read += data_read_len;
                buffer_length -= data_read_len;
            } while(ops_found != -1 && buffer_length > 0 && context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            REQUIRE(ops_found != -1);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_reset(context);

            do {
                ops_found = protocol_redis_reader_read(buffer, strlen(buffer), context, ops, ops_size);
                REQUIRE(ops_found != -1);
                REQUIRE(context->error == 0);

                for(int32_t op_index = 0; op_index < ops_found; op_index++) {
                    REQUIRE(expected_op_types[expected_op_type_index] == ops[op_index].type);
                    data_read_len += ops[op_index].data_read_len;

                    // Validate the number of arguments read with the first op index
                    if (expected_op_type_index == 9) {
                        REQUIRE(ops[op_index].data.command.arguments_count == 3);
                    }

                    // First argument
                    if (expected_op_type_index == 10) {
                        REQUIRE(ops[op_index].data.argument.length == 3);
                        REQUIRE(strncmp(buffer + ops[op_index].data.argument.offset, "FOR", ops[op_index].data.argument.length) == 0);
                    }

                    // Second argument
                    if (expected_op_type_index == 13) {
                        REQUIRE(ops[op_index].data.argument.length == 2);
                        REQUIRE(strncmp(buffer + ops[op_index].data.argument.offset, "AN", ops[op_index].data.argument.length) == 0);
                    }

                    // Third argument
                    if (expected_op_type_index == 16) {
                        REQUIRE(ops[op_index].data.argument.length == 12);
                        REQUIRE(strncmp(buffer + ops[op_index].data.argument.offset, "HELLO WORLD!", ops[op_index].data.argument.length) == 0);
                    }

                    expected_op_type_index++;
                }

                buffer_read += data_read_len;
                buffer_length -= data_read_len;
            } while(ops_found != -1 && buffer_length > 0 && context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple arguments, 1 byte at time") {
            char buffer[] = "*3\r\n$3\r\nFOR\r\n$2\r\nAN\r\n$12\r\nHELLO WORLD!\r\n";
            protocol_redis_reader_op_type_t expected_op_types[] = {
                    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END,
            };
            size_t buffer_length = strlen(buffer);
            off_t data_read_len = 0;
            uint32_t expected_op_type_index = 0;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            char* buffer_new = (char*)malloc(buffer_length + 1);
            memset(buffer_new, 0, buffer_length + 1);
            unsigned long buffer_new_length = 0;
            unsigned long buffer_new_offset = 0;
            for(int i = 0; i < buffer_length; i++) {
                buffer_new[i] = buffer[i];
                buffer_new_length++;

                int32_t ops_found = protocol_redis_reader_read(buffer_new + buffer_new_offset, buffer_new_length - buffer_new_offset, context, ops, ops_size);
                REQUIRE(ops_found != -1);
                REQUIRE(context->error == 0);

                for(int32_t op_index = 0; op_index < ops_found; op_index++) {
                    REQUIRE(expected_op_types[expected_op_type_index] == ops[op_index].type);
                    data_read_len += ops[op_index].data_read_len;

                    // Validate the number of arguments read with the first op index
                    if (expected_op_type_index == 0) {
                        REQUIRE(ops[op_index].data.command.arguments_count == 3);
                    }

                    // First argument
                    if (expected_op_type_index == 2) {
                        REQUIRE(ops[op_index].data.argument.length == 3);
                        REQUIRE(strncmp(buffer + ops[op_index].data.argument.offset, "FOR", ops[op_index].data.argument.length) == 0);
                    }

                    // Second argument
                    if (expected_op_type_index == 5) {
                        REQUIRE(ops[op_index].data.argument.length == 2);
                        REQUIRE(strncmp(buffer + ops[op_index].data.argument.offset, "AN", ops[op_index].data.argument.length) == 0);
                    }

                    // Third argument
                    if (expected_op_type_index == 8) {
                        REQUIRE(ops[op_index].data.argument.length == 12);
                        REQUIRE(strncmp(buffer + ops[op_index].data.argument.offset, "HELLO WORLD!", ops[op_index].data.argument.length) == 0);
                    }

                    expected_op_type_index++;
                }
            }

            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple arguments, multiple buffers") {
            int buffers_count = 4;
            char buffers_stack[][50] = {
                    "*2\r\n$3\r\nFOR\r\n$35\r\n1234567890\0",
                    "ABCDEFGHIL\0",
                    "MNBVCXZLKJ12345\0",
                    "\r\n\0"
            };
            protocol_redis_reader_op_type_t expected_op_types[] = {
                    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_BEGIN,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_DATA,
                    PROTOCOL_REDIS_READER_OP_TYPE_ARGUMENT_END,
                    PROTOCOL_REDIS_READER_OP_TYPE_COMMAND_END,
            };
            off_t data_read_len = 0;
            uint32_t expected_op_type_index = 0;
            int32_t ops_found;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            for (int buffer_index = 0; buffer_index < buffers_count; buffer_index++) {
                char* buffer_read = buffers_stack[buffer_index];
                long buffer_length = strlen(buffer_read);
                data_read_len = 0;

                do {
                    ops_found = protocol_redis_reader_read(buffer_read, buffer_length, context, ops, ops_size);

                    REQUIRE(ops_found != -1);
                    REQUIRE(context->error == 0);

                    for(int32_t op_index = 0; op_index < ops_found; op_index++) {
                        REQUIRE(expected_op_types[expected_op_type_index] == ops[op_index].type);
                        data_read_len += ops[op_index].data_read_len;

                        // Validate the number of arguments read with the first op index
                        if (expected_op_type_index == 0) {
                            REQUIRE(ops[op_index].data.command.arguments_count == 3);
                        }

                        // First argument
                        if (expected_op_type_index == 2) {
                            REQUIRE(ops[op_index].data.argument.length == 3);
                            REQUIRE(strncmp(buffer_read + ops[op_index].data.argument.offset, "FOR", ops[op_index].data.argument.length) == 0);
                        }

                        // Second argument first block of data
                        if (expected_op_type_index == 5) {
                            REQUIRE(ops[op_index].data.argument.length == 35);
                            REQUIRE(ops[op_index].data.argument.data_length == 10);
                            REQUIRE(strncmp(buffer_read + ops[op_index].data.argument.offset, "1234567890", ops[op_index].data.argument.length) == 0);
                        }

                        // Second argument second block of data
                        if (expected_op_type_index == 6) {
                            REQUIRE(ops[op_index].data.argument.length == 35);
                            REQUIRE(ops[op_index].data.argument.data_length == 10);
                            REQUIRE(strncmp(buffer_read + ops[op_index].data.argument.offset, "ABCDEFGHIL", ops[op_index].data.argument.length) == 0);
                        }

                        // Second argument third block of data
                        if (expected_op_type_index == 7) {
                            REQUIRE(ops[op_index].data.argument.length == 35);
                            REQUIRE(ops[op_index].data.argument.data_length == 15);
                            REQUIRE(strncmp(buffer_read + ops[op_index].data.argument.offset, "MNBVCXZLKJ12345", ops[op_index].data.argument.length) == 0);
                        }

                        expected_op_type_index++;
                    }

                    buffer_read += data_read_len;
                    buffer_length -= data_read_len;
                } while(ops_found != -1 && buffer_length > 0 && context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);
            }

            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }
    }
}
