#include <catch2/catch.hpp>
#include <cstring>

#include "protocols/redis/protocol_redis_reader.h"


TEST_CASE("protocols/redis/protocol_redis_reader.c/resp", "[protocols][redis][protocol_redis_reader][resp]") {
    SECTION("protocol_redis_reader_read") {
        SECTION("empty array") {
            char buffer[] = "*0\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("invalid length array") {
            char buffer[] = "*0a\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, negative arguments count") {
            char buffer[] = "*-1\r\n$5\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, arguments count malformed length") {
            char buffer[] = "*1a\r\n$5\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_ARRAY_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument") {
            char buffer[] = "*1\r\n$5\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == 5);
            REQUIRE(strncmp(context->arguments.list[0].value, "HELLO", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 0);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, zero length") {
            char buffer[] = "*1\r\n$0\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 0);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("two arguments") {
            char buffer[] = "*2\r\n$5\r\nHELLO\r\n$8\r\nNEWWORLD\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len1 = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len1 == 15);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[0].length == 5);
            REQUIRE(strncmp(context->arguments.list[0].value, "HELLO", context->arguments.list[0].length) == 0);

            long data_read_len2 = protocol_redis_reader_read(buffer + data_read_len1, strlen(buffer) - data_read_len1, context);

            REQUIRE(data_read_len1 + data_read_len2 == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.list[1].length == 8);
            REQUIRE(strncmp(context->arguments.list[1].value, "NEWWORLD", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 1);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, no type") {
            char buffer[] = "*1\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_EXPECTED);

            protocol_redis_reader_context_free(context);
        }


        SECTION("one argument, malformed, argument negative length") {
            char buffer[] = "*1\r\n$-1\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, argument malformed length") {
            char buffer[] = "*1\r\n$5a\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_INVALID_LENGTH);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, malformed, argument incorrect length wrong signature") {
            char buffer[] = "*1\r\n$3\r\nHELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_BLOB_STRING_MISSING_END_SIGNATURE);

            protocol_redis_reader_context_free(context);
        }


        SECTION("multiple commands, multiple arguments") {
            char buffer[] = "*2\r\n$5\r\nFIRST\r\n$8\r\nARGUMENT\r\n*3\r\n$3\r\nFOR\r\n$2\r\nAN\r\n$12\r\nHELLO WORLD!\r\n";
            char* buffer_read = buffer;
            unsigned long buffer_length = strlen(buffer);
            long data_read_len;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            do {
                data_read_len = protocol_redis_reader_read(buffer_read, buffer_length, context);

                REQUIRE(data_read_len != -1);
                REQUIRE(context->error == 0);

                buffer_read += data_read_len;
                buffer_length -= data_read_len;
            } while(data_read_len != -1 && buffer_length > 0 && context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[0].length == 5);
            REQUIRE(strncmp(context->arguments.list[0].value, "FIRST", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 8);
            REQUIRE(strncmp(context->arguments.list[1].value, "ARGUMENT", context->arguments.list[1].length) == 0);

            protocol_redis_reader_context_reset(context);

            do {
                data_read_len = protocol_redis_reader_read(buffer_read, buffer_length, context);

                REQUIRE(data_read_len != -1);
                REQUIRE(context->error == 0);

                buffer_read += data_read_len;
                buffer_length -= data_read_len;
            } while(data_read_len != -1 && buffer_length > 0 && context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "FOR", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 2);
            REQUIRE(strncmp(context->arguments.list[1].value, "AN", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 12);
            REQUIRE(strncmp(context->arguments.list[2].value, "HELLO WORLD!", context->arguments.list[2].length) == 0);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple arguments, 1 byte at time") {
            char buffer[] = "*3\r\n$3\r\nFOR\r\n$2\r\nAN\r\n$12\r\nHELLO WORLD!\r\n";
            long buffer_length = strlen(buffer);

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            char* buffer_new = (char*)malloc(buffer_length + 1);
            memset(buffer_new, 0, buffer_length + 1);
            unsigned long buffer_new_length = 0;
            unsigned long buffer_new_offset = 0;
            for(int i = 0; i < buffer_length; i++) {
                buffer_new[i] = buffer[i];
                buffer_new_length++;

                long data_read_len = protocol_redis_reader_read(buffer_new + buffer_new_offset, buffer_new_length - buffer_new_offset, context);
                buffer_new_offset += data_read_len;
            }

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "FOR", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 2);
            REQUIRE(strncmp(context->arguments.list[1].value, "AN", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 12);
            REQUIRE(strncmp(context->arguments.list[2].value, "HELLO WORLD!", context->arguments.list[2].length) == 0);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple arguments, multiple buffers") {
            int buffers_count = 4;
            char* buffers[] = {
                    "*2\r\n$3\r\nFOR\r\n$35\r\n1234567890",
                    "ABCDEFGHIL",
                    "MNBVCXZLKJ12345",
                    "\r\n"
            };

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            for (int buffer_index = 0; buffer_index < buffers_count; buffer_index++) {
                char* buffer_read = buffers[buffer_index];
                long buffer_length = strlen(buffer_read);

                long data_read_len;

                do {
                    data_read_len = protocol_redis_reader_read(buffer_read, buffer_length, context);

                    REQUIRE(data_read_len != -1);
                    REQUIRE(context->error == 0);

                    buffer_read += data_read_len;
                    buffer_length -= data_read_len;
                } while(data_read_len != -1 && buffer_length > 0 && context->state != PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);
            }

            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "FOR", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 35);
            REQUIRE(strncmp(context->arguments.list[1].value, "1234567890", 10) == 0);
            REQUIRE(context->state == PROTOCOL_REDIS_READER_STATE_COMMAND_PARSED);

            protocol_redis_reader_context_free(context);
        }
    }
}
