#include <catch2/catch.hpp>

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <liburing.h>

#include "support/io_uring/io_uring_support.h"
#include "support/io_uring/io_uring_capabilities.h"

TEST_CASE("support/io_uring/io_uring_capabilities.c", "[support][io_uring][io_uring_capabilities_is_supported]") {
    SECTION("io_uring_capabilities_kallsyms_fetch_symbol_name") {
        SECTION("fetch symbol") {
            char name[500] = {0};
            char symbol_name_to_cmp[] = "symbol_test_name";
            char line_to_write[] = "0000000000000000 T symbol_test_name\n";

            int fd = memfd_create("io_uring_capabilities_kallsyms_fetch_symbol_name-symbol_found", 0);

            FILE *f = fdopen(fd, "w+");
            REQUIRE(fwrite(line_to_write, sizeof(line_to_write), 1, f) > 0);
            REQUIRE(fflush(f) == 0);
            REQUIRE(fseek(f, 0, SEEK_SET) == 0);
            REQUIRE(io_uring_capabilities_kallsyms_fetch_symbol_name(
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

            int fd = memfd_create("io_uring_capabilities_kallsyms_fetch_symbol_name-symbol_found", 0);

            FILE *f = fdopen(fd, "w+");
            REQUIRE(fwrite(line_to_write, sizeof(line_to_write), 1, f) > 0);
            REQUIRE(fflush(f) == 0);
            REQUIRE(fseek(f, 0, SEEK_SET) == 0);
            REQUIRE(io_uring_capabilities_kallsyms_fetch_symbol_name(
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

            int fd = memfd_create("io_uring_capabilities_kallsyms_fetch_symbol_name-symbol_found", 0);

            FILE *f = fdopen(fd, "w+");

            for(uint8_t line_index = 0; line_index < lines_count; line_index++) {
                REQUIRE(fprintf(f, line_to_write_format, line_index) > 0);
            }

            REQUIRE(fflush(f) == 0);
            REQUIRE(fseek(f, 0, SEEK_SET) == 0);

            for(uint8_t line_index = 0; line_index < lines_count; line_index++) {
                REQUIRE(io_uring_capabilities_kallsyms_fetch_symbol_name(
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

    SECTION("io_uring_capabilities_kallsyms_is_expected_symbol_name") {
        SECTION("match") {
            char symbol_name[] = "__zzz_private_io_uring_setup@1234.5678.90";
            REQUIRE(io_uring_capabilities_kallsyms_is_expected_symbol_name(
                    symbol_name,
                    sizeof(symbol_name)));
        }
        SECTION("no match") {
            char symbol_name[] = "_i_do_not_match@5678.90.1234";
            REQUIRE(!io_uring_capabilities_kallsyms_is_expected_symbol_name(
                    symbol_name,
                    sizeof(symbol_name)));
        }
    }

    SECTION("io_uring_capabilities_kallsyms_open") {
        // The file /proc/kallsyms is always expected to exist
        FILE *f = io_uring_capabilities_kallsyms_open();
        REQUIRE(f != NULL);
        fclose(f);
    }

    SECTION("io_uring_capabilities_is_linked_op_files_update_supported") {
        io_uring_t *ring;
        io_uring_sqe_t *sqe = NULL;
        io_uring_cqe_t *cqe = NULL;
        bool ret;
        int fd = 1;
        uint32_t files_map_count = 16;
        const int files_map_registered[16] = {0};

        ring = io_uring_support_init(16, NULL, NULL);

        REQUIRE(ring != NULL);
        REQUIRE(io_uring_register_files(ring, files_map_registered, files_map_count) >= 0);

        sqe = io_uring_support_get_sqe(ring);
        REQUIRE(sqe != NULL);
        io_uring_prep_files_update(sqe, &fd, 1, 10);
        io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);

        REQUIRE(io_uring_support_sqe_enqueue_nop(ring, 0, 0));
        REQUIRE(io_uring_support_sqe_submit(ring));

        io_uring_wait_cqe(ring, &cqe);

        ret = cqe->res == 1;

        io_uring_support_free(ring);

        REQUIRE(ret == io_uring_capabilities_is_linked_op_files_update_supported());
    }

    SECTION("io_uring_capabilities_kallsyms_ensure_iouring_available") {
        // Currently dummy test to expose problems, io_uring is always expected to be supported because is the only
        // I/O library currently implemented
        io_uring_capabilities_kallsyms_ensure_iouring_available();
        REQUIRE(true);
    }

    SECTION("io_uring_capabilities_is_supported") {
        // Currently dummy test to expose problems, io_uring is always expected to be supported because is the only
        // I/O library currently implemented
        io_uring_capabilities_is_supported();
        REQUIRE(true);
    }
}
