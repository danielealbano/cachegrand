#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>

#include "log.h"
#include "fatal.h"
#include "random.h"

LOG_PRODUCER_CREATE_LOCAL_DEFAULT("main", main)

int main()
{
    LOG_D(LOG_PRODUCER_DEFAULT, "Debug log message: %d", 999);
    LOG_V(LOG_PRODUCER_DEFAULT, "Verbose log message: %d", 888);
    LOG_I(LOG_PRODUCER_DEFAULT, "Info log message: %d", 777);


    LOG_D(LOG_PRODUCER_DEFAULT, "Debug log message: %d", 666);
    LOG_V(LOG_PRODUCER_DEFAULT, "Verbose log message: %d", 555);
    LOG_I(LOG_PRODUCER_DEFAULT, "Info log message: %d", 444);
    LOG_W(LOG_PRODUCER_DEFAULT, "Warning log message: %d", 333);
    LOG_R(LOG_PRODUCER_DEFAULT, "Recoverable log message: %d", 222);
    LOG_E(LOG_PRODUCER_DEFAULT, "Error log message: %d", 111);

    FATAL(LOG_PRODUCER_DEFAULT, "KAbooommmmm %d", 000);
    return 0;
}
