#ifndef CACHEGRAND_CONFIG_CYAML_CONFIG_H
#define CACHEGRAND_CONFIG_CYAML_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

extern void config_internal_cyaml_log(
        cyaml_log_t level_cyaml,
        __attribute__((unused)) void *ctx,
        const char *fmt,
        va_list args);

extern void* config_internal_cyaml_malloc(
        void *ctx,
        void *ptr,
        size_t size);

extern cyaml_err_t config_internal_cyaml_load(
        config_t** config,
        char* config_path,
        cyaml_config_t* cyaml_config,
        cyaml_schema_value_t* schema);

extern void config_internal_cyaml_free(
        config_t* config,
        cyaml_config_t* cyaml_config,
        cyaml_schema_value_t* schema);

cyaml_config_t* config_cyaml_config_get_global();

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_CONFIG_CYAML_CONFIG_H
