#ifndef CACHEGRAND_FATAL_H
#define CACHEGRAND_FATAL_H

#include <log.h>
#ifdef __cplusplus
extern "C" {
#endif

#define FATAL(tag, message, ...) \
    { fatal(tag, message, __VA_ARGS__); }

void fatal(log_producer_t *tag, const char *message, ...);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FATAL_H
