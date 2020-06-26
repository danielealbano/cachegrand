#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>

#include "log.h"
#include "fatal.h"
#include "random.h"


int main()
{
    log_producer_t* TAG = init_log_producer("main");
    LOG_D(TAG, "Debug log message: %d", 999);
    LOG_V(TAG, "Verbose log message: %d", 888);
    LOG_I(TAG, "Info log message: %d", 777);


    LOG_D(TAG, "Debug log message: %d", 666);
    LOG_V(TAG, "Verbose log message: %d", 555);
    LOG_I(TAG, "Info log message: %d", 444);
    LOG_W(TAG, "Warning log message: %d", 333);
    LOG_R(TAG, "Recoverable log message: %d", 222);
    LOG_E(TAG, "Error log message: %d", 111);

    FATAL(TAG, "KAbooommmmm %d", 000);
    free(TAG);
    return 0;
}
