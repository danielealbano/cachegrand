#ifndef CACHEGRAND_LOG_SINK_SUPPORT_H
#define CACHEGRAND_LOG_SINK_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

size_t log_sink_support_printer_str_len(
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        va_list args);

size_t log_sink_support_printer_str(
        char* message_out,
        size_t message_out_len,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        va_list args);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_LOG_SINK_SUPPORT_H
