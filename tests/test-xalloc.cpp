#include "catch.hpp"

#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>

#include "signals_support.h"
#include "xalloc.h"

sigjmp_buf jump_fp_xalloc;
void test_xalloc_signal_sigabrt_handler_longjmp(int signal_number) {
    siglongjmp(jump_fp_xalloc, 1);
}

void test_xalloc_setup_sigabrt_signal_handler() {
    signals_support_register_signal_handler(
            SIGABRT,
            test_xalloc_signal_sigabrt_handler_longjmp,
            NULL);
}

TEST_CASE("xalloc.c", "[xalloc]") {
    SECTION("xalloc_alloc") {
        SECTION("valid size") {
            char *data = (char*)xalloc_alloc(16);

            REQUIRE(data != NULL);

            free(data);
        }
        SECTION("invalid size") {
            char *data = NULL;
            bool fatal_catched = false;

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (char*)xalloc_alloc(-1);
            } else {
                fatal_catched = true;
            }

            REQUIRE(fatal_catched);
        }
    }

    SECTION("xalloc_free") {
        char *data = (char*)xalloc_alloc(16);

        REQUIRE(data != NULL);

        xalloc_free(data);
    }

    SECTION("xalloc_alloc_zero") {
        SECTION("valid size") {
            char *data = (char*)xalloc_alloc_zero(16);

            REQUIRE(data != NULL);
            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(data[i] == 0);
            }

            free(data);
        }
        SECTION("invalid size") {
            char *data = NULL;
            bool fatal_catched = false;

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (char*)xalloc_alloc_zero(-1);
            } else {
                fatal_catched = true;
            }

            REQUIRE(fatal_catched);
        }
    }

    SECTION("xalloc_alloc_aligned") {
        SECTION("valid size") {
            uintptr_t data = (uintptr_t)xalloc_alloc_aligned(64, 16);

            REQUIRE(data != 0);
            REQUIRE(data % 64 == 0);

            free((void*)data);
        }
        SECTION("invalid size") {
            uintptr_t data = 0;
            bool fatal_catched = false;

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (uintptr_t)xalloc_alloc_aligned(-1, 64);
            } else {
                fatal_catched = true;
            }

            REQUIRE(fatal_catched);
        }
    }

    SECTION("xalloc_alloc_aligned_zero") {
        SECTION("valid size") {
            uintptr_t data = (uintptr_t)xalloc_alloc_aligned_zero(64, 16);

            REQUIRE(data != 0);
            REQUIRE(data % 64 == 0);
            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(((char*)data)[i] == 0);
            }

            free((void*)data);
        }
        SECTION("invalid size") {
            uintptr_t data = 0;
            bool fatal_catched = false;

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (uintptr_t)xalloc_alloc_aligned_zero(-1, 64);
            } else {
                fatal_catched = true;
            }

            REQUIRE(fatal_catched);
        }
    }

    SECTION("xalloc_mmap_align_size") {
        size_t size = 64;
        long alignment = sysconf(_SC_PAGESIZE);
        size_t test_size = size - (size % alignment) + alignment;
        size_t aligned_size = xalloc_mmap_align_size(size);

        REQUIRE(aligned_size % alignment == 0);
        REQUIRE(aligned_size == test_size);
    }

    SECTION("xalloc_mmap_alloc") {
        SECTION("valid size") {
            size_t size = 64;
            long alignment = sysconf(_SC_PAGESIZE);
            uintptr_t data = (uintptr_t)xalloc_mmap_alloc(size);

            REQUIRE(data != 0);
            REQUIRE(data % alignment == 0);
            for(uint8_t i = 0; i < size; i++) {
                REQUIRE(((char*)data)[i] == 0);
            }

            REQUIRE(munmap((void*)data, xalloc_mmap_align_size(size)) == 0);
        }
        SECTION("invalid size") {
            uintptr_t data = 0;
            bool fatal_catched = false;

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (uintptr_t)xalloc_mmap_alloc(-1);
            } else {
                fatal_catched = true;
            }

            REQUIRE(fatal_catched);
        }
    }

    SECTION("xalloc_mmap_free") {
        size_t size = 64;
        long alignment = sysconf(_SC_PAGESIZE);
        uintptr_t data = (uintptr_t)xalloc_mmap_alloc(size);

        REQUIRE(data != 0);
        REQUIRE(data % alignment == 0);

        REQUIRE(xalloc_mmap_free((void*)data, size) == 0);
    }
}
