#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sentry.h>
#include <sys/stat.h>
#include <unistd.h>

#include "exttypes.h"
#include "log/log.h"
#include "xalloc.h"
#include "signals_support.h"

#include "sentry_support.h"

#define SENTRYIO_DATA_PATH_SUFFIX "sentryio-db"
#define TAG "sentry_support"

static char release_str[200] = { 0 };
static char* internal_data_path = NULL;

void sentry_support_shutdown() {
    sentry_close();
    xalloc_free(internal_data_path);
}

void sentry_support_signal_sigsegv_handler(
        int signal_number) {
    // cachegrand crashed, let's give a few second to sentry.io to process and, if the dsn is set, upload the minidump
    sleep(5);
    exit(-1);
}

void sentry_support_register_signal_sigsegv_handler() {
    signals_support_register_signal_handler(
            SIGSEGV,
            sentry_support_signal_sigsegv_handler,
            NULL);
}

void sentry_support_init(
        char* data_path,
        char* dsn) {
    if (data_path == NULL) {
        // Set the data path if it hasn't been set in the config
        char data_path_temp[] = "/tmp/" CACHEGRAND_CMAKE_CONFIG_NAME;
        mkdir(data_path_temp, 0700);
        data_path = data_path_temp;
    }

    sentry_support_register_signal_sigsegv_handler();

    // Append the data path suffix at the end
    size_t needed_size = snprintf(NULL, 0, "%s/%s", data_path, SENTRYIO_DATA_PATH_SUFFIX) + 1;
    internal_data_path = xalloc_alloc(needed_size + 1);
    snprintf(internal_data_path, needed_size, "%s/%s", data_path, SENTRYIO_DATA_PATH_SUFFIX);

    // Create the folder (it ignore failures & don't check if it already exists on purpose)
    mkdir(internal_data_path, 0700);

    // Initialize the release_str "cachegrand vVERSION (built on TIMESTAMP)"
    snprintf(
            release_str,
            sizeof(release_str),
            "%s@%s",
            CACHEGRAND_CMAKE_CONFIG_NAME,
            CACHEGRAND_CMAKE_CONFIG_VERSION_GIT);

    LOG_I(TAG, "Configuring sentry.io");
    LOG_I(TAG, "> release: %s", release_str);
    LOG_I(TAG, "> dsn: %s", dsn == NULL ? "<not set>" : dsn);
    LOG_I(TAG, "> data path: %s", internal_data_path);

    sentry_options_t *options = sentry_options_new();

    sentry_options_set_symbolize_stacktraces(
            options,
            true);
    sentry_options_set_release(
            options,
            release_str);
    sentry_options_set_dsn(
            options,
            dsn);
    sentry_options_set_database_path(
            options,
            internal_data_path);

    // TODO: Should track the version of the dependencies, the architecture of the machine, etc...

#if DEBUG == 1
    sentry_options_set_environment(
            options,
            "dev-build");
    sentry_options_set_debug(
            options,
            1);
#else
    sentry_options_set_environment(
            options,
            "release-build");
#endif

    sentry_init(options);

    LOG_I(TAG, "sentry.io configured");
}
