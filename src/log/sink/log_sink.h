#ifndef CACHEGRAND_LOG_SINK_H
#define CACHEGRAND_LOG_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct log_sink log_sink_t;

extern log_sink_t** log_sink_registered;
extern uint8_t log_sink_registered_count;

enum log_sink_type {
    LOG_SINK_TYPE_CONSOLE = 0,
    LOG_SINK_TYPE_FILE,
    LOG_SINK_TYPE_MAX
};
typedef enum log_sink_type log_sink_type_t;

typedef union log_sink_settings log_sink_settings_t;
union log_sink_settings {
    struct {
        bool use_stdout_for_errors;
    } console;
    struct {
        char* path;
        struct {
            int fd;
        } internal;
    } file;
};

typedef void (log_sink_printer_fn_t)(
        log_sink_settings_t* settings,
        const char* tag,
        time_t timestamp,
        log_level_t level,
        char* early_prefix_thread,
        const char* message,
        size_t message_len);

typedef void (log_sink_free_fn_t)(
        log_sink_settings_t* settings);

struct log_sink {
    log_sink_type_t type;
    log_level_t levels;
    log_sink_printer_fn_t* printer_fn;
    log_sink_free_fn_t* free_fn;
    log_sink_settings_t settings;
};

log_sink_t* log_sink_init(
        log_sink_type_t type,
        log_level_t levels,
        log_sink_settings_t* settings,
        log_sink_printer_fn_t printer_fn,
        log_sink_free_fn_t free_fn);

void log_sink_free(
        log_sink_t *log_sink);

bool log_sink_register(
        log_sink_t *sink);

log_sink_t *log_sink_factory(
        log_sink_type_t type,
        log_level_t levels,
        log_sink_settings_t* settings);

log_sink_t** log_sink_registered_get();

uint8_t log_sink_registered_count_get();

void log_sink_registered_free();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_LOG_SINK_H
