#ifndef CACHEGRAND_FATAL_H
#define CACHEGRAND_FATAL_H

#ifdef __cplusplus
extern "C" {
#endif

#define FATAL(tag, ...) fatal(tag, __VA_ARGS__)

void fatal(
        const char *tag,
        const char *message,
        ...) __attribute__ ((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FATAL_H
