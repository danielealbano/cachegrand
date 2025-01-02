/**
 * Copyright (C) 2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <liburing.h>

#include "misc.h"
#include "intrinsics.h"
#include "clock.h"
#include "xalloc.h"
#include "config.h"
#include "log/log.h"
#include "network/network_tls.h"

#include "program_startup_report.h"

#define TAG "program_startup_report"

void program_startup_report_build() {
    LOG_I(
            TAG,
            "%s version %s (built on %s)",
            CACHEGRAND_CMAKE_CONFIG_NAME,
            CACHEGRAND_CMAKE_CONFIG_VERSION_GIT,
            CACHEGRAND_CMAKE_CONFIG_BUILD_DATE_TIME);
    LOG_I(
            TAG,
            "> %s build, compiled using %s v%s",
            CACHEGRAND_CMAKE_CONFIG_BUILD_TYPE,
            CACHEGRAND_CMAKE_CONFIG_C_COMPILER_ID,
            CACHEGRAND_CMAKE_CONFIG_C_COMPILER_VERSION);
    LOG_I(
            TAG,
            "> Hashing algorithm in use %s",
            CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_T1HA2
            ? "t1ha2"
            : (CACHEGRAND_CMAKE_CONFIG_USE_HASH_ALGORITHM_XXH3
               ? "xxh3" :
               "crc32c"));
}

void program_startup_report_machine_uname() {
    struct utsname utsname;
    if (uname(&utsname) == 0) {
        LOG_I(
                TAG,
                "> Running on %s %s %s %s %s",
                utsname.sysname,
                utsname.nodename,
                utsname.release,
                utsname.version,
                utsname.machine);
    } else {
        LOG_W(
                TAG,
                "> Running on Failed to get the machine information");
    }
}

void program_startup_report_machine_memory() {
    struct sysinfo system_info;

    if (sysinfo(&system_info) == 0) {
        LOG_I(
                TAG,
                "> Memory: %ld MB total, %ld MB swap total",
                system_info.totalram / 1024 / 1024,
                system_info.totalswap / 1024 / 1024);
    } else {
        LOG_W(
                TAG,
                "> Memory: Failed to get the memory information");
    }
}

void program_startup_report_machine_liburing() {
    LOG_I(
            TAG,
            "> liburing: v%d.%d",
            io_uring_major_version(),
            io_uring_minor_version());
}

void program_startup_report_machine_tls() {
    LOG_I(
            TAG,
            "> TLS: %s (kernel offloading %s)",
            network_tls_mbedtls_version(),
            network_tls_is_ulp_tls_supported() ? "enabled" : "disabled");
    if (!network_tls_is_ulp_tls_supported()) {
        LOG_I(
                TAG,
                "       Try to load the tls kernel module with \"modprobe tls\", no need to restart %s",
                CACHEGRAND_CMAKE_CONFIG_NAME);
    }
}

void program_startup_report_machine_clock() {
    LOG_I(
            TAG,
            "> Realtime clock source <POSIX>, resolution <%ld ms>",
            clock_realtime_coarse_get_resolution_ms());
    LOG_I(
            TAG,
            "> Monotonic clock source <Hardware (TSC)> (%scpu cycles per second <%0.02lf GHz>), resolution <%ld ms>",
            intrinsics_frequency_max_is_estimated() ? "estimated " : "",
            (double)intrinsics_frequency_max() / 1000000000.0f,
            clock_monotonic_coarse_get_resolution_ms());
}

void program_startup_report() {
    program_startup_report_build();
    program_startup_report_machine_uname();
    program_startup_report_machine_memory();
    program_startup_report_machine_liburing();
    program_startup_report_machine_tls();
    program_startup_report_machine_clock();
}
