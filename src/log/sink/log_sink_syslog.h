#ifndef CACHEGRAND_LOG_SINK_SYSLOG_H
#define CACHEGRAND_LOG_SINK_SYSLOG_H

#ifdef __cplusplus
extern "C" {
#endif

log_sink_t *log_sink_syslog_init(
        log_level_t levels,
        log_sink_settings_t* settings);

void log_sink_syslog_free(
        log_sink_settings_t* settings);

void log_sink_syslog_printer(
        log_sink_settings_t* settings,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        size_t message_len);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_LOG_SINK_SYSLOG_H
