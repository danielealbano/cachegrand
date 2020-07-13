#include "catch.hpp"

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#include "io_uring_supported.h"

TEST_CASE("io_uring_supported.c", "[io_uring_supported]") {
    SECTION("io_uring_supported_fetch_kallsyms_symbol_name") {
        SECTION("fetch symbol") {
            char name[500] = {0};
            char symbol_name_to_cmp[] = "symbol_test_name";
            char line_to_write[] = "0000000000000000 T symbol_test_name\n";

            int fd = memfd_create("io_uring_supported_fetch_kallsyms_symbol_name-symbol_found", 0);

            FILE *f = fdopen(fd, "w+");
            REQUIRE(fwrite(line_to_write, sizeof(line_to_write), 1, f) > 0);
            REQUIRE(fflush(f) == 0);
            REQUIRE(fseek(f, 0, SEEK_SET) == 0);
            REQUIRE(io_uring_supported_fetch_kallsyms_symbol_name(
                    f,
                    name,
                    sizeof(name)));
            REQUIRE(strncmp(name, symbol_name_to_cmp, sizeof(symbol_name_to_cmp)) == 0);

            fclose(f);
            close(fd);
        }

        SECTION("can't fetch symbol") {
            char name[500] = {0};
            char symbol_name_to_cmp[] = "symbol_test_name";
            char line_to_write[] = "0000 this is a mangle symbol line! T symbol_test_name\n";

            int fd = memfd_create("io_uring_supported_fetch_kallsyms_symbol_name-symbol_found", 0);

            FILE *f = fdopen(fd, "w+");
            REQUIRE(fwrite(line_to_write, sizeof(line_to_write), 1, f) > 0);
            REQUIRE(fflush(f) == 0);
            REQUIRE(fseek(f, 0, SEEK_SET) == 0);
            REQUIRE(io_uring_supported_fetch_kallsyms_symbol_name(
                    f,
                    name,
                    sizeof(name)));
            REQUIRE(strncmp(name, symbol_name_to_cmp, sizeof(symbol_name_to_cmp)) != 0);

            fclose(f);
            close(fd);
        }

        SECTION("fetch multiple symbols") {
            char name[500] = {0};
            char symbol_name_to_cmp_format[] = "symbol_test_name_%d";
            char line_to_write_format[] = "0000000000000000 T symbol_test_name_%d\n";
            uint8_t lines_count = 5;

            int fd = memfd_create("io_uring_supported_fetch_kallsyms_symbol_name-symbol_found", 0);

            FILE *f = fdopen(fd, "w+");

            for(uint8_t line_index = 0; line_index < lines_count; line_index++) {
                REQUIRE(fprintf(f, line_to_write_format, line_index) > 0);
            }

            REQUIRE(fflush(f) == 0);
            REQUIRE(fseek(f, 0, SEEK_SET) == 0);

            for(uint8_t line_index = 0; line_index < lines_count; line_index++) {
                REQUIRE(io_uring_supported_fetch_kallsyms_symbol_name(
                        f,
                        name,
                        sizeof(name)));

                char symbol_name_to_cmp[100] = {0};
                REQUIRE(snprintf(symbol_name_to_cmp, sizeof(symbol_name_to_cmp), symbol_name_to_cmp_format, line_index) > 0);
                REQUIRE(strncmp(name, symbol_name_to_cmp, sizeof(symbol_name_to_cmp)) == 0);
            }

            fclose(f);
            close(fd);
        }
    }

    SECTION("io_uring_supported_is_expected_symbol_name") {
        SECTION("match") {
            char symbol_name[] = "__zzz_private_io_uring_setup@1234.5678.90";
            REQUIRE(io_uring_supported_is_expected_symbol_name(
                    symbol_name,
                    sizeof(symbol_name)));
        }
        SECTION("no match") {
            char symbol_name[] = "_i_do_not_match@5678.90.1234";
            REQUIRE(!io_uring_supported_is_expected_symbol_name(
                    symbol_name,
                    sizeof(symbol_name)));
        }
    }

    SECTION("io_uring_supported_open_kallsyms") {
        // The file /proc/kallsyms is always expected to exist
        FILE *f = io_uring_supported_open_kallsyms();
        REQUIRE(f != NULL);
        fclose(f);
    }

    SECTION("io_uring_supported") {
        // Currently dummy test to expose problems, io_uring is always expected to be supported because is the only
        // I/O library currently implemented
        REQUIRE(io_uring_supported());
    }
}
