#ifndef CACHEGRAND_SIGNAL_HANDLER_SIGSEGV_H
#define CACHEGRAND_SIGNAL_HANDLER_SIGSEGV_H

#ifdef __cplusplus
extern "C" {
#endif

void signal_handler_sigsegv_handler(int sig);
void signal_handler_sigsegv_init();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_SIGNAL_HANDLER_SIGSEGV_H
