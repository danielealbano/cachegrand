#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sentry.h>

#include "exttypes.h"
#include "spinlock.h"
#include "log/log.h"
#include "data_structures/hashtable/mcmp/hashtable.h"
#include "config.h"
#include "worker/worker.h"
#include "program.h"
#include "xalloc.h"

#include "sentry_support.h"

#define SENTRYIO_DATA_PATH_SUFFIX "sentryio-db"
#define TAG "sentry_support"

static char release_str[200] = { 0 };

static char* internal_data_path = NULL;

void sentry_support_shutdown() {
    sentry_close();
    xalloc_free(internal_data_path);
}

void sentry_support_init(
        char* data_path,
        char* dsn) {
    if (data_path == NULL) {
        // Set the data path if it hasn't been set in the config
        char template[] = "/tmp/tmpdir.XXXXXX";
        data_path = mkdtemp(template);
    }

    // Append the data path suffix at the end
    size_t needed_size = snprintf(NULL, 0, "%s/%s", data_path, SENTRYIO_DATA_PATH_SUFFIX);
    internal_data_path = xalloc_alloc(needed_size + 1);
    snprintf(internal_data_path, needed_size, "%s/%s", data_path, SENTRYIO_DATA_PATH_SUFFIX);

    // Initialize the release_str "cachegrand vVERSION (built on TIMESTAMP)"
    snprintf(
            release_str,
            sizeof(release_str),
            "%s@%s (built on %s)",
            PROGRAM_NAME,
            PROGRAM_VERSION,
            PROGRAM_BUILD_DATE_TIME);

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
            data_path);

#if DEBUG == 1
    sentry_options_set_debug(
            options,
            1);
#endif

    sentry_init(options);

    LOG_I(TAG, "sentry.io configured");
}
