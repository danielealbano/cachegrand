#include <catch2/catch.hpp>
#include <cstring>

#include "exttypes.h"
#include "spinlock.h"

#include "protocols/redis/protocol_redis_reader.h"


TEST_CASE("protocols/redis/protocol_redis_reader.c", "[protocols][redis][protocol_redis_reader]") {
    SECTION("protocol_redis_reader_read") {
        SECTION("empty command, new line") {
            char buffer[] = "\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == -1);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("empty command, no new line") {
            char buffer[] = "";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == 0);
            REQUIRE(context->error == 0);
            REQUIRE(context->is_plaintext == false);
            REQUIRE(context->arguments.count == 0);
            REQUIRE(context->arguments.current.beginning == false);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 0);
            REQUIRE(context->command_parsed == false);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, no quotes, with new line") {
            char buffer[] = "HELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == strlen(buffer) - 2);
            REQUIRE(strncmp(context->arguments.list[0].value, buffer, context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, no quotes, without new line") {
            char buffer[] = "HELLO";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == strlen(buffer));
            REQUIRE(strncmp(context->arguments.list[0].value, buffer, context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.current.beginning == false);
            // No new line, the current length is not reset-ed to zero and beginning is not reset-ed to true
            REQUIRE(context->arguments.current.length == strlen(buffer));
            REQUIRE(context->arguments.current.index == 0);
            REQUIRE(context->command_parsed == false);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, single quotes, with new line") {
            char buffer[] = "'HELLO'\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == strlen(buffer) - 4);
            REQUIRE(strncmp(context->arguments.list[0].value, buffer + 1, context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.index == 0);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.plaintext.current.quote_char == '\'');
            REQUIRE(context->arguments.plaintext.current.decode_escaped_chars == false);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, double quotes, with new line") {
            char buffer[] = "\"HELLO\"\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == strlen(buffer) - 4);
            REQUIRE(strncmp(context->arguments.list[0].value, buffer + 1, context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 0);
            REQUIRE(context->arguments.plaintext.current.quote_char == '"');
            REQUIRE(context->arguments.plaintext.current.decode_escaped_chars == true);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, double quotes with backslashes, with new line") {
            char buffer[] = "\"HELLO\\\"\"\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == strlen(buffer));
            REQUIRE(context->error == 0);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == strlen(buffer) - 4);
            REQUIRE(strncmp(context->arguments.list[0].value, buffer + 1, context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == 0);
            REQUIRE(context->arguments.plaintext.current.quote_char == '"');
            REQUIRE(context->arguments.plaintext.current.decode_escaped_chars == true);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, single unbalanced quotes (1), with new line") {
            char buffer[] = "'HELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == -1);
            REQUIRE(context->command_parsed == false);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, single unbalanced quotes (2), with new line") {
            char buffer[] = "'HELLO\"\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == -1);
            REQUIRE(context->command_parsed == false);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, double unbalanced quotes (1), with new line") {
            char buffer[] = "\"HELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == -1);
            REQUIRE(context->command_parsed == false);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, double unbalanced quotes (2), with new line") {
            char buffer[] = "\"HELLO'\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == -1);
            REQUIRE(context->command_parsed == false);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, double unbalanced quotes with backslashes (2), with new line") {
            char buffer[] = "\"HELLO\\\"\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_ARGS_INLINE_UNBALANCED_QUOTES);
            REQUIRE(context->is_plaintext == true);
            REQUIRE(context->arguments.count == 0);
            REQUIRE(context->arguments.current.beginning == true);
            REQUIRE(context->arguments.current.length == 0);
            REQUIRE(context->arguments.current.index == -1);
            REQUIRE(context->command_parsed == false);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument, no quotes, with new line") {
            char buffer[] = "SET mykey myvalue\r\n";
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read1 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1 == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1;
            size_t data_read2 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2 == 6);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2;
            size_t data_read3 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3 == 9);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument, no quotes, without new line") {
            char buffer[] = "SET mykey myvalue";
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read1 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1 == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1;
            size_t data_read2 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2 == 6);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2;
            size_t data_read3 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3 == 7);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == false);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument, single quotes (1), with new line") {
            char buffer[] = "SET 'mykey' myvalue\r\n";
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read1 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1 == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1;
            size_t data_read2 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2 == 8);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2;
            size_t data_read3 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3 == 9);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument, single quotes (2), with new line") {
            char buffer[] = "SET 'mykey' 'myvalue'\r\n";
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read1 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1 == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1;
            size_t data_read2 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2 == 8);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2;
            size_t data_read3 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3 == 11);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument, double quotes (1), with new line") {
            char buffer[] = "SET \"mykey\" myvalue\r\n";
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read1 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1 == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1;
            size_t data_read2 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2 == 8);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2;
            size_t data_read3 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3 == 9);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument, double quotes (2), with new line") {
            char buffer[] = "SET \"mykey\" \"myvalue\"\r\n";
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            size_t data_read1 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1 == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1;
            size_t data_read2 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2 == 8);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2;
            size_t data_read3 = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3 == 11);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }
    }
}
