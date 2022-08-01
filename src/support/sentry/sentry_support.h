#ifndef CACHEGRAND_SENTRY_SUPPORT_H
#define CACHEGRAND_SENTRY_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define SENTRY_DSN "https://05dd54814d8149cab65ba2987d560340@o590814.ingest.sentry.io/5740234"

void sentry_support_shutdown();

void sentry_support_signal_sigsegv_handler(
        int signal_number);

void sentry_support_register_signal_sigsegv_handler();

void sentry_support_init(
        char* data_path);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SENTRY_SUPPORT_H
