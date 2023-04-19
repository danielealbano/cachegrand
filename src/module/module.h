#ifndef CACHEGRAND_MODULE_H
#define CACHEGRAND_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

enum module_types {
    MODULE_TYPE_UNKNOWN = 0,
    MODULE_TYPE_REDIS,
    MODULE_TYPE_PROMETHEUS
};
typedef enum module_types module_types_t;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
static char *module_types_text[] = {
        "Unknown",
        "Redis",
        "Prometheus",
};
#pragma GCC diagnostic pop

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_MODULE_H
