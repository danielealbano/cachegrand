#ifndef CACHEGRAND_LOG_SINK_SUPPORT_H
#define CACHEGRAND_LOG_SINK_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

size_t log_sink_support_printer_str_len(
        const char* tag,
        const char* early_prefix_thread,
        size_t message_len);

size_t log_sink_support_printer_str(
        char* message_out,
        size_t message_out_len,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        const char* early_prefix_thread,
        const char* message,
        size_t message_len);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_LOG_SINK_SUPPORT_H
