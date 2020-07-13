#define _GNU_SOURCE

#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "utils_cpu.h"
#include "misc.h"
#include "log.h"

#include "thread.h"

LOG_PRODUCER_CREATE_DEFAULT("thread", thread)

long thread_current_get_id() {
    return syscall(SYS_gettid);
}

uint32_t thread_current_set_affinity(
        uint32_t thread_index) {
    int res;
    cpu_set_t cpuset;
    pthread_t thread;

    uint32_t logical_core_count = utils_cpu_count();
    uint32_t logical_core_index = (thread_index % logical_core_count) * 2;

    if (logical_core_index >= logical_core_count) {
        logical_core_index = logical_core_index - logical_core_count + 1;
    }

    CPU_ZERO(&cpuset);
    CPU_SET(logical_core_index, &cpuset);

    thread = pthread_self();
    res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        LOG_E(
                LOG_PRODUCER_DEFAULT,
                "Unable to set current thread <%u> affinity to core <%u>",
                thread_current_get_id(),
                logical_core_index);
        LOG_E_OS_ERROR(LOG_PRODUCER_DEFAULT);
        logical_core_index = 0;
    }

    return logical_core_index;
}
