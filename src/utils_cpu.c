#include <unistd.h>

#include "utils_cpu.h"

int utils_cpu_count() {
    static int count = 0;

#if defined(__linux__)
    if (count != 0) {
        return count;
    }

    count = sysconf(_SC_NPROCESSORS_ONLN);
#else
#error Platform not supported
#endif

    return count;
}

int utils_cpu_count_all() {
    static int count = 0;

#if defined(__linux__)
    if (count != 0) {
        return count;
    }

    count = sysconf(_SC_NPROCESSORS_CONF);
#else
#error Platform not supported
#endif

    return count;
}
