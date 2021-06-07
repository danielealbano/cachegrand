#ifndef CACHEGRAND_FATAL_H
#define CACHEGRAND_FATAL_H

#ifdef __cplusplus
extern "C" {
#endif

#define FATAL(tag, ...) fatal(tag, __VA_ARGS__)

__attribute__((noreturn))
__attribute__((format(printf, 2, 3)))
void fatal(
        const char *tag,
        const char *message,
        ...);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_FATAL_H
