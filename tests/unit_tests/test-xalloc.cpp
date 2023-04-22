/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <csetjmp>
#include <csignal>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "hugepages.h"
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
            nullptr);
}

TEST_CASE("xalloc.c", "[xalloc]") {
    int internal_stderr = dup(STDERR_FILENO);
    int internal_nullfd = open("/dev/null", O_RDWR);

    SECTION("xalloc_alloc") {
        SECTION("valid size") {
            char *data = (char*)xalloc_alloc(16);

            REQUIRE(data != nullptr);

            xalloc_free(data);
        }
        SECTION("invalid size") {
            char *data;
            bool fatal_caught = false;

            dup2(internal_nullfd, STDERR_FILENO);

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (char*)xalloc_alloc(-1);
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);
        }
    }

    SECTION("xalloc_realloc") {
        SECTION("valid size") {
            char short_test_string[] = "short string";
            char *data = (char*)xalloc_alloc(16);
            memcpy(data, short_test_string, strlen(short_test_string) + 1);

            data = (char*)xalloc_realloc(data, 32);

            REQUIRE(data != nullptr);
            REQUIRE(strncmp(data, short_test_string, strlen(short_test_string)) == 0);

            xalloc_free(data);
        }
        SECTION("invalid size") {
            char *data = (char*)xalloc_alloc(16);
            bool fatal_caught = false;

            dup2(internal_nullfd, STDERR_FILENO);

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (char*)xalloc_realloc(data, -1);
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);

            xalloc_free(data);
        }
    }

    SECTION("xalloc_free") {
        char *data = (char*)xalloc_alloc(16);

        REQUIRE(data != nullptr);

        xalloc_free(data);
    }

    SECTION("xalloc_alloc_zero") {
        SECTION("valid size") {
            char *data = (char*)xalloc_alloc_zero(16);

            REQUIRE(data != nullptr);
            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(data[i] == 0);
            }

            xalloc_free(data);
        }
        SECTION("invalid size") {
            char *data = nullptr;
            bool fatal_caught = false;

            dup2(internal_nullfd, STDERR_FILENO);

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (char*)xalloc_alloc_zero(-1);
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);
        }
    }

    SECTION("xalloc_alloc_aligned") {
        SECTION("valid size") {
            auto data = (uintptr_t)xalloc_alloc_aligned(64, 16);

            REQUIRE(data != 0);
            REQUIRE(data % 64 == 0);

            xalloc_free((void*)data);
        }

        SECTION("invalid size") {
            uintptr_t data = 0;
            bool fatal_caught = false;

            dup2(internal_nullfd, STDERR_FILENO);

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (uintptr_t)xalloc_alloc_aligned(64, -1);
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);
        }

        SECTION("invalid alignment") {
            uintptr_t data = 0;
            bool fatal_caught = false;

            dup2(internal_nullfd, STDERR_FILENO);

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (uintptr_t)xalloc_alloc_aligned(-1, 16);
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);
        }
    }

    SECTION("xalloc_alloc_aligned_zero") {
        SECTION("valid size") {
            auto data = (uintptr_t)xalloc_alloc_aligned_zero(64, 16);

            REQUIRE(data != 0);
            REQUIRE(data % 64 == 0);
            for(uint8_t i = 0; i < 16; i++) {
                REQUIRE(((char*)data)[i] == 0);
            }

            xalloc_free((void*)data);
        }

        SECTION("invalid size") {
            uintptr_t data = 0;
            bool fatal_caught = false;

            dup2(internal_nullfd, STDERR_FILENO);

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (uintptr_t)xalloc_alloc_aligned_zero(64, -1);
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);
        }

        SECTION("invalid alignment") {
            uintptr_t data = 0;
            bool fatal_caught = false;

            dup2(internal_nullfd, STDERR_FILENO);

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (uintptr_t)xalloc_alloc_aligned_zero(-1, 16);
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);
        }
    }

    SECTION("xalloc_get_page_size") {
        long page_size_os = sysconf(_SC_PAGESIZE);
        long page_size_xalloc = xalloc_get_page_size();

        REQUIRE(page_size_os == page_size_xalloc);
    }

    SECTION("xalloc_mmap_align_addr") {
        void* memaddr = (void*)64;
        long alignment = sysconf(_SC_PAGESIZE);
        void* test_memaddr = (void*)((uintptr_t)memaddr - ((uintptr_t)memaddr % alignment) + alignment);
        void* aligned_memaddr = xalloc_mmap_align_addr(memaddr);

        REQUIRE((uintptr_t)aligned_memaddr % alignment == 0);
        REQUIRE(aligned_memaddr == test_memaddr);
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
            auto data = (uintptr_t)xalloc_mmap_alloc(size);

            REQUIRE(data != 0);
            REQUIRE(data % alignment == 0);
            for(uint8_t i = 0; i < size; i++) {
                REQUIRE(((char*)data)[i] == 0);
            }

            REQUIRE(munmap((void*)data, xalloc_mmap_align_size(size)) == 0);
        }
        SECTION("invalid size") {
            uintptr_t data = 0;
            bool fatal_caught = false;

            dup2(internal_nullfd, STDERR_FILENO);

            if (sigsetjmp(jump_fp_xalloc, 1) == 0) {
                test_xalloc_setup_sigabrt_signal_handler();
                data = (uintptr_t)xalloc_mmap_alloc(-1);
            } else {
                fatal_caught = true;
            }

            dup2(internal_stderr, STDERR_FILENO);

            REQUIRE(fatal_caught);
        }
    }

    SECTION("xalloc_mmap_free") {
        size_t size = 64;
        long alignment = sysconf(_SC_PAGESIZE);
        auto data = (uintptr_t)xalloc_mmap_alloc(size);

        REQUIRE(data != 0);
        REQUIRE(data % alignment == 0);

        REQUIRE(xalloc_mmap_free((void*)data, size) == 0);
    }

    SECTION("xalloc_hugepage_alloc") {
        SECTION("valid size") {
            if (hugepages_2mb_is_available(1)) {
                size_t hugepage_2mb_size = 2 * 1024 * 1024;
                auto data = (uintptr_t)xalloc_hugepage_alloc(hugepage_2mb_size);

                REQUIRE(data != 0);
                REQUIRE((data % (hugepage_2mb_size)) == 0);

                xalloc_hugepage_free((void *) data, hugepage_2mb_size);
            } else {
                WARN("Can't test hugepages support in xalloc, hugepages not enabled or not enough hugepages for testing");
            }
        }

        SECTION("invalid size") {
            REQUIRE(xalloc_hugepage_alloc(0) == nullptr);
        }
    }

    close(internal_stderr);
    close(internal_nullfd);
}
