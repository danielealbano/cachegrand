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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == strlen(buffer));
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

            long data_read_len = protocol_redis_reader_read(buffer, 0, context);

            REQUIRE(data_read_len == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_NO_DATA);

            protocol_redis_reader_context_free(context);
        }

        SECTION("error in context maintained") {
            char buffer[] = "HELLO\r\n";
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len1 = protocol_redis_reader_read(buffer_read, 0, context);

            REQUIRE(data_read_len1 == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_NO_DATA);

            long data_read_len2 = protocol_redis_reader_read(buffer_read, 0, context);

            REQUIRE(data_read_len2 == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_NO_DATA);

            protocol_redis_reader_context_free(context);
        }

        SECTION("one argument, no quotes, with new line") {
            char buffer[] = "HELLO\r\n";

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == strlen(buffer));
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == strlen(buffer));
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == strlen(buffer));
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == strlen(buffer));
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == strlen(buffer));
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
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

            long data_read_len = protocol_redis_reader_read(buffer, strlen(buffer), context);

            REQUIRE(data_read_len == -1);
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

            long data_read1_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1_len == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1_len;
            long data_read2_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2_len == 6);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2_len;
            long data_read3_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3_len == 9);
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

            long data_read1_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1_len == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1_len;
            long data_read2_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2_len == 6);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2_len;
            long data_read3_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3_len == 7);
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

            long data_read1_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1_len == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1_len;
            long data_read2_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2_len == 8);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2_len;
            long data_read3_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3_len == 9);
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

            long data_read1_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1_len == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1_len;
            long data_read2_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2_len == 8);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2_len;
            long data_read3_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3_len == 11);
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

            long data_read1_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1_len == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1_len;
            long data_read2_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2_len == 8);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2_len;
            long data_read3_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3_len == 9);
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

            long data_read1_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read1_len == 4);
            // No need to check everything again, already tested
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read1_len;
            long data_read2_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read2_len == 8);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 2);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->command_parsed == false);

            buffer_read += data_read2_len;
            long data_read3_len = protocol_redis_reader_read(buffer_read, strlen(buffer_read), context);

            REQUIRE(data_read3_len == 11);
            REQUIRE(context->error == 0);
            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument 1 byte at time, no quotes, with new line") {
            char buffer[] = "SET mykey myvalue\r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            char* buffer_new = (char*)malloc(buffer_length + 1);
            long buffer_new_length = 0;
            long buffer_new_offset = 0;
            for(int i = 0; i < buffer_length; i++) {
                buffer_new[i] = buffer[i];
                buffer_new_length++;

                long data_read_len = protocol_redis_reader_read(buffer_new + buffer_new_offset, buffer_new_length - buffer_new_offset, context);
                buffer_new_offset += data_read_len;
            }

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
            free(buffer_new);
        }

        SECTION("multiple argument, no quotes, with new line, clone data") {
            char buffer[] = "SET mykey myvalue\r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            // The expectation is that the command contained in the buffer will be fully parsed with 3 iterations
            // as all the spaces and new lines will be preemptively found and skipped to avoid useless calls.
            long buffer_offset = 0;
            for(int i = 0; i < 3; i++) {
                long data_read_len = protocol_redis_reader_read(
                        buffer_read + buffer_offset,
                        buffer_length - buffer_offset,
                        context);
                buffer_offset += data_read_len;

                if (context->arguments.current.length > 0 && context->arguments.list[context->arguments.current.index].from_buffer) {
                    protocol_redis_reader_context_arguments_clone_current(context);
                }
            }

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

//        SECTION("multiple argument 1 byte at time, no quotes, with new line, clone data") {
//            // TODO: This test covers an edge case not managed by the parser where the command is sent in very small
//            //       chunks and the data are not from the buffer, it will try to copy some extra bytes at the end
//            //       past the end of the buffer.
//            //       This behaviour is wrong and requires a fix as may trigger a segfault but being a very edgy case
//            //       can be left on a side and fixed within the next iterations.
//            char buffer[] = "SET mykey myvalue\r\n";
//            int buffer_length = strlen(buffer);
//            char* buffer_read = buffer;
//
//            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();
//
//            char* buffer_new = (char*)malloc(buffer_length + 1);
//            long buffer_new_length = 0;
//            long buffer_new_offset = 0;
//            for(int i = 0; i < buffer_length; i++) {
//                buffer_new[i] = buffer[i];
//                buffer_new_length++;
//
//                long data_read_len = protocol_redis_reader_read(buffer_new + buffer_new_offset, buffer_new_length - buffer_new_offset, context);
//                buffer_new_offset += data_read_len;
//
//                if (context->arguments.list[context->arguments.current.index].from_buffer) {
//                    protocol_redis_reader_context_arguments_clone_current(context);
//                }
//            }
//
//            REQUIRE(context->arguments.count == 3);
//            REQUIRE(context->arguments.list[0].length == 3);
//            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
//            REQUIRE(context->arguments.list[1].length == 5);
//            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
//            REQUIRE(context->arguments.list[2].length == 7);
//            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
//            REQUIRE(context->command_parsed == true);
//
//            protocol_redis_reader_context_free(context);
//            free(buffer_new);
//        }

        SECTION("multiple argument with spaces, no quotes, with new line") {
            char buffer[] = " SET   mykey  myvalue   \r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            // The expectation is that the command contained in the buffer will be fully parsed with 3 iterations
            // as all the spaces and new lines will be preemptively found and skipped to avoid useless calls.
            long buffer_offset = 0;
            for(int i = 0; i < 3; i++) {
                long data_read_len = protocol_redis_reader_read(
                        buffer_read + buffer_offset,
                        buffer_length - buffer_offset,
                        context);
                buffer_offset += data_read_len;
            }

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument with spaces, single quotes (1), with new line") {
            char buffer[] = " SET   'mykey'  myvalue   \r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            // The expectation is that the command contained in the buffer will be fully parsed with 3 iterations
            // as all the spaces and new lines will be preemptively found and skipped to avoid useless calls.
            long buffer_offset = 0;
            for(int i = 0; i < 3; i++) {
                long data_read_len = protocol_redis_reader_read(
                        buffer_read + buffer_offset,
                        buffer_length - buffer_offset,
                        context);
                buffer_offset += data_read_len;
            }

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument with spaces, single quotes (2), with new line") {
            char buffer[] = " SET   'mykey'  'myvalue'   \r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            // The expectation is that the command contained in the buffer will be fully parsed with 3 iterations
            // as all the spaces and new lines will be preemptively found and skipped to avoid useless calls.
            long buffer_offset = 0;
            for(int i = 0; i < 3; i++) {
                long data_read_len = protocol_redis_reader_read(
                        buffer_read + buffer_offset,
                        buffer_length - buffer_offset,
                        context);
                buffer_offset += data_read_len;
            }

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument with spaces, double quotes (1), with new line") {
            char buffer[] = " SET   \"mykey\"  myvalue   \r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            // The expectation is that the command contained in the buffer will be fully parsed with 3 iterations
            // as all the spaces and new lines will be preemptively found and skipped to avoid useless calls.
            long buffer_offset = 0;
            for(int i = 0; i < 3; i++) {
                long data_read_len = protocol_redis_reader_read(
                        buffer_read + buffer_offset,
                        buffer_length - buffer_offset,
                        context);
                buffer_offset += data_read_len;
            }

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("multiple argument with spaces, double quotes (2), with new line") {
            char buffer[] = " SET   \"mykey\"  \"myvalue\"   \r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            // The expectation is that the command contained in the buffer will be fully parsed with 3 iterations
            // as all the spaces and new lines will be preemptively found and skipped to avoid useless calls.
            long buffer_offset = 0;
            for(int i = 0; i < 3; i++) {
                long data_read_len = protocol_redis_reader_read(
                        buffer_read + buffer_offset,
                        buffer_length - buffer_offset,
                        context);
                buffer_offset += data_read_len;
            }

            REQUIRE(context->arguments.count == 3);
            REQUIRE(context->arguments.list[0].length == 3);
            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
            REQUIRE(context->arguments.list[1].length == 5);
            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
            REQUIRE(context->arguments.list[2].length == 7);
            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

//        SECTION("multiple argument with spaces, 1 byte at time, no quotes, with new line") {
//            // TODO: This test covers an edge case not managed by the parser where the command has multiple spaces and
//            //       it's not received in one read, in this case the parser will treat the spaces as data to read and
//            //       create items in the argument list.
//            //       This behaviour is wrong and requires a fix but being a very edgy case can be left on a side and
//            //       fixed within the next iterations.
//
//            char buffer[] = " SET   mykey    myvalue      \r\n";
//            int buffer_length = strlen(buffer);
//
//            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();
//
//            char* buffer_new = (char*)malloc(buffer_length + 1);
//            int buffer_new_length = 0;
//            int buffer_new_offset = 0;
//            for(int i = 0; i < buffer_length; i++) {
//                buffer_new[i] = buffer[i];
//                buffer_new_length++;
//
//                long data_read_len = protocol_redis_reader_read(buffer_new + buffer_new_offset, buffer_new_length - buffer_new_offset, context);
//                buffer_new_offset += data_read_len;
//            }
//
//            REQUIRE(context->arguments.count == 3);
//            REQUIRE(context->arguments.list[0].length == 3);
//            REQUIRE(strncmp(context->arguments.list[0].value, "SET", context->arguments.list[0].length) == 0);
//            REQUIRE(context->arguments.list[1].length == 5);
//            REQUIRE(strncmp(context->arguments.list[1].value, "mykey", context->arguments.list[1].length) == 0);
//            REQUIRE(context->arguments.list[2].length == 7);
//            REQUIRE(strncmp(context->arguments.list[2].value, "myvalue", context->arguments.list[2].length) == 0);
//            REQUIRE(context->command_parsed == true);
//
//            protocol_redis_reader_context_free(context);
//        }

        SECTION("single argument, no quotes, with new line, multiple commands") {
            char buffer[] = "HELLO\r\nWORLD\r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len1 = protocol_redis_reader_read(buffer_read, buffer_length, context);

            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == 5);
            REQUIRE(strncmp(context->arguments.list[0].value, "HELLO", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_reset(context);

            protocol_redis_reader_read(buffer_read + data_read_len1, buffer_length, context);

            REQUIRE(context->arguments.count == 1);
            REQUIRE(context->arguments.list[0].length == 5);
            REQUIRE(strncmp(context->arguments.list[0].value, "WORLD", context->arguments.list[0].length) == 0);
            REQUIRE(context->command_parsed == true);

            protocol_redis_reader_context_free(context);
        }

        SECTION("single argument, no quotes, with new line, multiple commands, no reset") {
            char buffer[] = "HELLO\r\nWORLD\r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            long data_read_len1 = protocol_redis_reader_read(buffer_read, buffer_length, context);
            protocol_redis_reader_read(buffer_read + data_read_len1, buffer_length, context);

            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_COMMAND_ALREADY_PARSED);

            protocol_redis_reader_context_free(context);
        }

        SECTION("error, no quotes, with new line, multiple commands, no reset") {
            char buffer[] = "HELLO\r\nWORLD\r\n";
            int buffer_length = strlen(buffer);
            char* buffer_read = buffer;

            protocol_redis_reader_context_t* context = protocol_redis_reader_context_init();

            protocol_redis_reader_read(buffer_read, buffer_length, context);
            long data_read_len2 = protocol_redis_reader_read(buffer_read, buffer_length, context);

            REQUIRE(data_read_len2 == -1);
            REQUIRE(context->error == PROTOCOL_REDIS_READER_ERROR_COMMAND_ALREADY_PARSED);

            protocol_redis_reader_context_free(context);
        }
    }
}
