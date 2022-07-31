#ifndef CACHEGRAND_SENTRY_SUPPORT_H
#define CACHEGRAND_SENTRY_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

void sentry_support_shutdown();

void sentry_support_signal_sigsegv_handler(
        int signal_number);

void sentry_support_register_signal_sigsegv_handler();

void sentry_support_init(
        char* data_path,
        char* dsn);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SENTRY_SUPPORT_H
