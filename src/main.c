#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "worker/worker.h"

#include "program.h"

int main() {
    return program_main();
}
