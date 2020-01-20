#include <execinfo.h>
#include <unistd.h>

#include "backtrace.h"

void backtrace_print() {
    void *array[50];
    int size;

    size = backtrace(array, 50);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
}
