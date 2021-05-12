#ifndef CACHEGRAND_SENTRY_SUPPORT_H
#define CACHEGRAND_SENTRY_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

void sentry_support_shutdown();

void sentry_support_init(
        char* data_path,
        char* dsn);

#endif //CACHEGRAND_SENTRY_SUPPORT_H
