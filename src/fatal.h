#ifndef CACHEGRAND_FATAL_H
#define CACHEGRAND_FATAL_H

#ifdef __cplusplus
extern "C" {
#endif

#define FATAL(producer, ...) \
    { fatal(producer, __VA_ARGS__); }

void fatal(log_producer_t *producer, const char *message, ...);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FATAL_H
